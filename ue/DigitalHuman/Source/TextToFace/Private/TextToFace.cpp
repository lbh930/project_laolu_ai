// TextToFaceEngine.cpp  (RAW PCM16, chunked, non-blocking, proper session reset)
#include "TextToFace.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogMacros.h"
#include "Async/Async.h"

// ACE
#include "ACERuntimeModule.h"
#include "ACEAudioCurveSourceComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogTextToFace, Log, All);

// NOTE: header declares: FString XiApiKey, VoiceId, ModelId;
// void StartTTSRequest(const FString&, TWeakObjectPtr<AActor>);
// static bool AnimateWithACE(AActor*, const int16*, int32, int32, int32);

static FName GA2FProvider(TEXT("Default"));   // change if you use LocalA2F-* or RemoteA2F

void UTextToFaceEngine::SynthesizeAndAnimate(const FString& Text, AActor* TargetActor)
{
    if (XiApiKey.IsEmpty())    { UE_LOG(LogTextToFace, Error, TEXT("XI API key not set.")); return; }
    if (VoiceId.IsEmpty())     { UE_LOG(LogTextToFace, Error, TEXT("VoiceId not set."));   return; }
    if (!IsValid(TargetActor)) { UE_LOG(LogTextToFace, Error, TEXT("TargetActor invalid.")); return; }

    UE_LOG(LogTextToFace, Log, TEXT("SynthesizeAndAnimate Running"));

    // Optional: pre-allocate to reduce first-call latency
    FACERuntimeModule::Get().AllocateA2F3DResources(GA2FProvider);

    StartTTSRequest(Text, TargetActor);
}

void UTextToFaceEngine::StartTTSRequest(const FString& Text, TWeakObjectPtr<AActor> WeakTarget)
{
    const FString Url = FString::Printf(
        TEXT("https://api.elevenlabs.io/v1/text-to-speech/%s?output_format=pcm_16000"),
        *VoiceId);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("xi-api-key"), XiApiKey);
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetHeader(TEXT("Accept"), TEXT("audio/pcm")); // expect RAW PCM16

    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("text"), Text);
    Body->SetStringField(TEXT("model_id"), ModelId.IsEmpty() ? TEXT("eleven_multilingual_v2") : ModelId);

    FString BodyStr;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
    Req->SetContentAsString(BodyStr);

    UE_LOG(LogTextToFace, Log, TEXT("ElevenLabs TTS request: %s"), *Url);

    Req->OnProcessRequestComplete().BindLambda(
        [WeakTarget](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded)
        {
            AActor* Target = WeakTarget.Get();
            if (!Target) { UE_LOG(LogTextToFace, Warning, TEXT("TargetActor destroyed before response.")); return; }

            if (!bSucceeded || !Response.IsValid())
            {
                UE_LOG(LogTextToFace, Error, TEXT("HTTP failed or response invalid."));
                return;
            }

            const int32 Code = Response->GetResponseCode();
            if (Code < 200 || Code >= 300)
            {
                UE_LOG(LogTextToFace, Error, TEXT("HTTP %d: %s"), Code, *Response->GetContentAsString());
                return;
            }

            const TArray<uint8>& Bytes = Response->GetContent();
            if (Bytes.Num() <= 0 || (Bytes.Num() & 1) != 0)
            {
                const FString Preview = FString(UTF8_TO_TCHAR((const char*)Bytes.GetData())).Left(400);
                UE_LOG(LogTextToFace, Error, TEXT("Payload not PCM16 (size=%d). First 400 chars: %s"), Bytes.Num(), *Preview);
                return;
            }

            // Copy PCM into an owned array we can move into the async task
            const int32 NumSamples = Bytes.Num() / 2;
            TArray<int16> Pcm;
            Pcm.SetNumUninitialized(NumSamples);
            FMemory::Memcpy(Pcm.GetData(), Bytes.GetData(), Bytes.Num());

            const int32 SampleRate = 16000;
            const int32 Channels   = 1;

            UE_LOG(LogTextToFace, Log, TEXT("PCM16 ready: %d samples, %d Hz, %d ch (~%.2fs)"),
                   NumSamples, SampleRate, Channels, double(NumSamples)/double(SampleRate*Channels));

            // Run the ACE feeding on a background thread to avoid hitches
            // Chunk ~50ms @16kHz -> 800 samples per channel (mono = 800)
            const int32 Chunk = 800;

            // Keep strong references for async lambda
            TArray<int16> PcmCopy = MoveTemp(Pcm);
            TWeakObjectPtr<AActor> LocalWeakTarget(Target);

            Async(EAsyncExecution::ThreadPool, [PcmCopy = MoveTemp(PcmCopy), LocalWeakTarget, SampleRate, Channels, Chunk]()
            {
                AActor* TargetActor = LocalWeakTarget.Get();
                if (!TargetActor)
                {
                    UE_LOG(LogTextToFace, Warning, TEXT("TargetActor gone before ACE feeding."));
                    return;
                }

                UACEAudioCurveSourceComponent* Consumer = TargetActor->FindComponentByClass<UACEAudioCurveSourceComponent>();
                if (!Consumer)
                {
                    UE_LOG(LogTextToFace, Error, TEXT("TargetActor missing UACEAudioCurveSourceComponent."));
                    return;
                }

                // Ensure previous session is closed (fixes: only first play works)
                FACERuntimeModule::Get().CancelAnimationGeneration(Consumer);
                FACERuntimeModule::Get().EndAudioSamples(Consumer);

                bool bAllOK = true;
                const int32 Num = PcmCopy.Num();
                for (int32 i = 0; i < Num; i += Chunk)
                {
                    const int32 ThisCount = FMath::Min(Chunk, Num - i);
                    const bool  bLast     = (i + ThisCount) >= Num;

                    const bool bOK = FACERuntimeModule::Get().AnimateFromAudioSamples(
                        Consumer,
                        TArrayView<const int16>(PcmCopy.GetData() + i, ThisCount),
                        Channels,
                        SampleRate,
                        /*bEndOfSamples*/ bLast,
                        TOptional<FAudio2FaceEmotion>(),
                        nullptr,
                        GA2FProvider
                    );

                    if (!bOK) { bAllOK = false; UE_LOG(LogTextToFace, Error, TEXT("ACE chunk failed at %d."), i); break; }

                    // Pace to ~real-time to avoid contention if needed (optional).
                    // FPlatformProcess::Sleep(0.050f);
                }

                if (bAllOK)
                {
                    UE_LOG(LogTextToFace, Log, TEXT("ACE feeding complete."));
                }
            });
        });

    Req->ProcessRequest();
}

bool UTextToFaceEngine::AnimateWithACE(AActor* TargetActor, const int16* Samples, int32 NumSamples, int32 SampleRate, int32 NumChannels)
{
    // Not used in chunking path, keep for compatibility if you call it elsewhere.
    if (!TargetActor) return false;
    UACEAudioCurveSourceComponent* Consumer = TargetActor->FindComponentByClass<UACEAudioCurveSourceComponent>();
    if (!Consumer)
    {
        UE_LOG(LogTextToFace, Error, TEXT("TargetActor missing UACEAudioCurveSourceComponent."));
        return false;
    }

    FACERuntimeModule::Get().CancelAnimationGeneration(Consumer);
    FACERuntimeModule::Get().EndAudioSamples(Consumer);

    return FACERuntimeModule::Get().AnimateFromAudioSamples(
        Consumer,
        TArrayView<const int16>(Samples, NumSamples),
        NumChannels,
        SampleRate,
        /*bEndOfSamples*/ true,
        TOptional<FAudio2FaceEmotion>(),
        nullptr,
        GA2FProvider
    );
}

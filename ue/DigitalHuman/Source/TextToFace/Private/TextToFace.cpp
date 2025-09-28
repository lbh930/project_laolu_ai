// TextToFaceEngine.cpp
#include "TextToFace.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "HAL/UnrealMemory.h"
#include "Logging/LogMacros.h"
#include "Async/Async.h"
#include "Containers/Ticker.h"  // ✅ FTSTicker
#include "Misc/Base64.h"

// ACE
#include "ACERuntimeModule.h"
#include "ACEAudioCurveSourceComponent.h"

// UE
#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogTextToFace, Log, All);

static FName GA2FProvider(TEXT("Default"));

// === 工具：在 Game Thread 正确创建并注册 ACE 组件（若不存在） ===
static UACEAudioCurveSourceComponent* EnsureACEConsumerOnGT(AActor* Actor)
{
    check(IsInGameThread());
    if (!IsValid(Actor)) return nullptr;

    if (UACEAudioCurveSourceComponent* Existing = Actor->FindComponentByClass<UACEAudioCurveSourceComponent>())
    {
        return Existing;
    }

    UACEAudioCurveSourceComponent* C =
        NewObject<UACEAudioCurveSourceComponent>(
            Actor,
            UACEAudioCurveSourceComponent::StaticClass(),
            TEXT("ACEAudioCurveSourceComponent"));

    if (!C) return nullptr;

    // 标准动态组件注册流程
    C->OnComponentCreated();
    Actor->AddInstanceComponent(C);
    C->RegisterComponent();
    return C;
}

void UTextToFaceEngine::SynthesizeAndAnimate(const FString& Text, AActor* TargetActor)
{
    // 保持不变：一次性合成并喂入
    if (XiApiKey.IsEmpty())    { UE_LOG(LogTextToFace, Error, TEXT("XI API key not set.")); return; }
    if (VoiceId.IsEmpty())     { UE_LOG(LogTextToFace, Error, TEXT("VoiceId not set."));   return; }
    if (!IsValid(TargetActor)) { UE_LOG(LogTextToFace, Error, TEXT("TargetActor invalid.")); return; }

    FACERuntimeModule::Get().AllocateA2F3DResources(GA2FProvider);

    // 确保组件存在
    AsyncTask(ENamedThreads::GameThread, [WeakTarget=TWeakObjectPtr<AActor>(TargetActor)]()
    {
        if (AActor* T = WeakTarget.Get())
        {
            EnsureACEConsumerOnGT(T);
        }
    });

    StartTTSRequest(Text, TargetActor); // 旧的非流式路径
}

// === 新：把文本加入引擎队列 ===
void UTextToFaceEngine::TextToFaceStreamAppend(const FString& Text, AActor* TargetActor)
{
    if (Text.IsEmpty() || !IsValid(TargetActor)) return;

    // 先确保资源与组件
    FACERuntimeModule::Get().AllocateA2F3DResources(GA2FProvider);
    AsyncTask(ENamedThreads::GameThread, [WeakTarget=TWeakObjectPtr<AActor>(TargetActor)]()
    {
        if (AActor* T = WeakTarget.Get())
        {
            EnsureACEConsumerOnGT(T); // 确保组件
        }
    });

    // 入队
    {
        FScopeLock _(&QueueMtx);
        UtterQueue.Add({Text, TWeakObjectPtr<AActor>(TargetActor)});
        UE_LOG(LogTextToFace, Verbose, TEXT("[Queue] appended. size=%d"), UtterQueue.Num());
    }
}

// === 新：仅在未播放时拉起消费 ===
void UTextToFaceEngine::StartTTSStreamIfStopped()
{
    FScopeLock _(&QueueMtx);
    if (bSpeaking) return; // 已在播
    StartNextLocked();
}

// === 私有：从队列取下一条并启动 ===
void UTextToFaceEngine::StartNextLocked()
{
    if (bSpeaking) return;
    if (UtterQueue.Num() == 0) return;

    const FUtterItem Item = UtterQueue[0];
    UtterQueue.RemoveAt(0);
    bSpeaking = true;

    UE_LOG(LogTextToFace, Verbose, TEXT("[Queue] start next. remain=%d"), UtterQueue.Num());

    StartTTSRequest(Item.Text, Item.Target); // 复用旧路径
}

static const TCHAR* HttpReqStatusToString(EHttpRequestStatus::Type S)
{
    switch (S)
    {
    case EHttpRequestStatus::NotStarted:             return TEXT("NotStarted");
    case EHttpRequestStatus::Processing:             return TEXT("Processing");
    case EHttpRequestStatus::Failed:                 return TEXT("Failed");
    case EHttpRequestStatus::Succeeded:              return TEXT("Succeeded");
    default:                                         return TEXT("Unknown");
    }
}

static void LogHttpFailure(const FString& Context,
                           FHttpRequestPtr Request,
                           FHttpResponsePtr Response,
                           bool bSucceeded)
{
    const TCHAR* StatusStr = TEXT("RequestPtrInvalid");
    if (Request.IsValid())
    {
        StatusStr = HttpReqStatusToString(Request->GetStatus());
    }

    int32 Code = -1;
    if (Response.IsValid())
    {
        Code = Response->GetResponseCode();
    }

    UE_LOG(LogTextToFace, Error,
        TEXT("[%s] HTTP fail: bSucceeded=%d, RespValid=%d, ReqStatus=%s, HttpCode=%d"),
        *Context,
        bSucceeded ? 1 : 0,
        Response.IsValid() ? 1 : 0,
        StatusStr,
        Code);

    if (Response.IsValid())
    {
        const FString ErrBody = Response->GetContentAsString();
        UE_LOG(LogTextToFace, Error, TEXT("[%s] body(%d)='%s'"),
            *Context, ErrBody.Len(), *ErrBody.Left(256));
    }
}

// （可留存的旧非流式 StartTTSRequest / AnimateWithACE）
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
    Req->SetHeader(TEXT("Accept"), TEXT("audio/pcm")); // 期望 RAW PCM16

    // 构造 JSON
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("text"), Text);
    Body->SetStringField(TEXT("model_id"), ModelId.IsEmpty() ? TEXT("eleven_multilingual_v2") : ModelId);

    FString BodyStr;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
    FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
    Req->SetContentAsString(BodyStr);

    UE_LOG(LogTextToFace, Log, TEXT("ElevenLabs TTS request: %s"), *Url);

    // 关键：弱引用 self，避免回调时对象已被 GC
    TWeakObjectPtr<UTextToFaceEngine> WeakSelf(this);

    Req->OnProcessRequestComplete().BindLambda(
        [WeakSelf, WeakTarget](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded)
        {
            UTextToFaceEngine* Self = WeakSelf.Get();
            if (!IsValid(Self)) { return; } // self 已无效

            AActor* Target = WeakTarget.Get();
            if (!Target)
            {
                UE_LOG(LogTextToFace, Warning, TEXT("TargetActor destroyed before response."));
                {
                    FScopeLock _(&Self->QueueMtx);
                    Self->bSpeaking = false;
                    Self->StartNextLocked();
                }
                Self->OnTTSClipFinished.Broadcast();
                return;
            }

            if (!bSucceeded || !Response.IsValid())
            {
                LogHttpFailure(TEXT("TTS"), Request, Response, bSucceeded);

                {
                    FScopeLock _(&Self->QueueMtx);
                    Self->bSpeaking = false;
                    Self->StartNextLocked();
                }
                Self->OnTTSClipFinished.Broadcast();
                return;
            }


            const int32 Code = Response->GetResponseCode();
            if (Code < 200 || Code >= 300)
            {
                UE_LOG(LogTextToFace, Error, TEXT("HTTP %d: %s"), Code, *Response->GetContentAsString());
                {
                    FScopeLock _(&Self->QueueMtx);
                    Self->bSpeaking = false;
                    Self->StartNextLocked();
                }
                Self->OnTTSClipFinished.Broadcast();
                return;
            }

            const TArray<uint8>& Bytes = Response->GetContent();
            if (Bytes.Num() <= 0 || (Bytes.Num() & 1) != 0)
            {
                const FString Preview = FString(UTF8_TO_TCHAR((const char*)Bytes.GetData())).Left(400);
                UE_LOG(LogTextToFace, Error, TEXT("Payload not PCM16 (size=%d). First 400 chars: %s"), Bytes.Num(), *Preview);
                {
                    FScopeLock _(&Self->QueueMtx);
                    Self->bSpeaking = false;
                    Self->StartNextLocked();
                }
                Self->OnTTSClipFinished.Broadcast();
                return;
            }

            // 拷贝 PCM 到可移动数组
            const int32 NumSamples = Bytes.Num() / 2;
            TArray<int16> Pcm;
            Pcm.SetNumUninitialized(NumSamples);
            FMemory::Memcpy(Pcm.GetData(), Bytes.GetData(), Bytes.Num());

            const int32 SampleRate = 16000;
            const int32 Channels   = 1;

            UE_LOG(LogTextToFace, Log, TEXT("PCM16 ready: %d samples, %d Hz, %d ch (~%.2fs)"),
                   NumSamples, SampleRate, Channels, double(NumSamples)/double(SampleRate*Channels));

            // 在 GT 确保组件存在
            AsyncTask(ENamedThreads::GameThread, [WeakTarget]()
            {
                if (AActor* T = WeakTarget.Get())
                {
                    // 确保 ACE 组件
                    if (!T->FindComponentByClass<UACEAudioCurveSourceComponent>())
                    {
                        // 若无则创建
                        UACEAudioCurveSourceComponent* C =
                            NewObject<UACEAudioCurveSourceComponent>(
                                T,
                                UACEAudioCurveSourceComponent::StaticClass(),
                                TEXT("ACEAudioCurveSourceComponent"));
                        if (C)
                        {
                            C->OnComponentCreated();
                            T->AddInstanceComponent(C);
                            C->RegisterComponent();
                        }
                    }
                }
            });

            // 后台分块喂入
            const int32 Chunk = 800; // ~50ms @16kHz
            TArray<int16> PcmCopy = MoveTemp(Pcm);
            TWeakObjectPtr<AActor> LocalWeakTarget(Target);

            Async(EAsyncExecution::ThreadPool, [WeakSelf, PcmCopy = MoveTemp(PcmCopy), LocalWeakTarget, SampleRate, Channels, Chunk]()
            {
                UTextToFaceEngine* Self2 = WeakSelf.Get();
                if (!IsValid(Self2)) { return; } // self 再次校验

                AActor* TargetActor = LocalWeakTarget.Get();
                if (!TargetActor)
                {
                    UE_LOG(LogTextToFace, Warning, TEXT("TargetActor gone before ACE feeding."));
                    {
                        FScopeLock _(&Self2->QueueMtx);
                        Self2->bSpeaking = false;
                        Self2->StartNextLocked();
                    }
                    Self2->OnTTSClipFinished.Broadcast();
                    return;
                }

                UACEAudioCurveSourceComponent* Consumer = TargetActor->FindComponentByClass<UACEAudioCurveSourceComponent>();
                if (!Consumer)
                {
                    UE_LOG(LogTextToFace, Error, TEXT("TargetActor missing UACEAudioCurveSourceComponent."));
                    {
                        FScopeLock _(&Self2->QueueMtx);
                        Self2->bSpeaking = false;
                        Self2->StartNextLocked();
                    }
                    Self2->OnTTSClipFinished.Broadcast();
                    return;
                }

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

                    if (!bOK)
                    {
                        bAllOK = false;
                        UE_LOG(LogTextToFace, Error, TEXT("ACE chunk failed at %d."), i);
                        break;
                    }
                }

                if (bAllOK)
                {
                    UE_LOG(LogTextToFace, Log, TEXT("ACE feeding complete."));
                }

                // 本条完成，推进队列
                {
                    FScopeLock _(&Self2->QueueMtx);
                    Self2->bSpeaking = false;
                    Self2->StartNextLocked();
                }
                Self2->OnTTSClipFinished.Broadcast();
            });
        });

    Req->ProcessRequest();
}


// （保持接口以备需要）
bool UTextToFaceEngine::AnimateWithACE(AActor* TargetActor, const int16* Samples, int32 NumSamples, int32 SampleRate, int32 NumChannels)
{
    if (!TargetActor || !Samples || NumSamples <= 0) return false;
    if (UACEAudioCurveSourceComponent* C = TargetActor->FindComponentByClass<UACEAudioCurveSourceComponent>())
    {
        return FACERuntimeModule::Get().AnimateFromAudioSamples(
            C,
            TArrayView<const int16>(Samples, NumSamples),
            NumChannels,
            SampleRate,
            true,
            TOptional<FAudio2FaceEmotion>(),
            nullptr,
            GA2FProvider
        );
    }
    return false;
}

// TextToFaceEngine.cpp

bool UTextToFaceEngine::IsSpeaking() const
{
    FScopeLock _(&QueueMtx);
    return bSpeaking;
}

int32 UTextToFaceEngine::PendingUtterCount() const
{
    FScopeLock _(&QueueMtx);
    return UtterQueue.Num();
}


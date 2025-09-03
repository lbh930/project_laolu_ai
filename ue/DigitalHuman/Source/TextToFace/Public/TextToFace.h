// Source/TextToFace/Public/TextToFace/TextToFaceEngine.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TextToFace.generated.h"

/**
 * TTS (ElevenLabs) -> WAV(PCM16) -> ACE AnimateFromAudioSamples
 */
UCLASS(BlueprintType)
class TEXTTOFACE_API UTextToFaceEngine : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="TextToFace")
    void SetXiApiKey(const FString& InKey) { XiApiKey = InKey; }

    UFUNCTION(BlueprintCallable, Category="TextToFace")
    void SetVoiceId(const FString& InVoiceId) { VoiceId = InVoiceId; }

    UFUNCTION(BlueprintCallable, Category="TextToFace")
    void SetModelId(const FString& InModelId) { ModelId = InModelId; }

    UFUNCTION(BlueprintCallable, Category="TextToFace")
    void SynthesizeAndAnimate(const FString& Text, AActor* TargetActor);

private:
    FString XiApiKey;
    FString VoiceId = TEXT("JBFqnCBsd6RMkjVDRZzb");
    FString ModelId = TEXT("eleven_multilingual_v2");

    void StartTTSRequest(const FString& Text, TWeakObjectPtr<AActor> WeakTarget);

    static bool ExtractPcm16FromWav(const TArray<uint8>& WavBytes, TArray<int16>& OutPcm, int32& OutSampleRate, int32& OutNumChannels);
    static bool AnimateWithACE(AActor* TargetActor, const int16* Samples, int32 NumSamples, int32 SampleRate, int32 NumChannels);
};

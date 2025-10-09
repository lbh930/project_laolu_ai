// TextToFaceEngine.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TextToFace.generated.h"

/**
 * Streaming TTS (ElevenLabs) -> ACE AnimateFromAudioSamples
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTTSClipFinished);

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

    // Non-stream kept
    UFUNCTION(BlueprintCallable, Category="TextToFace")
    void SynthesizeAndAnimate(const FString& Text, AActor* TargetActor);

    // 新：Widget 把可说文本 append 到引擎队列
    UFUNCTION(BlueprintCallable, Category="TextToFace")
    void TextToFaceStreamAppend(const FString& Text, AActor* TargetActor);

    // 新：仅在未播放时拉起消费
    UFUNCTION(BlueprintCallable, Category="TextToFace")
    void StartTTSStreamIfStopped();

    // 单条完成时广播（外界可选监听）
    UPROPERTY(BlueprintAssignable, Category="TextToFace")
    FOnTTSClipFinished OnTTSClipFinished;

    // 是否正在播报TTS（线程安全只读）
    UFUNCTION(BlueprintCallable, Category="TextToFace")
    bool IsSpeaking() const;

    // 待播条数
    UFUNCTION(BlueprintCallable, Category="TextToFace")
    int32 PendingUtterCount() const;

private:
    FString XiApiKey;
    FString VoiceId = TEXT("JBFqnCBsd6RMkjVDRZzb");
    FString ModelId = TEXT("eleven_multilingual_v2");

    // 队列项
    struct FUtterItem
    {
        FString Text;
        TWeakObjectPtr<AActor> Target;
        int32 RetryCount = 0; // 重试次数
    };

    // 简单串行队列
    TArray<FUtterItem> UtterQueue; // 简单顺序数组队列
    mutable FCriticalSection QueueMtx;     // 保护队列
    bool bSpeaking = false;        // 正在消费中

    // 重试配置
    static constexpr int32 MaxRetries = 3;      // 最大重试次数
    static constexpr float TimeoutSeconds = 10.0f; // HTTP超时时间（秒）

private:
    void StartTTSRequest(const FString& Text, TWeakObjectPtr<AActor> WeakTarget, int32 RetryCount = 0); // 旧非流式，保留复用
    void StartNextLocked(); // 启动下一条（需已持锁）
    static bool AnimateWithACE(AActor* TargetActor, const int16* Samples, int32 NumSamples, int32 SampleRate, int32 NumChannels);
};

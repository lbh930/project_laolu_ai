// Deprecated
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Chatbot.h" // 声明动态委托用
#include "TextToFaceWidget.generated.h"

class UEditableTextBox;
class UButton;
class UTextToFaceEngine;

USTRUCT(BlueprintType)
struct FTextToFaceSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) bool bReady = false;     // 依赖是否就绪
	UPROPERTY(BlueprintReadOnly) bool bThinking = false; // LLM是否在流式生成
	UPROPERTY(BlueprintReadOnly) bool bSpeaking = false; // 估计是否在播放TextToFace
};

UCLASS()
class DIGITALHUMAN_API UTextToFaceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TextToFace")
	UTextToFaceEngine* EngineClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TextToFace")
	AActor* TargetActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chatbot")
	UChatbotClient* ChatbotClient = nullptr;

	UFUNCTION(BlueprintCallable, Category="TextToFace")
	FTextToFaceSnapshot GetRuntimeSnapshot() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta=(BindWidget)) UEditableTextBox* InputTextBox;
	UPROPERTY(meta=(BindWidget)) UButton* SendButton;

private:
	UFUNCTION() void OnSendClicked();
	
	// Legacy non-streaming callbacks
	UFUNCTION() void HandleChatResponse(const FString& AIResponse);
	UFUNCTION() void HandleChatError(const FString& Error);

	// Streaming callbacks（仅从 LLM 收流，交给引擎排队）
	UFUNCTION() void HandleChatDelta(const FString& Delta);
	UFUNCTION() void HandleChatDone(const FString& Full);

	// For inputs from PixelStreaming Web Frontend
	UFUNCTION()
	void HandleUserInputReceived(const FString& UserText);

	// Chunking / flush helpers
	void FlushBuffer(bool bForce);
	bool ReachedBoundary() const;
	AActor* ResolveTargetActor() const;

	// 纯文本缓冲（不再本地排队语音）
	FString StreamBuffer;
	FTimerHandle FlushTimerHandle;
	double LastFlushTimeSec = 0.0;
	
	UPROPERTY(EditAnywhere, Category="Streaming|Chunking")
	float MaxWaitSeconds = 0.35f;        // flush if no new text for this time

	UPROPERTY(EditAnywhere, Category="Streaming|Chunking")
	FString SentenceBoundaries = TEXT(".!?。！？\n"); // punctuation boundary

	UPROPERTY(EditAnywhere, Category="TextToFace", meta=(ClampMin="0.1", ClampMax="10.0"))
	float SpeakWindowSec = 3.0f;

	bool bStreamingInFlight = false;
};

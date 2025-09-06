// Deprecated
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Chatbot.h" // 声明动态委托用
#include "TextToFaceWidget.generated.h"

class UEditableTextBox;
class UButton;
class UTextToFaceEngine;

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

	bool bStreamingInFlight = false;
};

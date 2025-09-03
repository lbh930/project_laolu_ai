// TextToFaceWidget.h
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

	UPROPERTY(meta=(BindWidget)) UEditableTextBox* InputTextBox;
	UPROPERTY(meta=(BindWidget)) UButton* SendButton;

private:
	UFUNCTION() void OnSendClicked();

	// 回调处理函数（必须是 UFUNCTION）
	UFUNCTION() void HandleChatResponse(const FString& AIResponse);
	UFUNCTION() void HandleChatError(const FString& Error);
};

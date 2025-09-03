// TextToFaceWidget.cpp
#include "TextToFaceWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "TextToFace.h"

void UTextToFaceWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!EngineClass)
	{
		EngineClass = NewObject<UTextToFaceEngine>(this);
		EngineClass->SetXiApiKey(TEXT("sk_e30762316d0e7a4a1a7d5937dc468e8d72fde1b13b37e325"));
		EngineClass->SetVoiceId (TEXT("Q26iPuGVPnOfNa3FzCH6"));
		EngineClass->SetModelId (TEXT("eleven_v3"));
	}

	if (!ChatbotClient)
	{
		ChatbotClient = NewObject<UChatbotClient>(this);
		ChatbotClient->ApiKey = TEXT("sk-dd53e4e7cff84456bd554e018dbe2b8c");
	}

	if (SendButton)
	{
		SendButton->OnClicked.AddDynamic(this, &UTextToFaceWidget::OnSendClicked);
	}
}

void UTextToFaceWidget::OnSendClicked()
{
	if (!InputTextBox || !ChatbotClient || !EngineClass) return;

	const FString UserText = InputTextBox->GetText().ToString();
	if (UserText.IsEmpty()) return;

	TArray<FString> Roles, Contents;
	Roles.Add(TEXT("system")); Contents.Add(TEXT("You are a helpful assistant."));
	Roles.Add(TEXT("user"));   Contents.Add(UserText);

	// 绑定动态委托到 UFUNCTION（重点）
	FOnChatResponse OnOk;
	OnOk.BindUFunction(this, FName("HandleChatResponse"));

	FOnChatError OnErr;
	OnErr.BindUFunction(this, FName("HandleChatError"));

	ChatbotClient->SendChat(Roles, Contents, /*Temperature*/1.3f, OnOk, OnErr);
}

void UTextToFaceWidget::HandleChatResponse(const FString& AIResponse)
{
	AActor* UseTarget = TargetActor ? TargetActor : GetOwningPlayerPawn();
	EngineClass->SynthesizeAndAnimate(AIResponse, UseTarget);
}

void UTextToFaceWidget::HandleChatError(const FString& Error)
{
	UE_LOG(LogTemp, Error, TEXT("Chatbot Error: %s"), *Error);
}


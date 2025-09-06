// Chatbot.h
#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Chatbot.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnChatResponse, const FString&, Text);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnChatError, const FString&, Error);
// New: streaming token delta
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnChatDelta, const FString&, Delta);

UCLASS(BlueprintType, Config=Game)
class CHATBOT_API UChatbotClient : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category="Chatbot")
	FString ApiKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Config, Category="Chatbot")
	FString BaseUrl = TEXT("https://api.deepseek.com");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chatbot")
	FString Model = TEXT("deepseek-chat");

	UFUNCTION(BlueprintCallable, Category="Chatbot")
	void SendChat(const TArray<FString>& Roles, const TArray<FString>& Contents,
				  float Temperature,
				  const FOnChatResponse& OnOk,
				  const FOnChatError& OnFail);

	// New: streaming SSE (polling-based)
	UFUNCTION(BlueprintCallable, Category="Chatbot")
	void SendChatStream(const TArray<FString>& Roles, const TArray<FString>& Contents,
						float Temperature,
						const FOnChatDelta& OnDelta,
						const FOnChatResponse& OnDone,
						const FOnChatError& OnFail);
};

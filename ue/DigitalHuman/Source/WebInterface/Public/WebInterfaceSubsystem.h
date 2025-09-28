// WebInterfaceSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WebInterfaceSubsystem.generated.h"

/** 收到用户文本输入（前端发来的 chat 文本） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUserInputReceived, const FString&, Text);
/** 收到原始 UI 消息（原样 JSON 字符串，便于调试或扩展） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRawMessage, const FString&, JsonString);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSendPS2Response, const FString&, Payload);

UCLASS()
class WEBINTERFACE_API UWebInterfaceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "WebInterface", meta=(WorldContext="WorldContextObject"))
	static UWebInterfaceSubsystem* Get(UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 外部把 UIInteraction / 任意字符串喂进来 */
	UFUNCTION(BlueprintCallable, Category = "WebInterface")
	void ReceiveUIMessage(const FString& JsonOrText);

	UPROPERTY(BlueprintAssignable, Category = "WebInterface")
	FOnUserInputReceived OnUserInputReceived;

	UPROPERTY(BlueprintAssignable, Category = "WebInterface")
	FOnRawMessage OnRawMessage;

private:
	/** 解析 {"type":"chat","text":"..."}；成功返回 true 并输出 Text */
	bool TryParseChatText(const FString& JsonString, FString& OutText) const;

	/** 处理 {"type":"call", ...} 请求；返回是否已处理（无论成功与否都会回响应） */
	void HandleCallRequest(const TSharedRef<FJsonObject>& Root);

	UPROPERTY(BlueprintAssignable, Category="WebInterface")
	FOnSendPS2Response OnSendPS2Response;

	UFUNCTION(BlueprintCallable, Category = "WebInterface")
	void SendPS2Response(bool bOk, const FString& RequestId, const FString& ErrorMsg, TOptional<bool> BoolResult);

	/** 工具：按 tag/name 定位 Actor */
	class AActor* FindTargetActor(const FString& By, const FString& Value) const;
	
	/** 工具：反射调用 UFUNCTION（可有返回值）并把返回值塞进 ResultOut（仅支持基础返回，示例覆盖 bool） */
	bool InvokeUFunctionReturn(UObject* Target, const FString& MethodName, TSharedPtr<class FJsonValue>& ResultOut, FString& ErrorOut) const;
};

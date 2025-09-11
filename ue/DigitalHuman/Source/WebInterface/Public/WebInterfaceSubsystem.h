// WebInterfaceSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WebInterfaceSubsystem.generated.h"

/** 收到用户文本输入（前端发来的 chat 文本） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUserInputReceived, const FString&, Text);

/** 收到原始 UI 消息（原样 JSON 字符串，便于调试或扩展） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRawMessage, const FString&, JsonString);

/**
 * 最小实现：负责接收来自 Web 的 UIInteraction 字符串（通常由 Pixel Streaming Input 蓝图事件提供），
 * 解析简单 JSON，并对外广播统一事件，供 TextToFaceWidget/Module 绑定使用。
 */
UCLASS()
class WEBINTERFACE_API UWebInterfaceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** 获取当前世界下的 Subsystem 便捷方法 */
	UFUNCTION(BlueprintCallable, Category = "WebInterface", meta=(WorldContext="WorldContextObject"))
	static UWebInterfaceSubsystem* Get(UObject* WorldContextObject);

	/** 初始化 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 反初始化 */
	virtual void Deinitialize() override;

	/**
	 * 供外部调用：把来自 PixelStreaming 的 UIInteraction/任意 JSON 字符串传进来。
	 * 解析到 {"type":"chat","text":"..."} 时会广播 OnUserInputReceived。
	 * 不解析失败会广播 OnRawMessage 便于上层自行处理。
	 */
	UFUNCTION(BlueprintCallable, Category = "WebInterface")
	void ReceiveUIMessage(const FString& JsonOrText);

	/** 上层可绑定：收到用户文本输入时回调 */
	UPROPERTY(BlueprintAssignable, Category = "WebInterface")
	FOnUserInputReceived OnUserInputReceived;

	/** 上层可绑定：收到任意原始 JSON 字符串（包含无法解析的情况） */
	UPROPERTY(BlueprintAssignable, Category = "WebInterface")
	FOnRawMessage OnRawMessage;

private:
	/** 解析 {"type":"chat","text":"..."}；成功返回 true 并输出 Text */
	bool TryParseChatText(const FString& JsonString, FString& OutText) const;
};

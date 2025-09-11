// WebInterfaceSubsystem.cpp
#include "WebInterfaceSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "JsonObjectConverter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogWebInterfaceSubsystem, Log, All);

UWebInterfaceSubsystem* UWebInterfaceSubsystem::Get(UObject* WorldContextObject)
{
    if (!WorldContextObject) return nullptr;

    UGameInstance* GI = nullptr;

    if (const UWorld* World = WorldContextObject->GetWorld())
    {
        GI = World->GetGameInstance();
    }
    else
    {
        GI = GEngine ? GEngine->GetWorldFromContextObjectChecked(WorldContextObject)->GetGameInstance() : nullptr;
    }

    return GI ? GI->GetSubsystem<UWebInterfaceSubsystem>() : nullptr;
}

void UWebInterfaceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogWebInterfaceSubsystem, Log, TEXT("UWebInterfaceSubsystem::Initialize"));
}

void UWebInterfaceSubsystem::Deinitialize()
{
    UE_LOG(LogWebInterfaceSubsystem, Log, TEXT("UWebInterfaceSubsystem::Deinitialize"));
    Super::Deinitialize();
}

void UWebInterfaceSubsystem::ReceiveUIMessage(const FString& JsonOrText)
{
    UE_LOG(LogWebInterfaceSubsystem, Verbose, TEXT("ReceiveUIMessage: %s"), *JsonOrText);

    // 广播原始消息，方便上层调试或做自定义扩展
    OnRawMessage.Broadcast(JsonOrText);

    // 兼容两种情况：1) 直接纯文本；2) JSON 包含 type/text 字段
    FString ChatText;
    if (TryParseChatText(JsonOrText, ChatText))
    {
        if (!ChatText.IsEmpty())
        {
            UE_LOG(LogWebInterfaceSubsystem, Display, TEXT("Parsed chat text: %s"), *ChatText);
            OnUserInputReceived.Broadcast(ChatText);
            return;
        }
    }
    else
    {
        // 如果不是 JSON 或解析失败，则把整个字符串当作文本（最小可用策略）
        // 这样前端即使直接发纯文本也能通
        const FString AsText = JsonOrText;
        if (!AsText.IsEmpty())
        {
            UE_LOG(LogWebInterfaceSubsystem, Display, TEXT("Treat as plain text: %s"), *AsText);
            OnUserInputReceived.Broadcast(AsText);
            return;
        }
    }

    // 空或无法处理就静默
}

bool UWebInterfaceSubsystem::TryParseChatText(const FString& JsonString, FString& OutText) const
{
    OutText.Reset();

    // 粗略判断是否像 JSON
    if (!JsonString.StartsWith(TEXT("{")) && !JsonString.StartsWith(TEXT("[")))
    {
        return false;
    }

    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    TSharedPtr<FJsonObject> RootObj;
    if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
    {
        UE_LOG(LogWebInterfaceSubsystem, Verbose, TEXT("JSON deserialize failed."));
        return false;
    }

    // 期望 {"type":"chat","text":"..."}
    FString Type;
    if (!RootObj->TryGetStringField(TEXT("type"), Type))
    {
        return false;
    }
    if (!Type.Equals(TEXT("chat"), ESearchCase::IgnoreCase))
    {
        // 未来你可以在这里支持更多 type
        return false;
    }

    FString Text;
    if (!RootObj->TryGetStringField(TEXT("text"), Text))
    {
        return false;
    }

    Text.TrimStartAndEndInline();
    OutText = MoveTemp(Text);
    return true;
}


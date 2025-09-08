#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PromptGenerator.generated.h"

// few_shots 的 (user, assistant) 成对样例
USTRUCT(BlueprintType)                            // 让该结构对蓝图可见
struct PROMPTGENERATOR_API FPGFewShot             // 导出宏，跨模块安全
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Prompt|FewShot")
    FString User;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Prompt|FewShot")
    FString Assistant;
};

UCLASS(BlueprintType)
class PROMPTGENERATOR_API UPromptGenerator : public UObject
{
    GENERATED_BODY()

public:
    // === 从 YAML 读取的配置 ===
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prompt")
    FString Persona;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prompt")
    FString Style;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prompt")
    FString Constraints;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prompt")
    FString OutputFormat;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prompt")
    bool TTSFriendly = true; // 为流式 TTS 分句优化

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prompt")
    TArray<FString> Facts;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prompt")
    TArray<FString> Stop;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prompt")
    TArray<FPGFewShot> FewShots;

public:
    // 读取 YAML（最小实现，支持 key:value、key: | 多行、列表、few_shots 列表对象）
    UFUNCTION(BlueprintCallable, Category="Prompt")
    bool LoadFromYaml(const FString& AbsPath);

    // 生成 messages：system（含 memory/facts & tts 规则）+ few_shots + 真实对话
    void BuildMessages(const TArray<FString>& Roles,
                       const TArray<FString>& Contents,
                       TArray<TSharedPtr<FJsonValue>>& OutMsgs) const;

    // （可选）把 stop 写进请求体
    void MaybeAttachStop(TSharedPtr<FJsonObject> Root) const;

private:
    static TSharedPtr<FJsonObject> MakeMsg(const FString& Role, const FString& Content);
    FString BuildSystemPrompt() const;

    // 解析器（YAML 子集）
    bool ParseYaml(const FString& Text);

    // 小工具
    static FString StripQuotes(const FString& S);
    static int32   LeadingSpaces(const FString& S);
};

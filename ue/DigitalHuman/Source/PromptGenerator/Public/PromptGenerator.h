#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PromptGenerator.generated.h"

// 少样例 (user, assistant)
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
    // === 配置字段（从 YAML 读取） ===
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prompt")
    FString Persona;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prompt")
    FString Style;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prompt")
    FString Constraints;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prompt")
    FString OutputFormat;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prompt")
    bool TTSFriendly = true; // 为分句优化

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prompt")
    TArray<FString> Facts;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prompt")
    TArray<FString> Stop; // 可选：模型支持就附上

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prompt")
    TArray<FPGFewShot> FewShots;

public:
    // 读取 YAML（最小子集语法）
    UFUNCTION(BlueprintCallable, Category="Prompt")
    bool LoadFromYaml(const FString& AbsPath);

    // 生成 messages（system + few-shots + 传入消息）
    void BuildMessages(const TArray<FString>& Roles,
                       const TArray<FString>& Contents,
                       TArray<TSharedPtr<FJsonValue>>& OutMsgs) const;

    // 可选：把 stop 写进 body（如果你的请求体支持）
    void MaybeAttachStop(TSharedPtr<FJsonObject> Root) const;

private:
    static TSharedPtr<FJsonObject> MakeMsg(const FString& Role, const FString& Content);
    FString BuildSystemPrompt() const;

    // 极简 YAML 解析：支持
    // key: value
    // key:        （进入列表/块）
    //   - item
    // few_shots:
    //   - user: ...
    //     assistant: ...
    bool ParseYaml(const FString& Text);
};

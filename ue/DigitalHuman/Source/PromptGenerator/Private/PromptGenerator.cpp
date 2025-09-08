#include "PromptGenerator.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

TSharedPtr<FJsonObject> UPromptGenerator::MakeMsg(const FString& Role, const FString& Content)
{
    auto Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("role"), Role);
    Obj->SetStringField(TEXT("content"), Content);
    return Obj;
}

FString UPromptGenerator::BuildSystemPrompt() const
{
    TArray<FString> Lines;
    if (!Persona.IsEmpty())      Lines.Add(Persona);
    if (!Style.IsEmpty())        Lines.Add("Style: " + Style);
    if (!Constraints.IsEmpty())  Lines.Add("Constraints: " + Constraints);
    if (!OutputFormat.IsEmpty()) Lines.Add("Output format: " + OutputFormat);

    if (TTSFriendly)
    {
        Lines.Add(TEXT("For TTS streaming: use short sentences with clear punctuation (.,!?). No markdown."));
    }

    // 预置记忆 Facts
    if (Facts.Num() > 0)
    {
        Lines.Add(TEXT("=== Memory Context ==="));
        for (int32 i=0;i<Facts.Num();++i)
            Lines.Add(FString::Printf(TEXT("%d) %s"), i+1, *Facts[i]));
    }

    return FString::Join(Lines, TEXT("\n"));
}

bool UPromptGenerator::LoadFromYaml(const FString& AbsPath)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *AbsPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[PromptGenerator] Could not read YAML: %s"), *AbsPath);
        return false;
    }
    return ParseYaml(Text);
}

void UPromptGenerator::BuildMessages(const TArray<FString>& Roles,
                                     const TArray<FString>& Contents,
                                     TArray<TSharedPtr<FJsonValue>>& OutMsgs) const
{
    OutMsgs.Reset();

    // system
    OutMsgs.Add(MakeShared<FJsonValueObject>(MakeMsg(TEXT("system"), BuildSystemPrompt())));

    // few-shots
    for (const auto& FS : FewShots)
    {
        if (!FS.User.IsEmpty())
            OutMsgs.Add(MakeShared<FJsonValueObject>(MakeMsg(TEXT("user"), FS.User)));
        if (!FS.Assistant.IsEmpty())
            OutMsgs.Add(MakeShared<FJsonValueObject>(MakeMsg(TEXT("assistant"), FS.Assistant)));
    }

    // real conversation
    for (int32 i=0;i<Roles.Num();++i)
        OutMsgs.Add(MakeShared<FJsonValueObject>(MakeMsg(Roles[i], Contents[i])));
}

void UPromptGenerator::MaybeAttachStop(TSharedPtr<FJsonObject> Root) const
{
    if (!Root.IsValid() || Stop.Num()==0) return;
    TArray<TSharedPtr<FJsonValue>> Stops;
    for (const auto& S : Stop) Stops.Add(MakeShared<FJsonValueString>(S));
    Root->SetArrayField(TEXT("stop"), Stops);
}

// ======= 极简 YAML 解析实现（子集） =======

static FString TrimLeftN(const FString& S, int32 N)
{
    if (N <= 0) return S;
    if (N >= S.Len()) return TEXT("");
    return S.Mid(N);
}

bool UPromptGenerator::ParseYaml(const FString& Text)
{
    // 清空旧值
    Persona.Empty(); Style.Empty(); Constraints.Empty(); OutputFormat.Empty();
    TTSFriendly = true; Facts.Reset(); Stop.Reset(); FewShots.Reset();

    enum class ESection { None, Facts, Stop, FewShots, Unknown };
    ESection Section = ESection::None;

    FPGFewShot PendingFS;
    bool bInsideFSItem = false;

    TArray<FString> Lines;
    Text.ParseIntoArrayLines(Lines, /*CullEmpty*/false);

    auto SetKV = [&](const FString& Key, const FString& Val)
    {
        const FString K = Key.ToLower();

        if (K == TEXT("persona"))            { Persona      = Val; return; }
        if (K == TEXT("style"))              { Style        = Val; return; }
        if (K == TEXT("constraints"))        { Constraints  = Val; return; }
        if (K == TEXT("output_format"))      { OutputFormat = Val; return; }
        if (K == TEXT("tts_friendly"))       { TTSFriendly  = (Val.ToLower()==TEXT("true") || Val==TEXT("1")); return; }

        // 未知顶层键：忽略
    };

    auto FlushPendingFS = [&]()
    {
        if (!PendingFS.User.IsEmpty() || !PendingFS.Assistant.IsEmpty())
            FewShots.Add(PendingFS);
        PendingFS = FPGFewShot{};
    };

    for (int32 li=0; li<Lines.Num(); ++li)
    {
        FString L = Lines[li];
        // 去 BOM/制表符 → 空白
        L.TrimStartAndEndInline();
        if (L.IsEmpty() || L.StartsWith(TEXT("#"))) continue;

        // 顶层列表项或键值
        // 匹配 list 开始：facts: / stop: / few_shots:
        if (!L.StartsWith(TEXT("-")) && L.EndsWith(TEXT(":")))
        {
            // 新 section
            const FString Key = L.LeftChop(1).TrimStartAndEnd();
            const FString KL  = Key.ToLower();

            // 切换 section 之前，若处于 few_shots item，先 flush
            if (bInsideFSItem) { FlushPendingFS(); bInsideFSItem = false; }

            if (KL == TEXT("facts"))      { Section = ESection::Facts; continue; }
            if (KL == TEXT("stop"))       { Section = ESection::Stop;  continue; }
            if (KL == TEXT("few_shots"))  { Section = ESection::FewShots; continue; }

            Section = ESection::Unknown; // 未知块
            continue;
        }

        // 顶层 key: value
        int32 ColonIdx = -1;
        if (Section == ESection::None || Section == ESection::Unknown)
        {
            if (L.FindChar(TEXT(':'), ColonIdx) && ColonIdx > 0)
            {
                const FString Key = L.Left(ColonIdx).TrimStartAndEnd();
                FString Val = L.Mid(ColonIdx+1).TrimStartAndEnd();
                // 去掉可能的引号
                Val.RemoveFromStart(TEXT("\"")); Val.RemoveFromEnd(TEXT("\""));
                Val.RemoveFromStart(TEXT("'"));  Val.RemoveFromEnd(TEXT("'"));
                SetKV(Key, Val);
                continue;
            }
        }

        // 处理列表
        if (Section == ESection::Facts)
        {
            if (L.StartsWith(TEXT("-")))
            {
                FString Item = TrimLeftN(L, 1).TrimStartAndEnd();
                Item.RemoveFromStart(TEXT("\"")); Item.RemoveFromEnd(TEXT("\""));
                Item.RemoveFromStart(TEXT("'"));  Item.RemoveFromEnd(TEXT("'"));
                if (!Item.IsEmpty()) Facts.Add(Item);
            }
            continue;
        }

        if (Section == ESection::Stop)
        {
            if (L.StartsWith(TEXT("-")))
            {
                FString Item = TrimLeftN(L, 1).TrimStartAndEnd();
                Item.RemoveFromStart(TEXT("\"")); Item.RemoveFromEnd(TEXT("\""));
                Item.RemoveFromStart(TEXT("'"));  Item.RemoveFromEnd(TEXT("'"));
                if (!Item.IsEmpty()) Stop.Add(Item);
            }
            continue;
        }

        if (Section == ESection::FewShots)
        {
            //  - user: ...
            //    assistant: ...
            if (L.StartsWith(TEXT("-")))
            {
                // 开始新 few_shot
                if (bInsideFSItem) { FlushPendingFS(); }
                bInsideFSItem = true;

                FString Rest = TrimLeftN(L, 1).TrimStartAndEnd();
                int32 C = -1;
                if (Rest.FindChar(TEXT(':'), C) && C>0)
                {
                    FString Key = Rest.Left(C).TrimStartAndEnd().ToLower();
                    FString Val = Rest.Mid(C+1).TrimStartAndEnd();
                    Val.RemoveFromStart(TEXT("\"")); Val.RemoveFromEnd(TEXT("\""));
                    Val.RemoveFromStart(TEXT("'"));  Val.RemoveFromEnd(TEXT("'"));
                    if (Key==TEXT("user")) PendingFS.User = Val;
                    else if (Key==TEXT("assistant")) PendingFS.Assistant = Val;
                }
                continue;
            }
            else
            {
                // 缩进行（继续填充当前 few_shot）
                int32 C2=-1;
                if (L.FindChar(TEXT(':'), C2) && C2>0)
                {
                    FString Key = L.Left(C2).TrimStartAndEnd().ToLower();
                    FString Val = L.Mid(C2+1).TrimStartAndEnd();
                    Val.RemoveFromStart(TEXT("\"")); Val.RemoveFromEnd(TEXT("\""));
                    Val.RemoveFromStart(TEXT("'"));  Val.RemoveFromEnd(TEXT("'"));

                    if (Key==TEXT("user")) PendingFS.User = Val;
                    else if (Key==TEXT("assistant")) PendingFS.Assistant = Val;
                }
                continue;
            }
        }

        // 其他未知内容：忽略
    }

    if (bInsideFSItem) { FlushPendingFS(); }

    return true;
}

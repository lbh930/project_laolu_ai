#include "PromptGenerator.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Json.h"

TSharedPtr<FJsonObject> UPromptGenerator::MakeMsg(const FString& Role, const FString& Content)
{
    auto Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("role"), Role);
    Obj->SetStringField(TEXT("content"), Content);
    return Obj;
}

FString UPromptGenerator::StripQuotes(const FString& S)
{
    FString T = S;
    T.TrimStartAndEndInline();
    if (T.StartsWith(TEXT("\"")) && T.EndsWith(TEXT("\"")) && T.Len() >= 2) return T.Mid(1, T.Len()-2);
    if (T.StartsWith(TEXT("'"))  && T.EndsWith(TEXT("'"))  && T.Len() >= 2) return T.Mid(1, T.Len()-2);
    return T;
}

int32 UPromptGenerator::LeadingSpaces(const FString& S)
{
    int32 C = 0;
    for (int32 i=0;i<S.Len();++i)
    {
        const TCHAR ch = S[i];
        if (ch == TEXT(' ')) ++C;
        else if (ch == TEXT('\t')) C += 2; // 粗略把 tab 当 2 空格
        else break;
    }
    return C;
}

FString UPromptGenerator::BuildSystemPrompt() const
{
    TArray<FString> Lines;
    if (!Persona.IsEmpty())      Lines.Add(Persona);
    if (!Style.IsEmpty())        Lines.Add(TEXT("Style: ") + Style);
    if (!Constraints.IsEmpty())  Lines.Add(TEXT("Constraints: ") + Constraints);
    if (!OutputFormat.IsEmpty()) Lines.Add(TEXT("Output format: ") + OutputFormat);

    if (TTSFriendly)
    {
        Lines.Add(TEXT("For TTS streaming: use short sentences with clear punctuation (.,!?). No markdown."));
    }

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

    // few_shots
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

// ================== YAML 子集解析 ==================

bool UPromptGenerator::ParseYaml(const FString& Text)
{
    // 清空旧值
    Persona.Empty(); Style.Empty(); Constraints.Empty(); OutputFormat.Empty();
    TTSFriendly = true; Facts.Reset(); Stop.Reset(); FewShots.Reset();

    enum class ESection { None, Facts, Stop, FewShots };
    ESection Section = ESection::None;

    // 处理多行标量：key: | 之后的块
    bool   bInBlock = false;
    FString BlockKey;
    int32  BlockIndent = 0;
    TArray<FString> BlockLines;

    auto FlushBlock = [&]()
    {
        if (!bInBlock) return;
        FString Val = FString::Join(BlockLines, TEXT("\n"));
        Val.TrimStartAndEndInline();

        const FString Key = BlockKey.ToLower();
        if (Key == TEXT("persona"))       Persona      = Val;
        else if (Key == TEXT("style"))    Style        = Val;
        else if (Key == TEXT("constraints"))  Constraints  = Val;
        else if (Key == TEXT("output_format")) OutputFormat = Val;
        // 其他块忽略

        bInBlock = false;
        BlockKey.Empty();
        BlockLines.Reset();
        BlockIndent = 0;
    };

    // few_shots 累积器
    FPGFewShot PendingFS;
    bool bInsideFSItem = false;
    auto FlushFS = [&]()
    {
        if (!PendingFS.User.IsEmpty() || !PendingFS.Assistant.IsEmpty())
            FewShots.Add(PendingFS);
        PendingFS = FPGFewShot{};
    };

    // 逐行
    TArray<FString> Lines;
    Text.ParseIntoArrayLines(Lines, /*CullEmpty*/false);

    for (int32 li=0; li<Lines.Num(); ++li)
    {
        FString Raw = Lines[li];

        // 注释 & BOM/空白
        // （注意：在多行块模式下，空行也是块内容之一）
        if (!bInBlock)
        {
            FString Tmp = Raw;
            Tmp.TrimStartAndEndInline();
            if (Tmp.IsEmpty() || Tmp.StartsWith(TEXT("#")))
            {
                continue;
            }
        }

        // 进入/处理 多行块
        if (bInBlock)
        {
            // 结束条件：遇到更浅缩进且非空且不是注释 → 说明到顶层了
            const int32 Ind = LeadingSpaces(Raw);
            FString Stripped = Raw.Mid(Ind);

            if (!Stripped.TrimStartAndEnd().IsEmpty() && Ind < BlockIndent && !Stripped.StartsWith(TEXT("#")))
            {
                // 块结束，先回退一行，后续正常处理
                FlushBlock();
                // 让这一行按普通逻辑再走一遍：
                // （通过减少 li 让 for 循环重新处理该行）
                li--;
                continue;
            }
            else
            {
                // 仍在块内：把当前行按块缩进裁剪后追加
                FString AppendLine;
                if (Stripped.IsEmpty())
                {
                    AppendLine = TEXT(""); // 空行保留
                }
                else
                {
                    const int32 Cut = FMath::Max(0, Ind - BlockIndent);
                    AppendLine = Raw.Mid(Ind); // 以块基准去除缩进
                }
                BlockLines.Add(AppendLine);
                continue;
            }
        }

        // 正常模式
        FString Line = Raw;
        Line.TrimStartAndEndInline();
        const int32 Indent = LeadingSpaces(Raw);
        const bool  IsTopLevel = (Indent == 0);

        // 识别块起始： key: |
        if (Line.EndsWith(TEXT("|")))
        {
            int32 C = -1;
            if (Line.FindChar(TEXT(':'), C) && C>0)
            {
                FString Key = Line.Left(C).TrimStartAndEnd();
                FString After = Line.Mid(C+1).TrimStartAndEnd(); // 应该是 "|"
                if (After == TEXT("|"))
                {
                    bInBlock   = true;
                    BlockKey   = Key;
                    BlockIndent = -1;

                    // 确定块基线缩进：取下一行的缩进
                    // （如果下一行不存在或是空，我们等到下一轮再设置）
                    // 这里不立即设置 BlockIndent，等第一条块内容行来时设置
                    // 但为了实现，我们在第一条块内容行时若 BlockIndent== -1，就用该行缩进
                    // —— 在上面 bInBlock 分支里处理
                    // 为简单起见，我们先设置 2 个空格基线，如果下一行更深会在第一次追加时被覆盖
                    BlockIndent = 0;
                    BlockLines.Reset();

                    // 如果下一行就来了，会在上面的 bInBlock 分支累积
                    continue;
                }
            }
        }

        // 顶层块：facts / stop / few_shots
        if (IsTopLevel && Line.EndsWith(TEXT(":")))
        {
            const FString Key = Line.LeftChop(1).TrimStartAndEnd().ToLower();

            // 切换 section 前，若在 few_shots 的 item 里，先 flush
            if (Section == ESection::FewShots && bInsideFSItem) { FlushFS(); bInsideFSItem = false; }

            if      (Key == TEXT("facts"))     { Section = ESection::Facts;     continue; }
            else if (Key == TEXT("stop"))      { Section = ESection::Stop;      continue; }
            else if (Key == TEXT("few_shots")) { Section = ESection::FewShots;  continue; }
            else
            {
                Section = ESection::None;
                continue;
            }
        }

        // 顶层 key: value（例如 tts_friendly: true）
        {
            int32 C = -1;
            if (IsTopLevel && Line.FindChar(TEXT(':'), C) && C>0)
            {
                const FString Key = Line.Left(C).TrimStartAndEnd().ToLower();
                FString Val = Line.Mid(C+1).TrimStartAndEnd();
                Val = StripQuotes(Val);

                if      (Key == TEXT("persona"))        { Persona      = Val; }
                else if (Key == TEXT("style"))          { Style        = Val; }
                else if (Key == TEXT("constraints"))    { Constraints  = Val; }
                else if (Key == TEXT("output_format"))  { OutputFormat = Val; }
                else if (Key == TEXT("tts_friendly"))   { const FString L = Val.ToLower(); TTSFriendly = (L==TEXT("true") || L==TEXT("1") || L==TEXT("yes")); }
                else
                {
                    // 未知顶层键：忽略
                    UE_LOG(LogTemp, Warning, TEXT("Prompt Generator: Unknown Key!"));
                }
                continue;
            }
        }

        // 列表处理
        if (Section == ESection::Facts)
        {
            //   - "..."
            if (Line.StartsWith(TEXT("-")))
            {
                FString Item = Line.Mid(1).TrimStartAndEnd();
                Item = StripQuotes(Item);
                if (!Item.IsEmpty()) Facts.Add(Item);
            }
            continue;
        }

        if (Section == ESection::Stop)
        {
            if (Line.StartsWith(TEXT("-")))
            {
                FString Item = Line.Mid(1).TrimStartAndEnd();
                Item = StripQuotes(Item);
                if (!Item.IsEmpty()) Stop.Add(Item);
            }
            continue;
        }

        if (Section == ESection::FewShots)
        {
            // 支持两种：同一行 "- user: ..." 或多行
            if (Line.StartsWith(TEXT("-")))
            {
                // 新条目开始
                if (bInsideFSItem) { FlushFS(); }
                bInsideFSItem = true;
                PendingFS = FPGFewShot{};

                FString Rest = Line.Mid(1).TrimStartAndEnd();
                int32 C = -1;
                if (Rest.FindChar(TEXT(':'), C) && C>0)
                {
                    FString Key = Rest.Left(C).TrimStartAndEnd().ToLower();
                    FString Val = StripQuotes(Rest.Mid(C+1).TrimStartAndEnd());
                    if (Key == TEXT("user"))      PendingFS.User = Val;
                    else if (Key == TEXT("assistant")) PendingFS.Assistant = Val;
                }
                continue;
            }
            else
            {
                // 条目内的缩进行： user: ... / assistant: ...
                int32 C = -1;
                if (Line.FindChar(TEXT(':'), C) && C>0)
                {
                    FString Key = Line.Left(C).TrimStartAndEnd().ToLower();
                    FString Val = StripQuotes(Line.Mid(C+1).TrimStartAndEnd());
                    if (Key == TEXT("user"))      PendingFS.User = Val;
                    else if (Key == TEXT("assistant")) PendingFS.Assistant = Val;
                }
                continue;
            }
        }

        // 其他情况：忽略
    }

    // 收尾
    if (bInBlock)   { FlushBlock(); }
    if (bInsideFSItem) { FlushFS(); }

    return true;
}

// Chatbot.cpp
#include "Chatbot.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "Async/Async.h"
#include "Containers/Ticker.h"   // ✅ for FTSTicker
#include "PromptGenerator.h"  // 新模块头
#include "Misc/Paths.h"

static TSharedPtr<FJsonObject> MakeMsg(const FString& Role, const FString& Content)
{
    auto Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("role"), Role);
    Obj->SetStringField(TEXT("content"), Content);
    return Obj;
}

// ======== 你已有的非流式 SendChat 保持原样 ========
void UChatbotClient::SendChat(const TArray<FString>& Roles, const TArray<FString>& Contents,
                              float Temperature,
                              const FOnChatResponse& OnOk,
                              const FOnChatError& OnFail)
{
    if (ApiKey.IsEmpty())
    {
        AsyncTask(ENamedThreads::GameThread, [OnFail]{ OnFail.ExecuteIfBound(TEXT("Missing API key")); });
        return;
    }
    if (Roles.Num() != Contents.Num() || Roles.Num() == 0)
    {
        AsyncTask(ENamedThreads::GameThread, [OnFail]{ OnFail.ExecuteIfBound(TEXT("Invalid messages")); });
        return;
    }

    const FString Url = BaseUrl + TEXT("/chat/completions");

    auto Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
    
    UE_LOG(LogTemp, Display, TEXT("Sending message: %s"), *Contents[0]);
    UE_LOG(LogTemp, Display, TEXT("Using API Key: %s"), *ApiKey);
    
    //print the api key that isbeing used
    FString Content;
    
    // body
    auto Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("model"), Model);
    Root->SetBoolField(TEXT("stream"), false);
    Root->SetNumberField(TEXT("temperature"), Temperature);

    TArray<TSharedPtr<FJsonValue>> Msgs;
    
    // [NEW] 使用 PromptGenerator 从 YAML 注入预置记忆 / 风格（失败则回退到原逻辑）
    bool bPGBuilt = false;
    {
        UPromptGenerator* PG = NewObject<UPromptGenerator>(this);
        if (PG)
        {
            const FString YamlPath = FPaths::ProjectContentDir() / TEXT("Persona/Memory.yaml");
            if (PG->LoadFromYaml(YamlPath))
            {
                PG->BuildMessages(Roles, Contents, Msgs);
                PG->MaybeAttachStop(Root);
                bPGBuilt = true;
            }
        }else
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to load Memory.yaml"));
        }
    }

    if (!bPGBuilt)
    {
        // 原始构造方式（回退）
        for (int32 i=0;i<Roles.Num();++i)
        {
            Msgs.Add(MakeShared<FJsonValueObject>(MakeMsg(Roles[i], Contents[i])));
        }
    }
    
    Root->SetArrayField(TEXT("messages"), Msgs);

    FString Body;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
    FJsonSerializer::Serialize(Root, Writer);
    Req->SetContentAsString(Body);

    Req->OnProcessRequestComplete().BindLambda(
        [OnOk, OnFail](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
        {
            if (!bOk || !Resp.IsValid())
            {
                AsyncTask(ENamedThreads::GameThread, [OnFail]{ OnFail.ExecuteIfBound(TEXT("HTTP failed")); });
                return;
            }
            const int32 Code = Resp->GetResponseCode();
            if (Code < 200 || Code >= 300)
            {
                const FString Err = FString::Printf(TEXT("HTTP %d: %s"), Code, *Resp->GetContentAsString());
                AsyncTask(ENamedThreads::GameThread, [OnFail, Err]{ OnFail.ExecuteIfBound(Err); });
                return;
            }

            // parse choices[0].message.content
            TSharedPtr<FJsonObject> RootObj;
            auto Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
            if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
            {
                AsyncTask(ENamedThreads::GameThread, [OnFail]{ OnFail.ExecuteIfBound(TEXT("JSON parse error")); });
                return;
            }

            const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
            if (!RootObj->TryGetArrayField(TEXT("choices"), Choices) || Choices->Num()==0)
            {
                AsyncTask(ENamedThreads::GameThread, [OnFail]{ OnFail.ExecuteIfBound(TEXT("Empty choices")); });
                return;
            }

            const auto Choice0 = (*Choices)[0]->AsObject();
            const auto MsgObj  = Choice0->GetObjectField(TEXT("message"));
            const FString Content = MsgObj->GetStringField(TEXT("content"));

            AsyncTask(ENamedThreads::GameThread, [OnOk, Content]{ OnOk.ExecuteIfBound(Content); });
        });

    Req->SetTimeout(30.0f);
    Req->ProcessRequest();
}
// ======== 新增：流式（SSE）- 使用 Ticker 轮询响应缓冲 ========
void UChatbotClient::SendChatStream(const TArray<FString>& Roles, const TArray<FString>& Contents,
                                    float Temperature, const FOnChatDelta& OnDelta,
                                    const FOnChatResponse& OnDone, const FOnChatError& OnFail)
{
    if (ApiKey.IsEmpty())
    { AsyncTask(ENamedThreads::GameThread, [OnFail]{ OnFail.ExecuteIfBound(TEXT("Missing API key")); }); return; }
    if (Roles.Num() != Contents.Num() || Roles.Num() == 0)
    { AsyncTask(ENamedThreads::GameThread, [OnFail]{ OnFail.ExecuteIfBound(TEXT("Invalid messages")); }); return; }

    const FString Url = BaseUrl + TEXT("/chat/completions");
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(Url);
    Req->SetVerb(TEXT("POST"));
    Req->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
    Req->SetHeader(TEXT("Accept"), TEXT("text/event-stream")); // hint for SSE

    // body
    auto Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("model"), Model);
    Root->SetBoolField(TEXT("stream"), true);
    Root->SetNumberField(TEXT("temperature"), Temperature);
    TArray<TSharedPtr<FJsonValue>> Msgs;

    // [NEW] 使用 PromptGenerator 从 YAML 注入预置记忆 / 风格（失败则回退到原逻辑）
    bool bPGBuilt = false;
    {
        UPromptGenerator* PG = NewObject<UPromptGenerator>(this);
        if (PG)
        {
            const FString YamlPath = FPaths::ProjectContentDir() / TEXT("Persona/Memory.yaml");
            if (PG->LoadFromYaml(YamlPath))
            {
                PG->BuildMessages(Roles, Contents, Msgs);
                PG->MaybeAttachStop(Root);
                bPGBuilt = true;
            }else
            {
                UE_LOG(LogTemp, Warning, TEXT("Failed to load Memory.yaml"));
            }
        }
    }

    if (!bPGBuilt)
    {
        // 原始构造方式（回退）
        for (int32 i=0;i<Roles.Num();++i)
            Msgs.Add(MakeShared<FJsonValueObject>(MakeMsg(Roles[i], Contents[i])));
    }

    Root->SetArrayField(TEXT("messages"), Msgs);

    FString Body; TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Body);
    FJsonSerializer::Serialize(Root, W);
    Req->SetContentAsString(Body);

    struct FStreamState
    {
        TArray<uint8> Accum;
        int32 PrevSize = 0;
        FTSTicker::FDelegateHandle TickHandle;
        bool bTickerActive = false;
    };
    TSharedRef<FStreamState, ESPMode::ThreadSafe> State = MakeShared<FStreamState, ESPMode::ThreadSafe>();

    // 开头：头到达后启动轮询
    Req->OnHeaderReceived().BindLambda([State, Req, OnDelta](FHttpRequestPtr, const FString&, const FString&)
    {
        if (State->bTickerActive) return;
        State->bTickerActive = true;

        State->TickHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateLambda([State, Req, OnDelta](float)
            {
                FHttpResponsePtr Resp = Req->GetResponse();
                if (!Resp.IsValid()) return true;

                const TArray<uint8>& Buf = Resp->GetContent();
                if (State->PrevSize > Buf.Num()) State->PrevSize = 0;
                const int32 NewBytes = Buf.Num() - State->PrevSize;
                if (NewBytes <= 0) return true;

                State->Accum.Append(Buf.GetData()+State->PrevSize, NewBytes);
                State->PrevSize = Buf.Num();

                // 逐个事件处理（SSE 以 "\n\n" 分隔）
                auto& B = State->Accum;
                while (true)
                {
                    int32 Split = -1;
                    for (int32 i=0;i+1<B.Num();++i) if (B[i]=='\n' && B[i+1]=='\n') { Split=i; break; }
                    if (Split < 0) break;

                    FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(B.GetData()), Split);
                    FString Chunk(Conv.Length(), Conv.Get());

                    // consume
                    if (Split+2 < B.Num())
                    { TArray<uint8> Tail; Tail.Append(B.GetData()+Split+2, B.Num()-Split-2); B = MoveTemp(Tail); }
                    else { B.Reset(); }

                    // 每行以 data: 开头
                    TArray<FString> Lines; Chunk.ParseIntoArrayLines(Lines);
                    for (const FString& L : Lines)
                    {
                        if (!L.StartsWith(TEXT("data:"))) continue;
                        const FString Payload = L.Mid(5).TrimStartAndEnd();
                        if (Payload == TEXT("[DONE]")) continue;

                        TSharedPtr<FJsonObject> Obj; auto R = TJsonReaderFactory<>::Create(Payload);
                        if (FJsonSerializer::Deserialize(R, Obj) && Obj.IsValid())
                        {
                            const TArray<TSharedPtr<FJsonValue>>* Choices=nullptr;
                            if (Obj->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num()>0)
                            {
                                auto C0 = (*Choices)[0]->AsObject();
                                if (C0.IsValid() && C0->HasTypedField<EJson::Object>(TEXT("delta")))
                                {
                                    auto Delta = C0->GetObjectField(TEXT("delta"));
                                    FString DeltaTxt;
                                    if (Delta->TryGetStringField(TEXT("content"), DeltaTxt) && !DeltaTxt.IsEmpty())
                                    {
                                        AsyncTask(ENamedThreads::GameThread, [OnDelta, DeltaTxt]
                                        { OnDelta.ExecuteIfBound(DeltaTxt); });
                                    }
                                }
                            }
                        }
                    }
                }
                return true;
            }),
            0.02f
        );
    });

    // 收尾：移除 Ticker 并可选解析完整体
    Req->OnProcessRequestComplete().BindLambda([State, OnDone, OnFail](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
    {
        if (State->bTickerActive)
        {
            FTSTicker::GetCoreTicker().RemoveTicker(State->TickHandle);
            State->bTickerActive = false;
        }

        if (!bOk || !Resp.IsValid())
        { AsyncTask(ENamedThreads::GameThread, [OnFail]{ OnFail.ExecuteIfBound(TEXT("HTTP failed")); }); return; }

        const int32 Code = Resp->GetResponseCode();
        if (Code < 200 || Code >= 300)
        {
            const FString Err = FString::Printf(TEXT("HTTP %d: %s"), Code, *Resp->GetContentAsString());
            AsyncTask(ENamedThreads::GameThread, [OnFail, Err]{ OnFail.ExecuteIfBound(Err); }); return;
        }

        // optional: best-effort final content
        FString Out;
        TSharedPtr<FJsonObject> RootObj; auto Reader = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
        if (FJsonSerializer::Deserialize(Reader, RootObj) && RootObj.IsValid())
        {
            const TArray<TSharedPtr<FJsonValue>>* Choices=nullptr;
            if (RootObj->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num()>0)
            {
                auto MsgObj = (*Choices)[0]->AsObject()->GetObjectField(TEXT("message"));
                MsgObj->TryGetStringField(TEXT("content"), Out);
            }
        }
        AsyncTask(ENamedThreads::GameThread, [OnDone, Out]{ OnDone.ExecuteIfBound(Out); });
    });

    Req->SetTimeout(0);
    Req->ProcessRequest();
}

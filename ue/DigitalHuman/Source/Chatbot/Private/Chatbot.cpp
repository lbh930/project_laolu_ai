// Chatbot.cpp
#include "Chatbot.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "Async/Async.h"
#include "Modules/ModuleManager.h"

static TSharedPtr<FJsonObject> MakeMsg(const FString& Role, const FString& Content)
{
    auto Obj = MakeShared<FJsonObject>();
    Obj->SetStringField(TEXT("role"), Role);
    Obj->SetStringField(TEXT("content"), Content);
    return Obj;
}

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
    for (int32 i=0;i<Roles.Num();++i)
    {
        Msgs.Add(MakeShared<FJsonValueObject>(MakeMsg(Roles[i], Contents[i])));
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
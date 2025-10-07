// WebInterfaceSubsystem.cpp
#include "WebInterfaceSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

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

static UActorComponent* FindComponentByNameOrClass(AActor* Actor, const FString& NameOrClass)
{
	// 先按名字匹配
	for (UActorComponent* C : Actor->GetComponents())
	{
		if (IsValid(C) && C->GetName().Equals(NameOrClass, ESearchCase::IgnoreCase))
		{
			return C;
		}
	}
	// 再按类名匹配
	for (UActorComponent* C : Actor->GetComponents())
	{
		if (IsValid(C) && C->GetClass()->GetName().Equals(NameOrClass, ESearchCase::IgnoreCase))
		{
			return C;
		}
	}
	return nullptr;
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
	OnRawMessage.Broadcast(JsonOrText);

	FString TrimmedJsonOrText = JsonOrText.ReplaceEscapedCharWithChar();
	if (TrimmedJsonOrText.Len() > 0 && TrimmedJsonOrText[0] == TEXT('"') && TrimmedJsonOrText.EndsWith(TEXT("\"")))
	{
		TrimmedJsonOrText = TrimmedJsonOrText.Left(TrimmedJsonOrText.Len() - 1);
		TrimmedJsonOrText.RemoveAt(0, 1);
	}

	UE_LOG(LogWebInterfaceSubsystem, Log, TEXT("Received Request trimmed: %s"), *TrimmedJsonOrText);

	// 先尝试把整条消息当成 JSON Value 解析（可能是对象，也可能就是一个字符串）
	// 测试发现，由于前端原因，可能收到的是double-stringified的json。后续如果第一次解码不成功会尝试第二次解码
	TSharedPtr<FJsonValue> Any;
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TrimmedJsonOrText);
		FJsonSerializer::Deserialize(Reader, Any);
	}

	// 如果直接是对象，拿对象用
	TSharedPtr<FJsonObject> Root;
	if (Any.IsValid() && Any->Type == EJson::Object)
	{
		Root = Any->AsObject();
	}
	else if (Any.IsValid() && Any->Type == EJson::String)
	{
		// 收到的是被二次字符串化的 JSON：把字符串再解析一次
		const FString& Inner = Any->AsString();

		TSharedRef<TJsonReader<>> Reader2 = TJsonReaderFactory<>::Create(Inner);
		FJsonSerializer::Deserialize(Reader2, Root);
	}
	else
	{
		// 还可能是数组或其它；按需扩展
		// 留个调试信息方便定位
		UE_LOG(LogWebInterfaceSubsystem, Warning, TEXT("JSON parse produced non-object type (type=%d)"), Any.IsValid() ? (int32)Any->Type : -1);
	}

	// 不是 JSON 对象 -> 走纯文本兜底
	if (!Root.IsValid())
	{
		UE_LOG(LogWebInterfaceSubsystem, Warning, TEXT("Received Non-JSON request!"));
		FString ChatText;
		if (!TrimmedJsonOrText.StartsWith(TEXT("{")) && !TrimmedJsonOrText.StartsWith(TEXT("[")))
		{
			ChatText = TrimmedJsonOrText;
		}
		else
		{
			TryParseChatText(TrimmedJsonOrText, ChatText);
		}
		if (!ChatText.IsEmpty())
		{
			OnUserInputReceived.Broadcast(ChatText);
		}
		return;
	}

	FString Type;
	if (Root->TryGetStringField(TEXT("type"), Type))
	{
		
		if (Type.Equals(TEXT("chat"), ESearchCase::IgnoreCase))
		{
			FString ChatText;
			if (TryParseChatText(TrimmedJsonOrText, ChatText) && !ChatText.IsEmpty())
			{
				OnUserInputReceived.Broadcast(ChatText);
			}
			return;
		}
		else if (Type.Equals(TEXT("call"), ESearchCase::IgnoreCase))
		{
			HandleCallRequest(Root.ToSharedRef());
			return;
		}
	}else
	{
		UE_LOG(LogWebInterfaceSubsystem, Error, TEXT("Can not get type of from json"));
	}

	// 其它类型：按需扩展
}


bool UWebInterfaceSubsystem::TryParseChatText(const FString& JsonString, FString& OutText) const
{
	UE_LOG(LogWebInterfaceSubsystem, Verbose, TEXT("Parsing Text Steam: %s"), *JsonString);
	
	OutText.Reset();

	UE_LOG(LogWebInterfaceSubsystem, Log, TEXT("TryParseChatText: %s"), *JsonString);

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

	FString Type;
	if (!RootObj->TryGetStringField(TEXT("type"), Type)) return false;
	if (!Type.Equals(TEXT("chat"), ESearchCase::IgnoreCase)) return false;

	FString Text;
	if (!RootObj->TryGetStringField(TEXT("text"), Text)) return false;

	Text.TrimStartAndEndInline();
	OutText = MoveTemp(Text);
	return true;
}

void UWebInterfaceSubsystem::HandleCallRequest(const TSharedRef<FJsonObject>& Root)
{
    // requestId 可选，用于前端对应
    FString RequestId;
    Root->TryGetStringField(TEXT("requestId"), RequestId);

    const TSharedPtr<FJsonObject>* TargetObj = nullptr;
    if (!Root->TryGetObjectField(TEXT("target"), TargetObj) || !TargetObj) { return; }

    FString By, Value;
    (*TargetObj)->TryGetStringField(TEXT("by"), By);
    (*TargetObj)->TryGetStringField(TEXT("value"), Value);

    FString ComponentName, MethodName;
    Root->TryGetStringField(TEXT("component"), ComponentName);
    Root->TryGetStringField(TEXT("method"), MethodName);

	UE_LOG(LogWebInterfaceSubsystem, Log, TEXT("Call %s: by=%s, Value=%s, component=%s, method=%s"),
		*RequestId, *By, *Value, *ComponentName, *MethodName);

    // 1) 找 Actor
    AActor* TargetActor = nullptr;
    if (By.Equals(TEXT("tag"), ESearchCase::IgnoreCase))
    {
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            if (It->ActorHasTag(*Value))
            {
                TargetActor = *It;
                break;
            }
        }
    }
    else if (By.Equals(TEXT("name"), ESearchCase::IgnoreCase))
    {
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            if (It->GetName().Equals(Value, ESearchCase::IgnoreCase))
            {
                TargetActor = *It;
                break;
            }
        }
    }

    if (!TargetActor)
    {
    	UE_LOG(LogWebInterfaceSubsystem, Warning, TEXT("Request %s: Can not find actor %s"), *RequestId, *Value);
        SendPS2Response(false, RequestId, TEXT("Actor not found"), false);
        return;
    }

    // 2) 找 Component
    UActorComponent* Comp = FindComponentByNameOrClass(TargetActor, ComponentName);
    if (!Comp)
    {
    	UE_LOG(LogWebInterfaceSubsystem, Warning, TEXT("Request %s: Can not find component %s"), *RequestId, *Value);
        SendPS2Response(false, RequestId, TEXT("Component not found"), false);
        return;
    }

    // 3) 反射调用无参函数（返回 bool/float/int/FString 都行；这里以 bool 为例）
    const FName FuncName(*MethodName);
    UFunction* Func = Comp->FindFunction(FuncName);
    if (!Func)
    {
    	UE_LOG(LogWebInterfaceSubsystem, Warning, TEXT("Request %s: Can not find function %s"), *RequestId, *MethodName);
        SendPS2Response(false, RequestId, TEXT("Method not found"), false);
        return;
    }

    // 调用并尝试取得返回值
    struct FNoParams { }; FNoParams Params;
    Comp->ProcessEvent(Func, &Params);

    // 取返回值（如果函数有 ReturnValue 属性，UE 会生成同名 pin）
    bool bHasReturn = false;
    bool bReturnBool = false;
    for (TFieldIterator<FProperty> It(Func); It; ++It)
    {
        if (It->HasAnyPropertyFlags(CPF_ReturnParm))
        {
            if (FBoolProperty* BP = CastField<FBoolProperty>(*It))
            {
                bHasReturn = true;
                bReturnBool = BP->GetPropertyValue_InContainer(&Params);
            }
            // 你也可以扩展 FString/float/int 等类型
            break;
        }
    }

    if (bHasReturn)
    {
    	FString returnBoolString = bReturnBool ? TEXT("true") : TEXT("false");
    	UE_LOG(LogWebInterfaceSubsystem, Log, TEXT("Request %s: Responding result as %s"), *RequestId, *returnBoolString);
        SendPS2Response(true, RequestId, FString(), TOptional<bool>(bReturnBool));
    }
    else
    {
    	
    	UE_LOG(LogWebInterfaceSubsystem, Log, TEXT("Request %s: Responding empty response"), *RequestId);
        SendPS2Response(true, RequestId, TEXT("OK (no return)"), false);
    }
}

// 在 Subsystem 里，把要发的 JSON 串好并广播
void UWebInterfaceSubsystem::SendPS2Response(bool bOk, const FString& RequestId, const FString& ErrorMsg, TOptional<bool> BoolResult)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("type"), TEXT("response"));
	if (!RequestId.IsEmpty()) Obj->SetStringField(TEXT("requestId"), RequestId);
	Obj->SetBoolField(TEXT("ok"), bOk);
	if (!ErrorMsg.IsEmpty()) Obj->SetStringField(TEXT("error"), ErrorMsg);
	if (BoolResult.IsSet()) Obj->SetBoolField(TEXT("result"), BoolResult.GetValue());

	FString Out;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), W);

	// 交给蓝图层去“Send Pixel Streaming Response”
	OnSendPS2Response.Broadcast(Out);
}


AActor* UWebInterfaceSubsystem::FindTargetActor(const FString& By, const FString& Value) const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	if (By.Equals(TEXT("tag"), ESearchCase::IgnoreCase))
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsWithTag(World, FName(*Value), Found);
		return Found.Num() > 0 ? Found[0] : nullptr;
	}
	else if (By.Equals(TEXT("name"), ESearchCase::IgnoreCase))
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			// 兼容精确名与包含匹配（避免 PIE 后缀、_C 等差异）
			if (A->GetName().Equals(Value, ESearchCase::IgnoreCase) || A->GetName().Contains(Value))
			{
				return A;
			}
		}
	}
	return nullptr;
}

bool UWebInterfaceSubsystem::InvokeUFunctionReturn(UObject* Target, const FString& MethodName, TSharedPtr<FJsonValue>& ResultOut, FString& ErrorOut) const
{
	ResultOut.Reset();
	ErrorOut.Reset();

	if (!Target)
	{
		ErrorOut = TEXT("Target is null");
		return false;
	}

	UFunction* Func = Target->FindFunction(FName(*MethodName));
	if (!Func)
	{
		ErrorOut = FString::Printf(TEXT("Method '%s' not found"), *MethodName);
		return false;
	}

	// 目前示例方法没有参数；如需支持参数，可从 RootObj->GetObjectField("args") 解析并填充 Params
	TArray<uint8> Params;
	Params.SetNumZeroed(Func->ParmsSize);

	Target->ProcessEvent(Func, Params.GetData());

	// 读取返回值（支持常见基础类型；当前示例为 bool）
	if (FProperty* RetProp = Func->GetReturnProperty())
	{
		if (FBoolProperty* BP = CastField<FBoolProperty>(RetProp))
		{
			const bool b = BP->GetPropertyValue_InContainer(Params.GetData());
			ResultOut = MakeShared<FJsonValueBoolean>(b);
			return true;
		}
		else if (FIntProperty* IP = CastField<FIntProperty>(RetProp))
		{
			ResultOut = MakeShared<FJsonValueNumber>(IP->GetPropertyValue_InContainer(Params.GetData()));
			return true;
		}
		else if (FFloatProperty* FP = CastField<FFloatProperty>(RetProp))
		{
			ResultOut = MakeShared<FJsonValueNumber>(FP->GetPropertyValue_InContainer(Params.GetData()));
			return true;
		}
		else if (FStrProperty* SP = CastField<FStrProperty>(RetProp))
		{
			ResultOut = MakeShared<FJsonValueString>(SP->GetPropertyValue_InContainer(Params.GetData()));
			return true;
		}
		// 可按需扩展更多类型（FName、FVector 等转 JSON）
	}

	// 没有返回值也算成功
	return true;
}

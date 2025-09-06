// Deprecated
#include "TextToFaceWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "TextToFace.h"

void UTextToFaceWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!EngineClass)
	{
		EngineClass = NewObject<UTextToFaceEngine>(this);
		//TODO: set this via config or env instead of hard-coding
		EngineClass->SetXiApiKey(TEXT("sk_e30762316d0e7a4a1a7d5937dc468e8d72fde1b13b37e325"));
		EngineClass->SetVoiceId (TEXT("Q26iPuGVPnOfNa3FzCH6"));
		EngineClass->SetModelId (TEXT("eleven_v3"));
		// 不再在 Widget 绑定引擎完成回调，队列内循环在引擎侧处理
	}

	if (!ChatbotClient)
	{
		ChatbotClient = NewObject<UChatbotClient>(this);
		//TODO: set this via config or env instead of hard-coding
		ChatbotClient->ApiKey = TEXT("sk-dd53e4e7cff84456bd554e018dbe2b8c");
		ChatbotClient->BaseUrl = TEXT("https://api.deepseek.com");
		ChatbotClient->Model   = TEXT("deepseek-chat");
	}

	if (SendButton)
	{
		SendButton->OnClicked.AddDynamic(this, &UTextToFaceWidget::OnSendClicked);
	}
}

void UTextToFaceWidget::OnSendClicked()
{
	if (!InputTextBox || !ChatbotClient || !EngineClass) return;

	const FString UserText = InputTextBox->GetText().ToString();
	if (UserText.IsEmpty()) return;

	// reset streaming state
	StreamBuffer.Reset();
	if (UWorld* W = GetWorld())
	{
		LastFlushTimeSec = W->GetTimeSeconds();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UTextToFaceWidget::OnSendClicked() - Can't get world!"));
	}
	
	bStreamingInFlight = true;

	// build messages
	TArray<FString> Roles, Contents;
	Roles.Add(TEXT("system")); Contents.Add(TEXT("You are a helpful assistant."));
	Roles.Add(TEXT("user"));   Contents.Add(UserText);

	// 绑定流式回调（直接把可 flush 的文本交引擎）
	FOnChatDelta OnDelta; OnDelta.BindUFunction(this, FName("HandleChatDelta"));
	FOnChatResponse OnDone; OnDone.BindUFunction(this, FName("HandleChatDone"));
	FOnChatError OnErr; OnErr.BindUFunction(this, FName("HandleChatError"));

	ChatbotClient->SendChatStream(Roles, Contents, /*Temperature*/1.0f, OnDelta, OnDone, OnErr);
}

void UTextToFaceWidget::HandleChatResponse(const FString& AIResponse)
{
	AActor* UseTarget = TargetActor ? TargetActor : GetOwningPlayerPawn();
	if (EngineClass && UseTarget)
	{
		// 非流式一次性也走直接合成
		EngineClass->SynthesizeAndAnimate(AIResponse, UseTarget);
	}
}

void UTextToFaceWidget::HandleChatDelta(const FString& Delta)
{
	UE_LOG(LogTemp, Display, TEXT("Received delta from chatbot: %s"), *Delta);
	// 仅做文本分句缓冲
	StreamBuffer += Delta;

	// 更新时间戳
	if (UWorld* W = GetWorld())
	{
		LastFlushTimeSec = W->GetTimeSeconds();
		UE_LOG(LogTemp, Display, TEXT("UTextToFaceWidget::HandleChatDelta() - Current delta time is %f"), LastFlushTimeSec);
	}

	// 命中边界则 flush
	if (ReachedBoundary())
	{
		FlushBuffer(/*bForce*/false);
	}
}

void UTextToFaceWidget::HandleChatDone(const FString& Full)
{
	UE_LOG(LogTemp, Display, TEXT("Received ChatDone: %s"), *Full);
	FlushBuffer(/*bForce*/true);
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(FlushTimerHandle);
	}
	bStreamingInFlight = false;
}

void UTextToFaceWidget::HandleChatError(const FString& Error)
{
	UE_LOG(LogTemp, Error, TEXT("Chatbot Error: %s"), *Error);
	FlushBuffer(/*bForce*/true);
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(FlushTimerHandle);
	}
	bStreamingInFlight = false;
}

// === helpers ===

bool UTextToFaceWidget::ReachedBoundary() const
{
	// 命中句末标点
	if (!StreamBuffer.IsEmpty())
	{
		const TCHAR LastCh = StreamBuffer[StreamBuffer.Len()-1];
		if (SentenceBoundaries.Contains(FString(1, &LastCh)))
		{
			UE_LOG(LogTemp, Display, TEXT("Reached Boundary because of seeing %s"), *FString(1, &LastCh));
			return true;
		}
	}
	return false;
}

void UTextToFaceWidget::FlushBuffer(bool bForce)
{
	UE_LOG(LogTemp, Display, TEXT("Received FlushBuffer command"));
	if (StreamBuffer.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Trying to flush a empty buffer!"));
		return;
	}

	FString ToSpeak = StreamBuffer;
	ToSpeak.TrimStartAndEndInline();
	if (ToSpeak.IsEmpty() && !bForce)
	{
		UE_LOG(LogTemp, Display, TEXT("Skipping flush buffer since to speak is '%s' and bForce is %s"),
			*ToSpeak, bForce ? TEXT("true") : TEXT("false"));
		return;
	}

	// 清空本地缓冲
	StreamBuffer.Reset();

	// 直接交由引擎排队
	AActor* UseTarget = ResolveTargetActor();
	if (UseTarget && EngineClass)
	{
		// 交给引擎队列
		UE_LOG(LogTemp, Display, TEXT("Flushing to TextToFace: %s"), *ToSpeak);
		EngineClass->TextToFaceStreamAppend(ToSpeak, UseTarget);
		EngineClass->StartTTSStreamIfStopped(); // 若未在播则启动
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to flush: either UseTarget can't be resolved or EngineClass not defined"));
	}

	if (UWorld* W = GetWorld()) { LastFlushTimeSec = W->GetTimeSeconds(); }
}

AActor* UTextToFaceWidget::ResolveTargetActor() const
{
	if (TargetActor) return TargetActor;
	if (APlayerController* PC = GetOwningPlayer())
		return PC->GetPawn();
	return nullptr;
}

void UTextToFaceWidget::NativeDestruct()
{
	// 不再移除引擎事件绑定
	if (SendButton)
	{
		SendButton->OnClicked.RemoveAll(this);
	}
	Super::NativeDestruct();
}

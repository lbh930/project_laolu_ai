// Fill out your copyright notice in the Description page of Project Settings.


#include "HumanState.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Containers/Array.h"

// Sets default values for this component's properties
UHumanState::UHumanState()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UHumanState::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

bool const UHumanState::CanReceiveNewMessage()
{
	return !bIsTalking;
}

// Called every frame
void UHumanState::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (TextToFaceWidgetInstance == nullptr)
	{
		TArray<UUserWidget*> FoundWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), FoundWidgets, UTextToFaceWidget::StaticClass(), false);
		if (FoundWidgets.Num() > 0)
		{
			TextToFaceWidgetInstance = Cast<UTextToFaceWidget>(FoundWidgets[0]);
		}
	}

	if (TextToFaceWidgetInstance)
	{
		const FTextToFaceSnapshot Snap = TextToFaceWidgetInstance->GetRuntimeSnapshot();
		// 正在思考或正在说话都视为“在说话态”，避免打断
		bIsTalking = (Snap.bThinking || Snap.bSpeaking);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("HumanState: Can not find TextToFaceWidget Instance!"));
		bIsTalking = false;
	}
}


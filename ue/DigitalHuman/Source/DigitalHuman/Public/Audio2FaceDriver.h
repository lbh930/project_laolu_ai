// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Audio2FaceDriver.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DIGITALHUMAN_API UAudio2FaceDriver : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UAudio2FaceDriver();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void TestAudio2Speech();
};

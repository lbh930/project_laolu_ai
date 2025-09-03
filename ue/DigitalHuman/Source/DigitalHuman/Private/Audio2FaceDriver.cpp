// Fill out your copyright notice in the Description page of Project Settings.


#include "Audio2FaceDriver.h"
#include "ACEBlueprintLibrary.h"
#include "Sound/SoundWave.h"

// Sets default values for this component's properties
UAudio2FaceDriver::UAudio2FaceDriver()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


void UAudio2FaceDriver::BeginPlay()
{
	Super::BeginPlay();
	TestAudio2Speech();
}

void UAudio2FaceDriver::TestAudio2Speech()
{
	USoundWave* MySpeech = LoadObject<USoundWave>(nullptr, TEXT("/Human/Hero/Audios/TestAudios/Audio_Test_Truman.Audio_Test_Truman"));
	if (!MySpeech) return;

	AActor* Owner = GetOwner();
	bool bStarted = UACEBlueprintLibrary::AnimateCharacterFromSoundWave(
		Owner, MySpeech);

	UE_LOG(LogTemp, Log, TEXT("A2F Start: %s"), bStarted ? TEXT("OK") : TEXT("FAIL"));
}


// Called every frame
void UAudio2FaceDriver::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}


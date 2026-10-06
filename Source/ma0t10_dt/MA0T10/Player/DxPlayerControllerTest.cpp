// Fill out your copyright notice in the Description page of Project Settings.


#include "DxPlayerControllerTest.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

ADxPlayerControllerTest::ADxPlayerControllerTest()
{
	ClickActivationPolicy=EDxClickActivationPolicy::SingleRelease;
}

void ADxPlayerControllerTest::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		if (World->HasBegunPlay()) ApplyProjectViewportPolicy();
		else WorldBeginPlayHandle = World->OnWorldBeginPlay.AddUObject(this, &ADxPlayerControllerTest::ApplyProjectViewportPolicy);
	}
}

void ADxPlayerControllerTest::ApplyProjectViewportPolicy()
{
	if (UWorld* World = GetWorld(); GEngine && World && World->GetGameViewport())
	{
		GEngine->SetEngineStat(World, World->GetGameViewport(), TEXT("FPS"), false);
	}
}

void ADxPlayerControllerTest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld()) World->OnWorldBeginPlay.Remove(WorldBeginPlayHandle);
	Super::EndPlay(EndPlayReason);
}

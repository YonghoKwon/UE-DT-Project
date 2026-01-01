// Fill out your copyright notice in the Description page of Project Settings.


#include "DxGameInstance.h"

#include "GameFramework/GameUserSettings.h"

void UDxGameInstance::Init()
{
	Super::Init();

	UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings();
	if (UserSettings)
	{
		// Frame 60 고정
		UserSettings->SetFrameRateLimit(60.0f);
		UserSettings->ApplySettings(false);
	}
}

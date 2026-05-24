#include "Core/DxGameInstance.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformFileManager.h" // 디렉토리 생성을 위해 필요
#include "GameFramework/GameUserSettings.h"

UDxGameInstance* UDxGameInstance::Instance = nullptr;

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

void UDxGameInstance::Shutdown()
{
	Super::Shutdown();
}

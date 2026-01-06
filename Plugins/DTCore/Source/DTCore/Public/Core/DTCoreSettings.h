#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/DataTable.h"
#include "DTCoreSettings.generated.h"

UCLASS(Config=Game, defaultconfig, meta=(DisplayName="DTCore Settings"))
class DTCORE_API UDTCoreSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// API 데이터 테이블 경로 설정
	UPROPERTY(Config, EditAnywhere, Category = "Data Management")
	TSoftObjectPtr<UDataTable> ApiDataTable;
	// WebSocket 데이터 테이블 경로 설정
	UPROPERTY(Config, EditAnywhere, Category = "Data Management")
	TSoftObjectPtr<UDataTable> WebSocketDataTable;
	// Level 데이터 테이블 경로 설정
	UPROPERTY(Config, EditAnywhere, Category = "Data Management")
	TSoftObjectPtr<UDataTable> LevelDataTable;



	// API 기본 URL
	UPROPERTY(Config, EditAnywhere, Category = "Network")
	FString BaseApiUrl = "http://localhost:8090";
private:
protected:

public:
private:
protected:
};

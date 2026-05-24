#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/DataTable.h"
#include "DTCoreSettings.generated.h"

UCLASS(Config=Game, meta=(DisplayName="DTCore Settings"))
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

	UPROPERTY(Config, EditAnywhere, Category = "3D Objects")
	TMap<FString, TObjectPtr<UClass>> ObjectBPClasses;

	UPROPERTY(Config, EditAnywhere, Category = "Data Management")
	TSoftObjectPtr<UDataTable> ShipObjectNameDataTable;

	UPROPERTY(Config, EditAnywhere, Category = "Network")
	FString WebSocketUrl = "ws://172.18.45.234:61616";

	// API 기본 URL
	UPROPERTY(Config, EditAnywhere, Category = "Network")
	FString BaseApiUrl = "http://localhost:8090";

	UPROPERTY(Config, EditAnywhere, Category = "Network")
	FString LocalApiUrl = "http://172.31.247.114:8090";

	UPROPERTY(Config, EditAnywhere, Category = "Network")
	FString TestApiUrl = "http://172.18.45.234:8000";

	UPROPERTY(Config, EditAnywhere, Category = "Network")
	FString ProdApiUrl = "http://172.18.46.164:8000";

	UPROPERTY(Config, EditAnywhere, Category = "Network|Topics")
	TArray<FString> WebSocketTopics = { TEXT("topic.cep.output.0") };

	UPROPERTY(Config, EditAnywhere, Category = "Network|Topics")
	TArray<FString> ApiTopics = { TEXT("topic.api.output.0") };

	// 프로젝트 전체에서 기본으로 사용할 위젯 테마 데이터
	UPROPERTY(config, EditAnywhere, Category = "UI")
	TSoftObjectPtr<class UDxWidgetThemeData> DefaultWidgetTheme;
private:
protected:

public:
private:
protected:
};

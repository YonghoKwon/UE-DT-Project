// Fill out your copyright notice in the Description page of Project Settings.


#include "KP1D0012.h"

#include "Kismet/GameplayStatics.h"
#include "m7at10_dt/M7AT10/Core/DxDataSubsystem.h"
#include "m7at10_dt/M7AT10/Core/DxDataType.h"
#include "m7at10_dt/M7AT10/Crane/CraneDataSyncComp.h"
#include "m7at10_dt/M7AT10/Crane/CraneDataTypes.h"
#include "m7at10_dt/M7AT10/Lib/YyJsonParser.h"

UKP1D0012::UKP1D0012()
{
	TransactionCode = "KP1D0012";
}

void UKP1D0012::ProcessData(FYyJsonParser* JsonParser, yyjson_val* RootNode)
{
	Super::ProcessData(JsonParser, RootNode);

	// 구조:
	// {
	//   "CREATE_TIMESTAMP": "20251124215148",
	//   "DATA_MAP": {
	//     "20251124215148": { ... }
	//   }
	// }

	// 1. 타임스탬프 키 값 가져오기
	yyjson_val* TimestampVal = JsonParser->JsonParseKeyword(RootNode, TEXT("CREATE_TIMESTAMP"));
	FString TimestampKey = JsonParser->GetString(TimestampVal);

	// 2. DATA_MAP 객체 가져오기
	yyjson_val* DataMapObj = JsonParser->JsonParseKeyword(RootNode, TEXT("DATA_MAP"));

	if (JsonParser->IsValid(DataMapObj) && !TimestampKey.IsEmpty())
	{
		// 3. 타임스탬프 키로 실제 데이터 객체 접근
		yyjson_val* ContentObj = JsonParser->JsonParseKeyword(DataMapObj, TimestampKey);

		// if (JsonParser->IsValid(ContentObj))
		// {
		// 	size_t len;
		// 	// 디버그 출력용으로 JSON 문자열로 변환
		// 	const char *json_str = yyjson_val_write(DataMapObj, YYJSON_WRITE_PRETTY, &len);
		// 	if (json_str)
		// 	{
		// 		UE_LOG(LogTemp, Warning, TEXT("DataMapObj: %s"), ANSI_TO_TCHAR(json_str));
		// 		free((void*)json_str);
		// 	}
		// }
		// else
		// {
		// 	UE_LOG(LogTemp, Warning, TEXT("[KP1D0012] Content object not found for key: %s"), *TimestampKey);
		// }

		if (!JsonParser->IsValid(ContentObj))
		{
			UE_LOG(LogTemp, Warning, TEXT("[KP1D0012] Content object not found for key: %s"), *TimestampKey);
			return;
		}

		// 4. 데이터 파싱하여 구조체에 담기
		FCraneStateData CraneData;
		CraneData.CraneId = JsonParser->GetString(JsonParser->JsonParseKeyword(ContentObj, TEXT("CRANE_ID")));
		CraneData.Position.TrolleyPosition = static_cast<float>(
			JsonParser->GetNumber(JsonParser->JsonParseKeyword(ContentObj, TEXT("TROLLEY_POS")))
		);
		CraneData.Position.HoistHeight = static_cast<float>(
			JsonParser->GetNumber(JsonParser->JsonParseKeyword(ContentObj, TEXT("HOIST_HEIGHT")))
		);
		CraneData.Position.GantryPosition = static_cast<float>(
			JsonParser->GetNumber(JsonParser->JsonParseKeyword(ContentObj, TEXT("GANTRY_POS")))
		);
		// ... 필요한 필드 추가 파싱

		// 5. Subsystem을 통해 해당 Crane의 DataSyncComp 찾기
		UWorld* World = nullptr;

		// 방법 A: GEngine의 GameViewport에서 World 가져오기
		if (GEngine && GEngine->GameViewport)
		{
			World = GEngine->GameViewport->GetWorld();
		}

		// 방법 B: 또는 첫 번째 PIE/Game World 찾기
		if (!World && GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
				{
					World = Context.World();
					break;
				}
			}
		}

		if (World)
		{
			if (UGameInstance* GI = UGameplayStatics::GetGameInstance(World))
			{
				if (UDxDataSubsystem* DxDataSub = GI->GetSubsystem<UDxDataSubsystem>())
				{
					if (UCraneDataSyncComp* CraneDataSyncComp = DxDataSub->FindCraneDataSyncComp(CraneData.CraneId))
					{
						CraneDataSyncComp->OnReceiveData(EDxDataType::CraneState, &CraneData);
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("[KP1D0012] CraneDataSyncComp not found for CraneId: %s"), *CraneData.CraneId);
					}
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[KP1D0012] No valid World found"));
		}
	}
}

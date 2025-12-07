// Fill out your copyright notice in the Description page of Project Settings.


#include "KP1D0012.h"

#include "m7at10_dt/m7at10_dt.h"
#include "m7at10_dt/M7AT10/Core/DxDataType.h"
#include "m7at10_dt/M7AT10/Core/DxProcessSubsystem.h"
#include "m7at10_dt/M7AT10/Crane/CraneDataSyncComp.h"
#include "m7at10_dt/M7AT10/Crane/CraneDataTypes.h"
#include "m7at10_dt/M7AT10/Lib/YyJsonParser.h"

UKP1D0012::UKP1D0012()
{
	TransactionCode = "KP1D0012";
}

TSharedPtr<FTransactionCodeDataBase> UKP1D0012::ParseToStruct(const FString& JsonString)
{
	TSharedPtr<FKP1D0012Data> KP1D0012Data = MakeShared<FKP1D0012Data>();
	KP1D0012Data->TransactionCode = TEXT("KP1D0012");

	// --- yyjson 파싱 로직 ---
	FYyJsonParser Parser;
	if (Parser.JsonParse(JsonString))
	{
		yyjson_val* Root = Parser.GetRoot();

		KP1D0012Data->CreationTimestamp = Parser.GetString(Parser.JsonParseKeyword(Root, TEXT("CREATE_TIMESTAMP")));
	}

	return KP1D0012Data;
}

void UKP1D0012::ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data)
{
	TSharedPtr<FKP1D0012Data> KP1D0012Data = StaticCastSharedPtr<FKP1D0012Data>(Data);

	if (KP1D0012Data.IsValid())
	{
		// UE_LOG KP1D0012Data
		// UE_LOG(LogM7AT10, Log, TEXT("[KP1D0012] TransactionCode: %s / CreationTimestamp: %s"), *KP1D0012Data->TransactionCode, *KP1D0012Data->CreationTimestamp);
	}
}

// void UKP1D0012::ProcessData(FYyJsonParser* JsonParser, yyjson_val* RootNode)
// {
// 	Super::ProcessData(JsonParser, RootNode);
//
// 	// 구조:
// 	// {
// 	//   "CREATE_TIMESTAMP": "20251124215148",
// 	//   "DATA_MAP": {
// 	//     "20251124215148": { ... }
// 	//   }
// 	// }
//
// 	// 1. 타임스탬프 키 값 가져오기
// 	yyjson_val* TimestampVal = JsonParser->JsonParseKeyword(RootNode, TEXT("CREATE_TIMESTAMP"));
// 	FString TimestampKey = JsonParser->GetString(TimestampVal);
//
// 	// 2. DATA_MAP 객체 가져오기
// 	yyjson_val* DataMapObj = JsonParser->JsonParseKeyword(RootNode, TEXT("DATA_MAP"));
//
// 	if (JsonParser->IsValid(DataMapObj) && !TimestampKey.IsEmpty())
// 	{
// 		// 3. 타임스탬프 키로 실제 데이터 객체 접근
// 		yyjson_val* ContentObj = JsonParser->JsonParseKeyword(DataMapObj, TimestampKey);
//
// 		// if (JsonParser->IsValid(ContentObj))
// 		// {
// 		// 	size_t len;
// 		// 	// 디버그 출력용으로 JSON 문자열로 변환
// 		// 	const char *json_str = yyjson_val_write(DataMapObj, YYJSON_WRITE_PRETTY, &len);
// 		// 	if (json_str)
// 		// 	{
// 		// 		UE_LOG(LogM7AT10, Warning, TEXT("DataMapObj: %s"), ANSI_TO_TCHAR(json_str));
// 		// 		free((void*)json_str);
// 		// 	}
// 		// }
// 		// else
// 		// {
// 		// 	UE_LOG(LogM7AT10, Warning, TEXT("[KP1D0012] Content object not found for key: %s"), *TimestampKey);
// 		// }
//
// 		if (!JsonParser->IsValid(ContentObj))
// 		{
// 			UE_LOG(LogM7AT10, Warning, TEXT("[KP1D0012] Content object not found for key: %s"), *TimestampKey);
// 			return;
// 		}
//
// 		FString CREATE_TIMESTAMP = JsonParser->GetString(JsonParser->JsonParseKeyword(RootNode, TEXT("CREATE_TIMESTAMP")));
// 		FString USER_ID = JsonParser->GetString(JsonParser->JsonParseKeyword(ContentObj, TEXT("USER_ID"))).TrimStartAndEnd();
// 		UE_LOG(LogM7AT10, Log, TEXT("[KP1D0012] USER_ID: %s / CREATE_TIMESTAMP: %s"), *USER_ID, *CREATE_TIMESTAMP);
//
// 		// 4. 데이터 파싱하여 구조체에 담기
// 		FCraneStateData CraneData;
// 		CraneData.CraneId = JsonParser->GetString(JsonParser->JsonParseKeyword(ContentObj, TEXT("VEHICLE_CD"))).TrimStartAndEnd();
// 		CraneData.Position.TrolleyPosition = static_cast<float>(
// 			JsonParser->GetNumber(JsonParser->JsonParseKeyword(ContentObj, TEXT("TROLLEY_POS")))
// 		);
// 		CraneData.Position.HoistHeight = static_cast<float>(
// 			JsonParser->GetNumber(JsonParser->JsonParseKeyword(ContentObj, TEXT("HOIST_HEIGHT")))
// 		);
// 		CraneData.Position.GantryPosition = static_cast<float>(
// 			JsonParser->GetNumber(JsonParser->JsonParseKeyword(ContentObj, TEXT("GANTRY_POS")))
// 		);
// 		// ... 필요한 필드 추가 파싱
//
// 		if (const UWorld* World = GetWorld())
// 		{
// 			if (const UGameInstance* GI = World->GetGameInstance())
// 			{
// 				if (UDxProcessSubsystem* DxProcessSubsystem = GI->GetSubsystem<UDxProcessSubsystem>())
// 				{
// 					if (UCraneDataSyncComp* CraneDataSyncComp = Cast<UCraneDataSyncComp>(DxProcessSubsystem->FindComponent(CraneData.CraneId)))
// 					{
// 						CraneDataSyncComp->OnReceiveData(EDxDataType::CraneState, &CraneData);
// 					}
// 					else
// 					{
// 						UE_LOG(LogM7AT10, Warning, TEXT("[KP1D0012] CraneDataSyncComp not found for CraneId: %s"), *CraneData.CraneId);
// 					}
// 				}
// 			}
// 		}
// 	}
// }

// Fill out your copyright notice in the Description page of Project Settings.


#include "KP1D0012.h"

#include "m7at10_dt/M7AT10/Lib/YyJsonParser.h"

UKP1D0012::UKP1D0012()
{
	TransactionCode = "KP1D0012";
}

void UKP1D0012::ProcessData(const TSharedPtr<FJsonObject>& DataMap)
{
	Super::ProcessData(DataMap);

	UE_LOG(LogTemp, Log, TEXT("KP1D0012  "));
}

void UKP1D0012::ProcessData(FYyJsonParser* JsonParser, yyjson_val* RootNode)
{
	Super::ProcessData(JsonParser, RootNode);

	// 구조:
	// {
	//   "CREATE_TIMESTAMP": "20251124215148",
	//   "DATA_MAP": {
	//     "20251124215148": { "RTC": {...}, ... }
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

		if (JsonParser->IsValid(ContentObj))
		{
			// 4. RTC 데이터 파싱
			yyjson_val* RtcObj = JsonParser->JsonParseKeyword(ContentObj, TEXT("RTC"));
			if (JsonParser->IsValid(RtcObj))
			{
				FString RtcCar1 = JsonParser->GetString(JsonParser->JsonParseKeyword(RtcObj, TEXT("RTC_CAR1")));
				FString RtcCar2 = JsonParser->GetString(JsonParser->JsonParseKeyword(RtcObj, TEXT("RTC_CAR2")));

				UE_LOG(LogTemp, Log, TEXT("[KP1D0012] RTC - CAR1: %s, CAR2: %s"), *RtcCar1, *RtcCar2);
			}

			// 5. BED 데이터 파싱
			yyjson_val* BedObj = JsonParser->JsonParseKeyword(ContentObj, TEXT("BED"));
			if (JsonParser->IsValid(BedObj))
			{
				FString BedRt = JsonParser->GetString(JsonParser->JsonParseKeyword(BedObj, TEXT("BED_RT")));
				UE_LOG(LogTemp, Log, TEXT("[KP1D0012] BED - RT: %s"), *BedRt);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[KP1D0012] Content object not found for key: %s"), *TimestampKey);
		}
	}
}

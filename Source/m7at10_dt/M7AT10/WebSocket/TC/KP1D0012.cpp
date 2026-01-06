// Fill out your copyright notice in the Description page of Project Settings.


#include "KP1D0012.h"

#include "Kismet/GameplayStatics.h"
#include "m7at10_dt/m7at10_dt.h"
#include "Core/DxDataType.h"
#include "Core/DxProcessSubsystem.h"
#include "m7at10_dt/M7AT10/Crane/CraneDataSyncComp.h"
#include "m7at10_dt/M7AT10/Crane/CraneDataTypes.h"
#include "Lib/YyJsonParser.h"

UKP1D0012::UKP1D0012()
{
	TransactionCode = "KP1D0012";
}

TSharedPtr<FTransactionCodeDataBase> UKP1D0012::ParseToStruct(const FString& JsonString) const
{
	TSharedPtr<FKP1D0012Data> KP1D0012Data = MakeShared<FKP1D0012Data>();

	// yyjson 파싱
	FYyJsonParser Parser;
	if (Parser.JsonParse(JsonString))
	{
		yyjson_val* Root = Parser.GetRoot();

		yyjson_val* TimestampVal = Parser.JsonParseKeyword(Root, TEXT("CREATE_TIMESTAMP"));
		FString TimestampKey = Parser.GetString(TimestampVal);

		yyjson_val* DataMapObj = Parser.JsonParseKeyword(Root, TEXT("DATA_MAP"));
		yyjson_val* ContentObj = Parser.JsonParseKeyword(DataMapObj, TimestampKey);

		KP1D0012Data->EqpBnoEqpSnum = Parser.GetString(Parser.JsonParseKeyword(ContentObj, TEXT("EQP_BNO_EQP_SNUM"))).TrimStartAndEnd();
		KP1D0012Data->VehicleCd = Parser.GetString(Parser.JsonParseKeyword(ContentObj, TEXT("VEHICLE_CD"))).TrimStartAndEnd();
		KP1D0012Data->LogiDestLat = Parser.GetString(Parser.JsonParseKeyword(ContentObj, TEXT("LOGI_DEST_LAT"))).TrimStartAndEnd();
		KP1D0012Data->LogiDestLon1 = Parser.GetString(Parser.JsonParseKeyword(ContentObj, TEXT("LOGI_DEST_LON1"))).TrimStartAndEnd();
		KP1D0012Data->LogiDestLon2 = Parser.GetString(Parser.JsonParseKeyword(ContentObj, TEXT("LOGI_DEST_LON2"))).TrimStartAndEnd();
	}

	return KP1D0012Data;
}

void UKP1D0012::ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data)
{
	TSharedPtr<FKP1D0012Data> KP1D0012Data = StaticCastSharedPtr<FKP1D0012Data>(Data);

	if (KP1D0012Data.IsValid())
	{
		TSharedPtr<FCraneStateData> CraneDataPtr = MakeShared<FCraneStateData>();
		CraneDataPtr->CraneId = KP1D0012Data->VehicleCd;
		CraneDataPtr->Position.TrolleyPosition = FCString::Atof(*KP1D0012Data->LogiDestLat);
		CraneDataPtr->Position.HoistHeight = FCString::Atof(*KP1D0012Data->LogiDestLon1);
		CraneDataPtr->Position.GantryPosition = FCString::Atof(*KP1D0012Data->LogiDestLon2);

		if (UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld()))
		{
			if (UDxProcessSubsystem* DxProcess = GI->GetSubsystem<UDxProcessSubsystem>())
			{
				if (UCraneDataSyncComp* CraneDataSyncComp = Cast<UCraneDataSyncComp>(DxProcess->FindComponent(CraneDataPtr->CraneId)))
				{
					CraneDataSyncComp->OnReceiveData(CraneDataPtr);
				}
				else
				{
					UE_LOG(LogM7AT10, Warning, TEXT("[KP1D0012] CraneDataSyncComp not found for CraneId: %s"), *CraneDataPtr->CraneId);
				}
			}
		}
	}
}

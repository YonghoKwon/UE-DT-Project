// Fill out your copyright notice in the Description page of Project Settings.


#include "CraneInfoGetByBittInOracle.h"

#include "m7at10_dt/m7at10_dt.h"
#include "m7at10_dt/M7AT10/Lib/YyJsonParser.h"

UCraneInfoGetByBittInOracle::UCraneInfoGetByBittInOracle()
{
	ResourceAndAction = "CraneInfoGetByBittInOracle";
}

TSharedPtr<FApiDataBase> UCraneInfoGetByBittInOracle::ParseToStruct(const FString& JsonString)
{
	TSharedPtr<FCraneInfoGetByBittInOracleData> CraneInfoGetByBittInOracleData = MakeShared<FCraneInfoGetByBittInOracleData>();

	// yyjson 파싱
	FYyJsonParser Parser;
	if (Parser.JsonParse(JsonString))
	{
		yyjson_val* Root = Parser.GetRoot();
		yyjson_val* MetaNode = Parser.JsonParseKeyword(Root, TEXT("data"));

		FString name = Parser.GetString(Parser.JsonParseKeyword(MetaNode, TEXT("name")));
		FString email = Parser.GetString(Parser.JsonParseKeyword(MetaNode, TEXT("email")));
	}

	return CraneInfoGetByBittInOracleData;
}

void UCraneInfoGetByBittInOracle::ProcessStructData(const TSharedPtr<FApiDataBase>& Data)
{
	TSharedPtr<FCraneInfoGetByBittInOracleData> CraneInfoGetByBittInOracleData = StaticCastSharedPtr<FCraneInfoGetByBittInOracleData>(Data);

	if (CraneInfoGetByBittInOracleData.IsValid())
	{
		// Actor의 ActorComponent에 데이터 넘겨줌
	}
}

// void UCraneInfoGetByBittInOracle::ProcessData(FYyJsonParser* JsonParser, yyjson_val* RootNode)
// {
// 	Super::ProcessData(JsonParser, RootNode);
//
// 	if (RootNode)
// 	{
// 		// 이 함수는 내부적으로 메모리를 할당하므로, 사용 후 free()를 호출
// 		char* JsonString = yyjson_val_write(RootNode, 0, nullptr);
// 		if (JsonString)
// 		{
// 			UE_LOG(LogM7AT10, Warning, TEXT("RootNode Content: %s"), UTF8_TO_TCHAR(JsonString));
//
// 			// yyjson_val_write가 할당한 메모리 해제
// 			free(JsonString);
// 		}
// 		else
// 		{
// 			UE_LOG(LogM7AT10, Error, TEXT("Failed to convert RootNode to string."));
// 		}
// 	}
// 	else
// 	{
// 		UE_LOG(LogM7AT10, Warning, TEXT("RootNode is null."));
// 	}
// }

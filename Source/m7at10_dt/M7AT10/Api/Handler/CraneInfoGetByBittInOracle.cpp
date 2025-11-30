// Fill out your copyright notice in the Description page of Project Settings.


#include "CraneInfoGetByBittInOracle.h"

UCraneInfoGetByBittInOracle::UCraneInfoGetByBittInOracle()
{
	ResourceAndAction = "CraneInfoGetByBittInOracle";
}

void UCraneInfoGetByBittInOracle::ProcessData(FYyJsonParser* JsonParser, yyjson_val* RootNode)
{
	Super::ProcessData(JsonParser, RootNode);

	if (RootNode)
	{
		// 이 함수는 내부적으로 메모리를 할당하므로, 사용 후 free()를 호출
		char* JsonString = yyjson_val_write(RootNode, 0, nullptr);
		if (JsonString)
		{
			UE_LOG(LogTemp, Warning, TEXT("RootNode Content: %s"), UTF8_TO_TCHAR(JsonString));

			// yyjson_val_write가 할당한 메모리 해제
			free(JsonString);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to convert RootNode to string."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("RootNode is null."));
	}
}

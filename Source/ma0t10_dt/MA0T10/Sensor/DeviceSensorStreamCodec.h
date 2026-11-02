#pragma once
#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Contracts/DeviceSensorStreamData.h"
/** Stateless parsers shared by DTCore TC handlers and the raw transport worker. */
namespace DeviceSensorStreamCodec
{
    MA0T10_DT_API TSharedPtr<FTransactionCodeDataBase> ParseCameraJson(const FString& JsonString);
    MA0T10_DT_API TSharedPtr<FTransactionCodeDataBase> ParseLidarJson(const FString& JsonString);
    MA0T10_DT_API TSharedPtr<FTransactionCodeDataBase> ParsePointCloudJson(const FString& JsonString);
    MA0T10_DT_API TSharedPtr<FTransactionCodeDataBase> ParseBinaryPcd(const TArray<uint8>& Body,const TMap<FName,FString>& Headers);
}

#include "Sensor/VirtualSensorDataTransportComp.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

UVirtualSensorDataTransportComp::UVirtualSensorDataTransportComp()
{
    PrimaryComponentTick.bCanEverTick = false;
}

FVirtualSensorTransportResult UVirtualSensorDataTransportComp::SendJson(const FString& SensorId, const FString& SensorType, const FString& JsonText)
{
    FVirtualSensorTransportResult Result;
    Result.DataLength = JsonText.Len();

    if (TransportMode == EVirtualSensorTransportMode::None)
    {
        Result.bSubmitted = true;
        Result.Message = TEXT("Transport disabled");
    }
    else if (TransportMode == EVirtualSensorTransportMode::SaveToFile)
    {
        Result = SaveJson(SensorId, SensorType, JsonText);
    }
    else
    {
        Result.bSubmitted = true;
        Result.Message = FString::Printf(TEXT("[%s:%s] dataLength=%d"), *SensorType, *SensorId, JsonText.Len());
        UE_LOG(LogTemp, Log, TEXT("%s"), *Result.Message);
    }

    OnDataSent.Broadcast(Result);
    return Result;
}

FVirtualSensorTransportResult UVirtualSensorDataTransportComp::SendHttp(const FString& SensorId, const FString& SensorType, const FString& JsonText) const
{
    FVirtualSensorTransportResult Result;
    Result.DataLength = JsonText.Len();
    Result.Message = TEXT("HTTP mode is reserved for project integration");
    return Result;
}

FVirtualSensorTransportResult UVirtualSensorDataTransportComp::SaveJson(const FString& SensorId, const FString& SensorType, const FString& JsonText) const
{
    FVirtualSensorTransportResult Result;
    Result.DataLength = JsonText.Len();

    const FString SafeSensorId = SensorId.IsEmpty() ? TEXT("UNKNOWN_SENSOR") : SensorId;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), SaveSubDirectory, SafeSensorId);
    IFileManager::Get().MakeDirectory(*Directory, true);

    const FString Timestamp = FString::Printf(TEXT("%s_%lld"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FDateTime::UtcNow().GetTicks());
    const FString FileName = FString::Printf(TEXT("%s_%s_%s.json"), *SafeSensorId, *SensorType, *Timestamp);
    const FString Path = FPaths::Combine(Directory, FileName);

    Result.bSubmitted = FFileHelper::SaveStringToFile(JsonText, *Path);
    Result.SavedFilePath = Path;
    Result.Message = Result.bSubmitted ? FString::Printf(TEXT("Saved: %s"), *Path) : FString::Printf(TEXT("Save failed: %s"), *Path);
    return Result;
}

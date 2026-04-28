#include "m7at10_dt/M7AT10/Sensor/VirtualSensorDataTransportComp.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
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
    else if (TransportMode == EVirtualSensorTransportMode::HttpPost)
    {
        Result = SendHttp(SensorId, SensorType, JsonText);
    }
    else
    {
        Result.bSubmitted = true;
        Result.Message = FString::Printf(TEXT("[%s:%s] dataLength=%d"), *SensorType, *SensorId, JsonText.Len());
        UE_LOG(LogTemp, Log, TEXT("%s"), *Result.Message);
    }

    LastResult = Result;
    OnDataSent.Broadcast(Result);
    return Result;
}

FVirtualSensorTransportResult UVirtualSensorDataTransportComp::SendBinary(const FString& SensorId, const FString& SensorType, const FString& Extension, const TArray<uint8>& Bytes)
{
    FVirtualSensorTransportResult Result;
    Result.DataLength = Bytes.Num();

    if (TransportMode == EVirtualSensorTransportMode::None)
    {
        Result.bSubmitted = true;
        Result.Message = TEXT("Transport disabled");
    }
    else if (TransportMode == EVirtualSensorTransportMode::SaveToFile)
    {
        Result = SaveBinary(SensorId, SensorType, Extension, Bytes);
    }
    else
    {
        Result.bSubmitted = true;
        Result.Message = FString::Printf(TEXT("[%s:%s] binaryLength=%d"), *SensorType, *SensorId, Bytes.Num());
        UE_LOG(LogTemp, Log, TEXT("%s"), *Result.Message);
    }

    LastResult = Result;
    OnDataSent.Broadcast(Result);
    return Result;
}

FVirtualSensorTransportResult UVirtualSensorDataTransportComp::SendHttp(const FString& SensorId, const FString& SensorType, const FString& JsonText)
{
    FVirtualSensorTransportResult Result;
    Result.DataLength = JsonText.Len();

    if (HttpEndpoint.IsEmpty())
    {
        Result.bSubmitted = false;
        Result.Message = FString::Printf(TEXT("[%s:%s] HttpEndpoint is empty"), *SensorType, *SensorId);
        UE_LOG(LogTemp, Warning, TEXT("%s"), *Result.Message);
        return Result;
    }

    const int32 SubmittedDataLength = JsonText.Len();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(HttpEndpoint);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("X-Sensor-Id"), SensorId);
    Request->SetHeader(TEXT("X-Sensor-Type"), SensorType);
    Request->SetTimeout(static_cast<float>(FMath::Max(1, HttpTimeoutSeconds)));
    Request->SetContentAsString(JsonText);

    Request->OnProcessRequestComplete().BindLambda([this, SensorId, SensorType, SubmittedDataLength](FHttpRequestPtr RequestPtr, FHttpResponsePtr Response, bool bSucceeded)
    {
        FVirtualSensorTransportResult CallbackResult;
        CallbackResult.bSubmitted = bSucceeded && Response.IsValid();
        CallbackResult.DataLength = SubmittedDataLength;
        CallbackResult.HttpStatusCode = Response.IsValid() ? Response->GetResponseCode() : 0;
        CallbackResult.Message = FString::Printf(TEXT("[%s:%s] HTTP completed success=%s code=%d"),
            *SensorType,
            *SensorId,
            CallbackResult.bSubmitted ? TEXT("true") : TEXT("false"),
            CallbackResult.HttpStatusCode);

        if (bLogHttpResponse)
        {
            UE_LOG(LogTemp, Log, TEXT("%s"), *CallbackResult.Message);
        }

        LastResult = CallbackResult;
        OnDataSent.Broadcast(CallbackResult);
    });

    Result.bSubmitted = Request->ProcessRequest();
    Result.Message = Result.bSubmitted
        ? FString::Printf(TEXT("[%s:%s] HTTP request submitted"), *SensorType, *SensorId)
        : FString::Printf(TEXT("[%s:%s] HTTP request submit failed"), *SensorType, *SensorId);
    return Result;
}

FVirtualSensorTransportResult UVirtualSensorDataTransportComp::SaveJson(const FString& SensorId, const FString& SensorType, const FString& JsonText) const
{
    FVirtualSensorTransportResult Result;
    Result.DataLength = JsonText.Len();
    const FString Path = BuildSavePath(SensorId, SensorType, TEXT("json"));
    Result.bSubmitted = FFileHelper::SaveStringToFile(JsonText, *Path);
    Result.SavedFilePath = Path;
    Result.Message = Result.bSubmitted ? FString::Printf(TEXT("Saved: %s"), *Path) : FString::Printf(TEXT("Save failed: %s"), *Path);
    return Result;
}

FVirtualSensorTransportResult UVirtualSensorDataTransportComp::SaveBinary(const FString& SensorId, const FString& SensorType, const FString& Extension, const TArray<uint8>& Bytes) const
{
    FVirtualSensorTransportResult Result;
    Result.DataLength = Bytes.Num();
    const FString Path = BuildSavePath(SensorId, SensorType, Extension.IsEmpty() ? TEXT("bin") : Extension);
    Result.bSubmitted = FFileHelper::SaveArrayToFile(Bytes, *Path);
    Result.SavedFilePath = Path;
    Result.Message = Result.bSubmitted ? FString::Printf(TEXT("Saved: %s"), *Path) : FString::Printf(TEXT("Save failed: %s"), *Path);
    return Result;
}

FString UVirtualSensorDataTransportComp::BuildSavePath(const FString& SensorId, const FString& SensorType, const FString& Extension) const
{
    const FString SafeSensorId = SensorId.IsEmpty() ? TEXT("UNKNOWN_SENSOR") : SensorId;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), SaveSubDirectory, SafeSensorId);
    IFileManager::Get().MakeDirectory(*Directory, true);

    const FString Timestamp = FString::Printf(TEXT("%s_%lld"), *FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")), FDateTime::UtcNow().GetTicks());
    const FString FileName = FString::Printf(TEXT("%s_%s_%s.%s"), *SafeSensorId, *SensorType, *Timestamp, *Extension);
    return FPaths::Combine(Directory, FileName);
}

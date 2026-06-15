#pragma once

#include "CoreMinimal.h"
#include "HttpResultCallback.h"
#include "HttpRouteHandle.h"
#include "LidarJsonLiveSourceComp.h"
#include "LidarHttpJsonLiveSourceComp.generated.h"

class IHttpRouter;
struct FHttpServerRequest;

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class M7AT10_DT_API ULidarHttpJsonLiveSourceComp : public ULidarJsonLiveSourceComp
{
    GENERATED_BODY()

public:
    ULidarHttpJsonLiveSourceComp();

    virtual bool StartSource() override;
    virtual void StopSource() override;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorLive|HTTP")
    bool ProcessHttpPayloadJson(const FString& PayloadJson);

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|HTTP", meta = (ClampMin = "1", ClampMax = "65535"))
    int32 ListenPort = 8082;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|HTTP")
    FString RoutePath = TEXT("/m7at10/lidar/live");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|HTTP", meta = (ClampMin = "1024", ClampMax = "104857600"))
    int32 MaxRequestBytes = 1048576;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|HTTP")
    bool bAutoPushReceivedFrame = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|HTTP")
    bool bSendTransportForReceivedFrames = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorLive|HTTP|Status")
    int32 LastReceivedRequestBytes = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorLive|HTTP|Status")
    int32 LastResponseCode = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorLive|HTTP|Status")
    FString LastHttpMessage;

private:
    bool HandleHttpRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
    void CloseHttpRoute();
    FString NormalizeRoutePath() const;

private:
    TSharedPtr<IHttpRouter> HttpRouter;
    FHttpRouteHandle RouteHandle;
};

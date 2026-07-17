#pragma once

#include "CoreMinimal.h"
#include "HttpResultCallback.h"
#include "HttpRouteHandle.h"
#include "LidarJsonLiveSourceComponent.h"
#include "LidarHttpJsonLiveSourceComponent.generated.h"

class IHttpRouter;
struct FHttpServerRequest;

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API ULidarHttpJsonLiveSourceComponent : public ULidarJsonLiveSourceComponent
{
    GENERATED_BODY()

public:
    ULidarHttpJsonLiveSourceComponent();

    virtual bool StartSource() override;
    virtual void StopSource() override;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorLive|HTTP")
    bool ProcessHttpPayloadJson(const FString& PayloadJson);

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|RealSensorLive|HTTP")
    bool IsHttpRouteBound() const { return RouteHandle.IsValid(); }

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|RealSensorLive|HTTP")
    bool IsAcceptingHttpRequests() const { return bAcceptingHttpRequests; }

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|HTTP", meta = (ClampMin = "1", ClampMax = "65535"))
    int32 ListenPort = 8082;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|HTTP")
    FString RoutePath = TEXT("/ma0t10/lidar/live");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|HTTP", meta = (ClampMin = "1024", ClampMax = "104857600"))
    int32 MaxRequestBytes = 1048576;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|HTTP")
    bool bAutoPushReceivedFrame = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|HTTP")
    bool bSendTransportForReceivedFrames = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorLive|HTTP|Status")
    bool bLastRequestProcessedOnGameThread = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorLive|HTTP|Status")
    bool bAcceptingHttpRequests = false;

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
    int32 ActiveRequestGeneration = 0;
};

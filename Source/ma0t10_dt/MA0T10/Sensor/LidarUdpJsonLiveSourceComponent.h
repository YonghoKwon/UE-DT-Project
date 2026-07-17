#pragma once

#include "CoreMinimal.h"
#include "Common/UdpSocketReceiver.h"
#include "LidarJsonLiveSourceComponent.h"
#include "LidarUdpJsonLiveSourceComponent.generated.h"

class FSocket;
struct FIPv4Endpoint;

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API ULidarUdpJsonLiveSourceComponent : public ULidarJsonLiveSourceComponent
{
    GENERATED_BODY()

public:
    ULidarUdpJsonLiveSourceComponent();

    virtual bool StartSource() override;
    virtual void StopSource() override;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorLive|UDP")
    bool ProcessUdpPayloadJson(const FString& PayloadJson);

    UFUNCTION(BlueprintPure, Category = "DigitalTwin|RealSensorLive|UDP")
    int32 GetBoundPort() const { return BoundPort; }

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|UDP")
    FString BindAddress = TEXT("127.0.0.1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|UDP", meta = (ClampMin = "0", ClampMax = "65535"))
    int32 BindPort = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|UDP", meta = (ClampMin = "1024", ClampMax = "1048576"))
    int32 MaxDatagramBytes = 65507;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|UDP")
    bool bAutoPushReceivedFrame = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|UDP")
    bool bSendTransportForReceivedFrames = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorLive|UDP|Status")
    int32 LastReceivedDatagramBytes = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorLive|UDP|Status")
    FString LastReceivedEndpoint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorLive|UDP|Status")
    FString LastUdpMessage;

private:
    void HandleReceivedData(const FArrayReaderPtr& Data, const FIPv4Endpoint& Endpoint);
    void CloseUdpSocket();

private:
    FSocket* ListenSocket = nullptr;
    TUniquePtr<FUdpSocketReceiver> SocketReceiver;
    int32 BoundPort = 0;
};

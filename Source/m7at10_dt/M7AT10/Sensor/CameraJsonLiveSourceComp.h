#pragma once

#include "CoreMinimal.h"
#include "RealSensorSourceComp.h"
#include "CameraJsonLiveSourceComp.generated.h"

class UVirtualCameraComp;

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class M7AT10_DT_API UCameraJsonLiveSourceComp : public URealSensorSourceComp
{
    GENERATED_BODY()

public:
    UCameraJsonLiveSourceComp();

    virtual bool StartSource() override;
    virtual void StopSource() override;
    virtual bool PushFrameOnce(bool bSendTransport = true) override;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorLive|Camera")
    bool AppendLivePayloadJson(const FString& PayloadJson);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorLive|Camera")
    void ClearBufferedFrame();

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|Camera")
    TObjectPtr<UVirtualCameraComp> TargetCamera;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive|Camera")
    bool bClearBufferAfterPush = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorLive|Camera|Status")
    bool bHasBufferedPayload = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorLive|Camera|Status")
    int32 LastPayloadLength = 0;

private:
    UVirtualCameraComp* ResolveTargetCamera() const;

private:
    UPROPERTY()
    FString BufferedPayloadJson;
};

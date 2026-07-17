#pragma once

#include "CoreMinimal.h"
#include "RealSensorSourceComponent.h"
#include "VirtualLidarSensorTypes.h"
#include "LidarJsonLiveSourceComponent.generated.h"

UCLASS(ClassGroup = (DTCore), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API ULidarJsonLiveSourceComponent : public URealSensorSourceComponent
{
    GENERATED_BODY()

public:
    ULidarJsonLiveSourceComponent();

    virtual bool StartSource() override;

    virtual void StopSource() override;

    virtual bool PushFrameOnce(bool bSendTransport = true) override;

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorLive")
    void QueuePushFrameOnce(bool bSendTransport = true);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorLive")
    void AppendJsonLine(const FString& JsonLine);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorLive")
    void AppendJsonLines(const FString& JsonLines);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorLive")
    bool AppendWebSocketPayload(const FString& PayloadJson);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorLive")
    bool AppendLivePayloadJson(const FString& PayloadJson);

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorLive")
    void ClearBufferedFrame();

    UFUNCTION(CallInEditor, Category = "DigitalTwin|RealSensorLive")
    void AppendSampleWebSocketFrameInEditor();

    UFUNCTION(CallInEditor, Category = "DigitalTwin|RealSensorLive")
    void PushBufferedFrameInEditor();

    UFUNCTION(CallInEditor, Category = "DigitalTwin|RealSensorLive")
    void PushBufferedFrameNoTransportInEditor();

    UFUNCTION(CallInEditor, Category = "DigitalTwin|RealSensorLive")
    void ClearBufferedFrameInEditor();

    UFUNCTION(BlueprintCallable, Category = "DigitalTwin|RealSensorLive")
    bool BuildFrameFromBufferedLines(TArray<FVirtualLidarPoint>& OutPoints) const;

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive")
    bool bClearBufferAfterPush = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive")
    bool bSkipMissPoints = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive", meta = (ClampMin = "0", ClampMax = "10000000"))
    int32 MaxPointsPerFrame = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive")
    FName DefaultSemanticLabel = TEXT("Unknown");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DigitalTwin|RealSensorLive", meta = (FilePathFilter = "json", RelativeToGameDir))
    FString SampleWebSocketPayloadPath = TEXT("Samples/websocket/lidar_json_live_frame_sample.json");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorLive|Status")
    int32 PendingLineCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorLive|Status")
    int32 LastParsedLineCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|RealSensorLive|Status")
    int32 LastDroppedLineCount = 0;

private:
    bool ParseJsonPointLine(const FString& Line, FVirtualLidarPoint& OutPoint) const;
    void AppendNormalizedLine(const FString& JsonLine);
    FString ResolveSampleWebSocketPayloadPath() const;

private:
    UPROPERTY()
    FString BufferedJsonLines;
};

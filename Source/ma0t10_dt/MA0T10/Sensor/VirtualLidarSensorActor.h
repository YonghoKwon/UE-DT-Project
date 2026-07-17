#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "VirtualLidarSensorActor.generated.h"

class UArrowComponent;
class UTexture2D;
class UVirtualLidarScanComponent;
class UVirtualLidarAnalysisComponent;
class UVirtualLidarExportComponent;
class UVirtualLidarVisualizationComponent;

UCLASS()
class MA0T10_DT_API AVirtualLidarSensorActor : public AVirtualSensorActorBase
{
    GENERATED_BODY()

public:
    AVirtualLidarSensorActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar")
    TObjectPtr<UVirtualLidarScanComponent> ScanComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar")
    TObjectPtr<UVirtualLidarAnalysisComponent> AnalysisComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar")
    TObjectPtr<UVirtualLidarVisualizationComponent> VisualizationComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|VirtualLidar")
    TObjectPtr<UVirtualLidarExportComponent> ExportComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DigitalTwin|EditorVisualization")
    TObjectPtr<UArrowComponent> EditorForwardArrowComp;

protected:
    virtual void BeginPlay() override;

public:
    virtual FString GetSensorId() const override;
    virtual EVirtualSensorKind GetSensorKind() const override;
    virtual bool IsSensorRunning() const override;
    virtual void StartSensor() override;
    virtual void StopSensor() override;
    virtual void CaptureSensorOnce() override;
    virtual UTexture* GetSensorPreviewTexture() const override;
    virtual FVirtualSensorRuntimeStatus GetSensorRuntimeStatus() const override;
    virtual bool SubmitExternalFrame(const FVirtualSensorFrameEnvelope& Frame, bool bSendTransport) override;
    virtual bool ReadEditableState(FVirtualSensorEditableState& OutState) const override;
    virtual bool ValidateEditableState(const FVirtualSensorEditableState& State, FString& OutError) const override;
    virtual bool ApplyEditableState(const FVirtualSensorEditableState& State, FString& OutError) override;
    virtual bool ApplyProfileAndSimulationQuality(const FVirtualSensorEditableState& RequestedState, FVirtualSensorEditableState& OutAppliedState, FString& OutError) override;

private:
    UFUNCTION()
    void HandleLidarFrame(const FString& JsonPayload, UTexture2D* ViewTexture);

    TOptional<bool> PendingExternalSendTransport;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VirtualCameraPayloadCodec.h"
#include "VirtualCameraGroundTruth.generated.h"

class UVirtualCameraCaptureComponent;
class UTexture2D;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class FVirtualLidarSemanticScene;
class FRHIGPUTextureReadback;
struct FVirtualCameraTruthWork;

struct MA0T10_DT_API FVirtualCameraIntrinsics
{
    int32 Width=0, Height=0;
    double Fx=0, Fy=0, Cx=0, Cy=0;
    static FVirtualCameraIntrinsics Make(int32 Width, int32 Height, double HorizontalFov);
};
struct MA0T10_DT_API FVirtualCameraTruthBox
{
    uint32 Instance=0;
    uint8 Class=0;
    int32 XMin=MAX_int32, YMin=MAX_int32, XMax=-1, YMax=-1, Pixels=0;
};
struct MA0T10_DT_API FVirtualCameraTruthFrame
{
    FVirtualCameraPayloadSnapshot Acquisition;
    FVirtualCameraIntrinsics Intrinsics;
    TArray<float> DepthMeters; // optical-axis Z, 0 invalid; never display-normalized
    TArray<uint8> Classes;
    TArray<uint32> Instances; // session-stable Actor ID, 0 invalid
    TArray<FColor> DatasetRgb; // only explicit DATASET-01 requests; original RGB target bytes
    TMap<uint32,FString> ActorPaths;
    TArray<FVirtualCameraTruthBox> Boxes; // visible mask bounds, inclusive pixel indices
    double SubmitMs=0, ReadbackLatencyMs=0, DecodeMs=0;
    int32 UnclassifiedPixels=0;
    static TArray<FVirtualCameraTruthBox> FindBoxes(const TArray<uint8>& Classes, const TArray<uint32>& Instances, int32 Width, int32 Height);
};

/** Lazy, bounded opt-in capture. No transport, registration loop or source geometry writes. */
UCLASS()
class MA0T10_DT_API UVirtualCameraGroundTruth : public UActorComponent
{
    GENERATED_BODY()
public:
    UVirtualCameraGroundTruth();
    virtual ~UVirtualCameraGroundTruth() override;
    bool Capture(UVirtualCameraCaptureComponent& Source, const FVirtualCameraPayloadSnapshot& Snapshot, bool bSave, bool bDataset=false, bool bIncludeTruth=true);
    void Cancel();
    void Poll();
    UTexture2D* Preview(uint8 Mode) const;
    TSharedPtr<const FVirtualCameraTruthFrame,ESPMode::ThreadSafe> LastFrame;
    FString Status;
    FString LastSavedDirectory;
    bool IsBusy() const { return Work.IsValid(); }
    bool IsSaving() const;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    bool HasTruthResourcesForTests() const { return DepthCapture!=nullptr||SemanticScene.IsValid(); }
    int32 GetReadbackAllocationsForTests() const { return ReadbackAllocations; }
#endif
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void OnUnregister() override;
private:
    UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> DepthCapture;
    UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> DepthTarget;
    UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> PreviewTextures;
    TUniquePtr<FVirtualLidarSemanticScene> SemanticScene;
    TSharedPtr<FVirtualCameraTruthWork,ESPMode::ThreadSafe> Work;
    TMap<TWeakObjectPtr<AActor>,uint32> ActorIds;
    uint32 NextActorId=1;
    FTimerHandle PollTimer;
    void Complete(const TSharedPtr<FVirtualCameraTruthWork,ESPMode::ThreadSafe>& Completed);
    TSharedPtr<FRHIGPUTextureReadback,ESPMode::ThreadSafe> CachedDepth,CachedSemantic,CachedRgb;
    FIntPoint CachedSize=FIntPoint::ZeroValue;
    int32 ReadbackAllocations=0;
    void ReleaseReadbackCache();
};

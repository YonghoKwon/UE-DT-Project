#pragma once
#include "CoreMinimal.h"
#include "Components/LineBatchComponent.h"
#include "LidarPlayerDisplayComponent.generated.h"

/** Ordinary player-view primitives, including restricted viewport rectangles. */
UCLASS(NotBlueprintable,Transient)
class MA0T10_DT_API ULidarPlayerDisplayComponent : public ULineBatchComponent
{
    GENERATED_BODY()
public:
    /** Opt-in fixed GPU buffers; zero-size slots have zero area and never become returns. */
    bool bFixedPointSubmission=false;
    FVector PointViewOrigin=FVector::ZeroVector,PointViewRight=FVector::RightVector,PointViewUp=FVector::UpVector,PointViewForward=FVector::ForwardVector;
    double PointPixelScale=1./1920,LastPointViewUpdate=-1;
    void RefreshPointView();
    /** Hatch opt-in: update existing sprite buffers without replacing the proxy. */
    void CommitPointData();
    uint64 GetFixedPointProxyCreations() const { return FixedPointProxyCreations.Load(); }
    uint64 GetFixedPointUploads() const { return FixedPointUploads.Load(); }
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    /** Explicit boundary-only audit: vertex/index buffer count,SRV count,bytes. */
    FIntVector GetFixedPointRhiResources() const;
#endif
    virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials,bool bGetDebugMaterials=false) const override;
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
protected:
    virtual void SendRenderDynamicData_Concurrent() override;
private:
    int32 FixedProxyPointCount=0;
    TAtomic<uint64> FixedPointProxyCreations{0},FixedPointUploads{0};
};

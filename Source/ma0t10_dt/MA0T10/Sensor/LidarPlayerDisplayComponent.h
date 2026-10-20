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
    virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials,bool bGetDebugMaterials=false) const override;
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
};

#include "LidarPlayerDisplayComponent.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "SceneManagement.h"

namespace
{
class FPlayerDisplayProxy final:public FPrimitiveSceneProxy
{
    const TArray<FBatchedPoint> Points;
    const TArray<FBatchedLine> Lines;
public:
    explicit FPlayerDisplayProxy(const ULidarPlayerDisplayComponent* C)
        :FPrimitiveSceneProxy(C),Points(C->BatchedPoints),Lines(C->BatchedLines){bWillEverBeLit=false;}
    SIZE_T GetTypeHash() const override {static char Key;return reinterpret_cast<SIZE_T>(&Key);}
    uint32 GetMemoryFootprint() const override {return sizeof(*this)+Points.GetAllocatedSize()+Lines.GetAllocatedSize();}
    FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
    {FPrimitiveViewRelevance R;R.bDrawRelevance=IsShown(View);R.bDynamicRelevance=true;R.bNormalTranslucency=R.bSeparateTranslucency=true;return R;}
    void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,const FSceneViewFamily&,uint32 Mask,FMeshElementCollector& Collector) const override
    {
        for(int32 I=0;I<Views.Num();++I)if(Mask&(1u<<I))
        {
            // Use the ordinary PDI in Editor/PIE too. Debug PDI has its own view
            // path and can replace a restricted player-view projection.
            auto* PDI=Collector.GetPDI(I);
            for(const auto& P:Points)if(P.PointSize>0&&P.Color.A>0)PDI->DrawPoint(P.Position,P.Color,P.PointSize,P.DepthPriority);
            for(const auto& L:Lines)PDI->DrawLine(L.Start,L.End,L.Color,L.DepthPriority,L.Thickness,0,true);
        }
    }
};
}
FPrimitiveSceneProxy* ULidarPlayerDisplayComponent::CreateSceneProxy(){return new FPlayerDisplayProxy(this);}

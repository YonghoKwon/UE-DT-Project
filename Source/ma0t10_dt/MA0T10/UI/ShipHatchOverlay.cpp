#include "ShipHatchOverlay.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "StaticMeshResources.h"
#include "SceneInterface.h"

namespace
{
struct FHatchPlaneBuffers
{
    FStaticMeshVertexBuffers Vertices;
    FDynamicMeshIndexBuffer32 Indices;
    FLocalVertexFactory Factory;
    TUniquePtr<FColoredMaterialRenderProxy> Material;
    FHatchPlaneBuffers(const FBatchedMesh& M,ERHIFeatureLevel::Type Level):Factory(Level,"HatchSectionPlane")
    {
        TArray<FDynamicMeshVertex> Data;for(const auto& P:M.MeshVerts){FDynamicMeshVertex V;V.Position=FVector3f(P);V.TextureCoordinate[0]=FVector2f::ZeroVector;V.TangentX=FVector3f(1,0,0);V.TangentZ=FVector3f(0,0,1);V.Color=FColor::White;Data.Add(V);}
        for(int32 I:M.MeshIndices)Indices.Indices.Add(uint32(I));Vertices.InitFromDynamicVertex(&Factory,Data,1);
        BeginInitResource(&Vertices.PositionVertexBuffer);BeginInitResource(&Vertices.StaticMeshVertexBuffer);BeginInitResource(&Vertices.ColorVertexBuffer);BeginInitResource(&Indices);BeginInitResource(&Factory);
        Material=MakeUnique<FColoredMaterialRenderProxy>(GEngine->DebugMeshMaterial->GetRenderProxy(),M.Color);
    }
    ~FHatchPlaneBuffers(){Vertices.PositionVertexBuffer.ReleaseResource();Vertices.StaticMeshVertexBuffer.ReleaseResource();Vertices.ColorVertexBuffer.ReleaseResource();Indices.ReleaseResource();Factory.ReleaseResource();}
};
class FHatchOverlayProxy final:public FPrimitiveSceneProxy
{
    const TArray<FBatchedLine> Lines;
    TArray<TUniquePtr<FHatchPlaneBuffers>> Planes;
public:
    explicit FHatchOverlayProxy(const UShipHatchOverlay* C):FPrimitiveSceneProxy(C),Lines(C->BatchedLines)
    {bWillEverBeLit=false;for(const auto& M:C->BatchedMeshes)if(M.MeshVerts.Num()&&M.MeshIndices.Num())Planes.Add(MakeUnique<FHatchPlaneBuffers>(M,GetScene().GetFeatureLevel()));}
    SIZE_T GetTypeHash() const override {static char Key;return reinterpret_cast<SIZE_T>(&Key);}
    uint32 GetMemoryFootprint() const override {return sizeof(*this)+Lines.GetAllocatedSize()+Planes.GetAllocatedSize()+Planes.Num()*sizeof(FHatchPlaneBuffers);}
    FPrimitiveViewRelevance GetViewRelevance(const FSceneView* V) const override
    {FPrimitiveViewRelevance R;R.bDrawRelevance=IsShown(V);R.bDynamicRelevance=true;R.bNormalTranslucency=R.bSeparateTranslucency=true;return R;}
    void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,const FSceneViewFamily&,uint32 Mask,FMeshElementCollector& Collector) const override
    {
        for(int32 I=0;I<Views.Num();++I)if(Mask&(1u<<I))
        {
            auto* PDI=Collector.GetPDI(I);for(const auto& L:Lines)PDI->DrawLine(L.Start,L.End,L.Color,L.DepthPriority,L.Thickness,0,true);
            for(const auto& P:Planes)
            {
                // The section quad reuses immutable vertex/index/material resources
                // across frames. Only an explicit display change replaces the proxy.
                FMeshBatch& Mesh=Collector.AllocateMesh();Mesh.VertexFactory=&P->Factory;Mesh.MaterialRenderProxy=P->Material.Get();Mesh.Type=PT_TriangleList;Mesh.DepthPriorityGroup=SDPG_World;Mesh.bCanApplyViewModeOverrides=false;
                auto& Element=Mesh.Elements[0];Element.IndexBuffer=&P->Indices;Element.FirstIndex=0;Element.NumPrimitives=P->Indices.Indices.Num()/3;Element.MinVertexIndex=0;Element.MaxVertexIndex=P->Vertices.PositionVertexBuffer.GetNumVertices()-1;
                auto& Uniform=Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();Uniform.Set(GetLocalToWorld(),GetLocalToWorld(),GetBounds(),GetLocalBounds(),GetLocalBounds(),false,false,false,GetCustomPrimitiveData());Element.PrimitiveUniformBufferResource=&Uniform.UniformBuffer;
                Collector.AddMesh(I,Mesh);
            }
        }
    }
};
}
FPrimitiveSceneProxy* UShipHatchOverlay::CreateSceneProxy(){return new FHatchOverlayProxy(this);}

#include "LidarPlayerDisplayComponent.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "SceneManagement.h"
#include "StaticMeshResources.h"
#include "DynamicMeshBuilder.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "SceneView.h"
#include "RenderingThread.h"

void ULidarPlayerDisplayComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials,bool bGetDebugMaterials) const
{Super::GetUsedMaterials(OutMaterials,bGetDebugMaterials);if(bFixedPointSubmission&&GEngine&&GEngine->VertexColorMaterial)OutMaterials.AddUnique(GEngine->VertexColorMaterial);}

void ULidarPlayerDisplayComponent::RefreshPointView()
{
    if(!bFixedPointSubmission||!GetWorld())return;
    const double Now=FPlatformTime::Seconds();if(Now-LastPointViewUpdate<.2)return;LastPointViewUpdate=Now;
    auto* PC=GetWorld()->GetFirstPlayerController();auto* Player=PC?PC->GetLocalPlayer():nullptr;auto* Viewport=GetWorld()->GetGameViewport();if(!Player||!Viewport||!Viewport->Viewport)return;
    FSceneViewProjectionData Projection;if(!Player->GetProjectionData(Viewport->Viewport,Projection,INDEX_NONE))return;
    FVector Origin;FRotator Rotation;PC->GetPlayerViewPoint(Origin,Rotation);const FRotationMatrix Axes(Rotation);
    const double Scale=1./FMath::Max(1.,Projection.GetConstrainedViewRect().Width()*Projection.ProjectionMatrix.M[0][0]);
    const FVector Right=Axes.GetUnitAxis(EAxis::Y),Up=Axes.GetUnitAxis(EAxis::Z),Forward=Axes.GetUnitAxis(EAxis::X);
    if(Origin.Equals(PointViewOrigin,.01)&&Right.Equals(PointViewRight,1.e-5)&&Up.Equals(PointViewUp,1.e-5)&&FMath::IsNearlyEqual(Scale,PointPixelScale,1.e-9))return;
    PointViewOrigin=Origin;PointViewRight=Right;PointViewUp=Up;PointViewForward=Forward;PointPixelScale=Scale;CommitPointData();
}

namespace
{
struct FPlayerPointUpdate
{
    TArray<FBatchedPoint> Points;
    FVector Origin,Right,Up,Forward;
    double PixelScale=0;
};
class FPlayerDisplayProxy final:public FPrimitiveSceneProxy
{
    TArray<FBatchedPoint> Points;
    const TArray<FBatchedLine> Lines;
    const bool bFixedPoints;
    FStaticMeshVertexBuffers Vertices;
    FDynamicMeshIndexBuffer32 Indices;
    FLocalVertexFactory Factory;
    const FMaterialRenderProxy* Material=nullptr;
public:
    explicit FPlayerDisplayProxy(const ULidarPlayerDisplayComponent* C)
        :FPrimitiveSceneProxy(C),Points(C->BatchedPoints),Lines(C->BatchedLines),bFixedPoints(C->bFixedPointSubmission&&GEngine->VertexColorMaterial&&Points.Num()>0),Factory(GetScene().GetFeatureLevel(),"OwnedLidarPoints")
    {
        bWillEverBeLit=false;if(!bFixedPoints)return;
        TArray<FDynamicMeshVertex> Data;Data.Reserve(Points.Num()*4);Indices.Indices.Reserve(Points.Num()*6);
        for(const auto& P:Points)
        {
            const double Depth=FVector::DotProduct(P.Position-C->PointViewOrigin,C->PointViewForward);
            const double Size=Depth>0&&P.PointSize>0&&P.Color.A>0?P.PointSize*Depth*C->PointPixelScale:0;
            const FVector X=C->PointViewRight*Size,Y=C->PointViewUp*Size;const uint32 Base=Data.Num();
            for(auto Corner:{FVector2D(-1,-1),FVector2D(1,-1),FVector2D(1,1),FVector2D(-1,1)})
            {
                FDynamicMeshVertex V;V.Position=FVector3f(P.Position+X*Corner.X+Y*Corner.Y);V.TextureCoordinate[0]=FVector2f::ZeroVector;V.TangentX=FVector3f(C->PointViewRight);V.TangentZ=FVector3f(-C->PointViewForward);V.Color=P.Color.ToFColor(true);Data.Add(V);
            }
            Indices.Indices.Append({Base,Base+1,Base+2,Base,Base+2,Base+3});
        }
        Vertices.InitFromDynamicVertex(&Factory,Data,1);BeginInitResource(&Vertices.PositionVertexBuffer);BeginInitResource(&Vertices.StaticMeshVertexBuffer);BeginInitResource(&Vertices.ColorVertexBuffer);BeginInitResource(&Indices);BeginInitResource(&Factory);
        Material=GEngine->VertexColorMaterial->GetRenderProxy();
    }
    ~FPlayerDisplayProxy()
    {if(bFixedPoints){Vertices.PositionVertexBuffer.ReleaseResource();Vertices.StaticMeshVertexBuffer.ReleaseResource();Vertices.ColorVertexBuffer.ReleaseResource();Indices.ReleaseResource();Factory.ReleaseResource();}}
    void UpdatePoints(FRHICommandListImmediate& Cmd,FPlayerPointUpdate&& Data)
    {
        check(IsInRenderingThread());
        if(!bFixedPoints||Data.Points.Num()!=Points.Num())return;
        Points=MoveTemp(Data.Points);
        for(int I=0;I<Points.Num();++I)
        {
            const auto& P=Points[I];const double Depth=FVector::DotProduct(P.Position-Data.Origin,Data.Forward);
            const double Size=Depth>0&&P.PointSize>0&&P.Color.A>0?P.PointSize*Depth*Data.PixelScale:0;
            const FVector X=Data.Right*Size,Y=Data.Up*Size;int CornerIndex=0;
            for(auto Corner:{FVector2D(-1,-1),FVector2D(1,-1),FVector2D(1,1),FVector2D(-1,1)})
            {
                const int V=I*4+CornerIndex++;Vertices.PositionVertexBuffer.VertexPosition(V)=FVector3f(P.Position+X*Corner.X+Y*Corner.Y);
                Vertices.ColorVertexBuffer.VertexColor(V)=P.Color.ToFColor(true);
            }
        }
        // Same UE5.3 buffer update path as procedural meshes. Index buffers,
        // vertex factory and bindings remain alive for the whole display.
        auto Upload=[&](auto& Buffer){const uint32 Bytes=Buffer.GetNumVertices()*Buffer.GetStride();void* Dest=Cmd.LockBuffer(Buffer.VertexBufferRHI,0,Bytes,RLM_WriteOnly);FMemory::Memcpy(Dest,Buffer.GetVertexData(),Bytes);Cmd.UnlockBuffer(Buffer.VertexBufferRHI);};
        Upload(Vertices.PositionVertexBuffer);Upload(Vertices.ColorVertexBuffer);
    }
    SIZE_T GetTypeHash() const override {static char Key;return reinterpret_cast<SIZE_T>(&Key);}
    uint32 GetMemoryFootprint() const override {return sizeof(*this)+Points.GetAllocatedSize()+Lines.GetAllocatedSize();}
    FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
    {FPrimitiveViewRelevance R;R.bDrawRelevance=IsShown(View);R.bDynamicRelevance=true;R.bRenderInMainPass=ShouldRenderInMainPass();R.bOpaque=bFixedPoints;R.bNormalTranslucency=R.bSeparateTranslucency=!bFixedPoints;return R;}
    void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,const FSceneViewFamily&,uint32 Mask,FMeshElementCollector& Collector) const override
    {
        for(int32 I=0;I<Views.Num();++I)if(Mask&(1u<<I))
        {
            auto* PDI=Collector.GetPDI(I);
            if(bFixedPoints)
            {
                FMeshBatch& Mesh=Collector.AllocateMesh();Mesh.VertexFactory=&Factory;Mesh.MaterialRenderProxy=Material;Mesh.Type=PT_TriangleList;Mesh.DepthPriorityGroup=SDPG_World;Mesh.bCanApplyViewModeOverrides=false;Mesh.bDisableBackfaceCulling=true;
                auto& E=Mesh.Elements[0];E.IndexBuffer=&Indices;E.FirstIndex=0;E.NumPrimitives=Indices.Indices.Num()/3;E.MinVertexIndex=0;E.MaxVertexIndex=Vertices.PositionVertexBuffer.GetNumVertices()-1;
                auto& Uniform=Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();Uniform.Set(GetLocalToWorld(),GetLocalToWorld(),GetBounds(),GetLocalBounds(),GetLocalBounds(),false,false,false,GetCustomPrimitiveData());E.PrimitiveUniformBufferResource=&Uniform.UniformBuffer;Collector.AddMesh(I,Mesh);
            }
            else for(const auto& P:Points)if(P.PointSize>0&&P.Color.A>0)PDI->DrawPoint(P.Position,P.Color,P.PointSize,P.DepthPriority);
            for(const auto& L:Lines)PDI->DrawLine(L.Start,L.End,L.Color,L.DepthPriority,L.Thickness,0,true);
        }
    }
};
}
FPrimitiveSceneProxy* ULidarPlayerDisplayComponent::CreateSceneProxy()
{FixedProxyPointCount=bFixedPointSubmission&&GEngine&&GEngine->VertexColorMaterial?BatchedPoints.Num():0;if(FixedProxyPointCount>0)++FixedPointProxyCreations;return new FPlayerDisplayProxy(this);}
void ULidarPlayerDisplayComponent::CommitPointData()
{
    UpdateBounds();
    if(bFixedPointSubmission&&SceneProxy&&FixedProxyPointCount>0&&FixedProxyPointCount==BatchedPoints.Num()){MarkRenderTransformDirty();MarkRenderDynamicDataDirty();}
    else MarkRenderStateDirty();
}
void ULidarPlayerDisplayComponent::SendRenderDynamicData_Concurrent()
{
    Super::SendRenderDynamicData_Concurrent();
    if(!bFixedPointSubmission||!SceneProxy||FixedProxyPointCount<=0||FixedProxyPointCount!=BatchedPoints.Num())return;
    FPlayerPointUpdate Data;Data.Points=BatchedPoints;Data.Origin=PointViewOrigin;Data.Right=PointViewRight;Data.Up=PointViewUp;Data.Forward=PointViewForward;Data.PixelScale=PointPixelScale;
    auto* Proxy=static_cast<FPlayerDisplayProxy*>(SceneProxy);++FixedPointUploads;
    ENQUEUE_RENDER_COMMAND(HatchUpdateFixedPoints)([Proxy,Data=MoveTemp(Data)](FRHICommandListImmediate& Cmd) mutable {Proxy->UpdatePoints(Cmd,MoveTemp(Data));});
}

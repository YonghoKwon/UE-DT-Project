#include "VirtualLidarStaticCollisionQuery.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/BrushComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "EngineUtils.h"
#include "Misc/Crc.h"
#include "Debug/DebugDrawComponent.h"

namespace
{
class FStaticCollision final:public IVirtualSensorRayQuery
{
public:
    FVirtualSensorCollisionMesh Mesh;
    FName BackendName() const override{return TEXT("static_collision_snapshot");}
    bool CollisionMeshes(TArray<FVirtualSensorCollisionMesh>& Out) const override{Out={Mesh};return true;}
    bool RequiresUnmanagedResolve(FVector,FVector) const override{return false;}
    bool Trace(FVector A,FVector B,FHitResult& Hit) const override
    {FVirtualSensorRayHit Raw;TraceGeometry(A,B,Raw);return Resolve(A,B,Raw,Hit);}
    void TraceGeometry(FVector A,FVector B,FVirtualSensorRayHit& Hit) const override
    {
        Hit=FVirtualSensorRayHit();const FVector3f Start(A),Delta(B-A);float Best=1;
        for(int I=0;I<Mesh.Vertices->Num();I+=3)
        {
            const auto& V=*Mesh.Vertices;const auto E1=V[I+1]-V[I],E2=V[I+2]-V[I],P=FVector3f::CrossProduct(Delta,E2);
            const float Det=FVector3f::DotProduct(E1,P);if(FMath::Abs(Det)<1.e-8f)continue;
            const float Inv=1/Det;const auto T=Start-V[I];const float U=FVector3f::DotProduct(T,P)*Inv;if(U<0||U>1)continue;
            const auto Q=FVector3f::CrossProduct(T,E1);const float W=FVector3f::DotProduct(Delta,Q)*Inv;if(W<0||U+W>1)continue;
            const float Distance=FVector3f::DotProduct(E2,Q)*Inv;if(Distance<0||Distance>=Best)continue;
            Best=Distance;auto Data=(*Mesh.NormalsAndSurfaces)[I/3];uint32 Surface;FMemory::Memcpy(&Surface,&Data.W,4);
            Hit.Component=Mesh.Surfaces[Surface];Hit.Normal=FVector3f(Data);if(FVector3f::DotProduct(Hit.Normal,Delta)>0)Hit.Normal=-Hit.Normal;Hit.Time=Distance;
        }
    }
    bool Resolve(FVector A,FVector B,const FVirtualSensorRayHit& Raw,FHitResult& Hit) const override
    {
        auto* C=Raw.Component.Get();if(!C)return false;const auto P=A+(B-A)*Raw.Time;
        Hit=FHitResult(C->GetOwner(),C,P,FVector(Raw.Normal));Hit.bBlockingHit=true;Hit.ImpactPoint=Hit.Location=P;
        Hit.Normal=Hit.ImpactNormal=FVector(Raw.Normal);Hit.TraceStart=A;Hit.TraceEnd=B;Hit.Time=Raw.Time;Hit.Distance=FVector::Distance(A,P);return true;
    }
};
}
TSharedPtr<const IVirtualSensorRayQuery,ESPMode::ThreadSafe> CaptureLidarStaticCollision(UWorld* World,AActor* Ignore,ECollisionChannel Channel,FString& Reason)
{
    check(IsInGameThread());if(!World)return {};
    auto Result=MakeShared<FStaticCollision,ESPMode::ThreadSafe>();auto Positions=MakeShared<TArray<FVector3f>,ESPMode::ThreadSafe>();
    auto Normals=MakeShared<TArray<FVector4f>,ESPMode::ThreadSafe>();
    for(TActorIterator<AActor> It(World);It;++It)
    {
        if(*It==Ignore)continue;TInlineComponentArray<UPrimitiveComponent*> Components(*It);
        for(auto* Primitive:Components)
        {
            if(!Primitive||!Primitive->IsRegistered()||!Primitive->IsPhysicsStateCreated()||!Primitive->IsQueryCollisionEnabled())continue;
            // Editor GameplayDebugger draw proxies have no physical collision
            // geometry. Match the existing hatch snapshot's display-only rule.
            if(Primitive->IsA<UDebugDrawComponent>())continue;
            const auto Response=Primitive->GetCollisionResponseToChannel(Channel);if(Response==ECR_Ignore)continue;
            auto* Static=Cast<UStaticMeshComponent>(Primitive);auto* Brush=Cast<UBrushComponent>(Primitive);auto* Body=Primitive->GetBodySetup();
            if(Brush&&(!Body||Body->AggGeom.GetElementCount()==0))continue;
            if(Response!=ECR_Block||(!Static&&!Brush)||!Body||(Static&&Static->IsA<UInstancedStaticMeshComponent>())||Body->CollisionTraceFlag==CTF_UseComplexAsSimple)
            {Reason=TEXT("Unsupported collision body retains CPU: ")+GetNameSafe(Primitive);return {};}
            const auto& Shapes=Body->AggGeom;
            if(Shapes.BoxElems.IsEmpty()&&Shapes.ConvexElems.IsEmpty())
            {Reason=TEXT("Static collision has no exported triangles: ")+GetNameSafe(Primitive);return {};}
            if(!Shapes.SphereElems.IsEmpty()||!Shapes.SphylElems.IsEmpty()||!Shapes.TaperedCapsuleElems.IsEmpty())
            {Reason=TEXT("Analytic collision shape retains CPU: ")+GetNameSafe(Primitive);return {};}
            const uint32 Surface=Result->Mesh.Surfaces.Add(Primitive);float SurfaceBits;FMemory::Memcpy(&SurfaceBits,&Surface,4);
            const auto Pose=Primitive->GetComponentTransform();
            const auto Add=[&](FVector A,FVector B,FVector C)
            {
                const FVector3f V0(Pose.TransformPosition(A)),V1(Pose.TransformPosition(B)),V2(Pose.TransformPosition(C));
                Positions->Add(V0);Positions->Add(V1);Positions->Add(V2);Normals->Add(FVector4f(FVector3f::CrossProduct(V1-V0,V2-V0).GetSafeNormal(),SurfaceBits));
            };
            for(const auto& Box:Shapes.BoxElems)
            {
                // Nonuniform scale plus rotated local boxes has physics-specific
                // scaling rules; reject rather than measure a sheared substitute.
                const auto Scale=Pose.GetScale3D().GetAbs();if(!Box.Rotation.IsNearlyZero()&&(!FMath::IsNearlyEqual(Scale.X,Scale.Y)||!FMath::IsNearlyEqual(Scale.Y,Scale.Z)))
                {Reason=TEXT("Rotated nonuniform collision box retains CPU");return {};}
                FVector V[8];for(int Z=0;Z<2;++Z)for(int Y=0;Y<2;++Y)for(int X=0;X<2;++X)V[Z*4+Y*2+X]=Box.Rotation.RotateVector(FVector((X-.5)*Box.X,(Y-.5)*Box.Y,(Z-.5)*Box.Z))+Box.Center;
                const int T[]={0,2,3,0,3,1,4,5,7,4,7,6,0,1,5,0,5,4,2,6,7,2,7,3,0,4,6,0,6,2,1,3,7,1,7,5};
                for(int I=0;I<UE_ARRAY_COUNT(T);I+=3)Add(V[T[I]],V[T[I+1]],V[T[I+2]]);
            }
            for(const auto& Convex:Shapes.ConvexElems)
            {
                if(Convex.IndexData.IsEmpty()||Convex.IndexData.Num()%3)
                {Reason=TEXT("Cooked convex triangle data unavailable; CPU retained");return {};}
                const auto Local=Convex.GetTransform();
                for(int I=0;I<Convex.IndexData.Num();I+=3)
                {
                    const int A=Convex.IndexData[I],B=Convex.IndexData[I+1],C=Convex.IndexData[I+2];
                    if(!Convex.VertexData.IsValidIndex(A)||!Convex.VertexData.IsValidIndex(B)||!Convex.VertexData.IsValidIndex(C)){Reason=TEXT("Invalid convex triangle data; CPU retained");return {};}
                    Add(Local.TransformPosition(Convex.VertexData[A]),Local.TransformPosition(Convex.VertexData[B]),Local.TransformPosition(Convex.VertexData[C]));
                }
            }
        }
    }
    if(Positions->IsEmpty()){Reason=TEXT("No supported static collision geometry; CPU retained");return {};}
    const uint32 A=FCrc::MemCrc32(Positions->GetData(),Positions->Num()*sizeof(FVector3f));
    const uint32 B=FCrc::MemCrc32(Normals->GetData(),Normals->Num()*sizeof(FVector4f));
    Result->Mesh.Revision=(uint64(A)<<32)|B|uint64(1)<<63;Result->Mesh.Vertices=Positions;Result->Mesh.NormalsAndSurfaces=Normals;
    return Result;
}

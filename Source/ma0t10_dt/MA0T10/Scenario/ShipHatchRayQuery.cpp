#include "ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorRayQuery.h"
#include "ProceduralMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Algo/Sort.h"
#include "Debug/DebugDrawComponent.h"
#include "HAL/LowLevelMemTracker.h"
#include <atomic>

namespace
{
std::atomic<uint64> NextCollisionRevision{1};
struct FTriangle
{
 FVector3f A,B,C,Center;
 int32 Surface=0;
};
struct FNode
{
 FBox3f Box=FBox3f(ForceInit);
 int32 First=0,Count=0,Left=-1,Right=-1;
};
struct FTree
{
 TArray<FTriangle> Triangles;
 mutable FVirtualSensorCollisionMesh GpuMesh;
 FTransform FirstSourcePose=FTransform::Identity;
 TArray<int32> Order;
 TArray<FNode> Nodes;
 TArray<TWeakObjectPtr<UPrimitiveComponent>> Surfaces;
 void Add(UProceduralMeshComponent* Mesh)
 {
  if(!Mesh||!Mesh->IsQueryCollisionEnabled()||Mesh->GetCollisionResponseToChannel(ECC_Visibility)!=ECR_Block)return;
  const int Surface=Surfaces.Add(Mesh);const auto Pose=Mesh->GetComponentTransform();if(Surface==0)FirstSourcePose=Pose;
  for(int Section=0;Section<Mesh->GetNumSections();++Section)if(const auto* S=Mesh->GetProcMeshSection(Section))if(S->bEnableCollision)
  {
   for(int I=0;I+2<S->ProcIndexBuffer.Num();I+=3)
   {
    FTriangle T;T.Surface=Surface;T.A=FVector3f(Pose.TransformPosition(S->ProcVertexBuffer[S->ProcIndexBuffer[I]].Position));T.B=FVector3f(Pose.TransformPosition(S->ProcVertexBuffer[S->ProcIndexBuffer[I+1]].Position));T.C=FVector3f(Pose.TransformPosition(S->ProcVertexBuffer[S->ProcIndexBuffer[I+2]].Position));T.Center=(T.A+T.B+T.C)/3.f;Triangles.Add(T);
   }
  }
 }
 int Build(int First,int Count)
 {
  const int Id=Nodes.AddDefaulted();FBox3f Box(ForceInit),Centers(ForceInit);
  for(int I=First;I<First+Count;++I){const auto& T=Triangles[Order[I]];Box+=T.A;Box+=T.B;Box+=T.C;Centers+=T.Center;}
  Nodes[Id].Box=Box.ExpandBy(.001f);Nodes[Id].First=First;Nodes[Id].Count=Count;
  if(Count>8)
  {
   const auto Size=Centers.GetSize();const int Axis=Size.X>=Size.Y&&Size.X>=Size.Z?0:Size.Y>=Size.Z?1:2;
   Algo::Sort(MakeArrayView(Order.GetData()+First,Count),[this,Axis](int A,int B){return Triangles[A].Center[Axis]<Triangles[B].Center[Axis];});
   const int Half=Count/2,L=Build(First,Half),R=Build(First+Half,Count-Half);Nodes[Id].Left=L;Nodes[Id].Right=R;Nodes[Id].Count=0;
  }
  return Id;
 }
 void Finish()
 {
  Order.SetNumUninitialized(Triangles.Num());for(int I=0;I<Order.Num();++I)Order[I]=I;
  Nodes.Reserve(Triangles.Num()/3);if(Order.Num())Build(0,Order.Num());
  GpuMesh.Revision=NextCollisionRevision.fetch_add(1);
 }
 const FVirtualSensorCollisionMesh& ExportGpuMesh() const
 {
  if(GpuMesh.Vertices)return GpuMesh;
  auto Vertices=MakeShared<TArray<FVector3f>,ESPMode::ThreadSafe>();
  auto Attributes=MakeShared<TArray<FVector4f>,ESPMode::ThreadSafe>();
  Vertices->Reserve(Triangles.Num()*3);Attributes->Reserve(Triangles.Num());
  GpuMesh.LocalToWorld=Surfaces.Num()==1?FirstSourcePose:FTransform::Identity;
  for(const auto& T:Triangles)
  {
   const FVector3f A=FVector3f(GpuMesh.LocalToWorld.InverseTransformPosition(FVector(T.A)));
   const FVector3f B=FVector3f(GpuMesh.LocalToWorld.InverseTransformPosition(FVector(T.B)));
   const FVector3f C=FVector3f(GpuMesh.LocalToWorld.InverseTransformPosition(FVector(T.C)));
   Vertices->Add(A);Vertices->Add(B);Vertices->Add(C);
   const auto Normal=FVector3f::CrossProduct(B-A,C-A).GetSafeNormal();
   uint32 Surface=T.Surface;float Bits;FMemory::Memcpy(&Bits,&Surface,sizeof(Bits));Attributes->Add(FVector4f(Normal,Bits));
  }
  GpuMesh.Vertices=Vertices;GpuMesh.NormalsAndSurfaces=Attributes;GpuMesh.Surfaces=Surfaces;return GpuMesh;
 }
 static bool BoxHit(const FBox3f& B,FVector3f S,FVector3f D,FVector3f Inv,float Limit,float& Entry)
 {
  float Low=0,High=Limit;for(int A=0;A<3;++A){if(FMath::Abs(D[A])<1.e-8f){if(S[A]<B.Min[A]||S[A]>B.Max[A])return false;}else{const float P=(B.Min[A]-S[A])*Inv[A],Q=(B.Max[A]-S[A])*Inv[A];Low=FMath::Max(Low,FMath::Min(P,Q));High=FMath::Min(High,FMath::Max(P,Q));if(High<Low)return false;}}Entry=Low;return true;
 }
 static FORCENOINLINE bool BoundaryTriangleHit(const FTriangle& T,FVector3f Start,FVector3f Delta,float& Distance)
 {
  // Shared vertices undergo the same ray-space projection in either adjacent
  // triangle. Inclusive edge functions cannot open the crack that independently
  // rounded Moller-Trumbore barycentrics leave on X=0 cargo-grid boundaries.
  // Only ambiguous edges use this path; the interior fast path stays unchanged.
  const auto Abs=Delta.GetAbs();const int Z=Abs.X>Abs.Y?(Abs.X>Abs.Z?0:2):(Abs.Y>Abs.Z?1:2);
  if(Delta[Z]==0)return false;
  int X=(Z+1)%3,Y=(X+1)%3;if(Delta[Z]<0)Swap(X,Y);
  const double SX=double(Delta[X])/Delta[Z],SY=double(Delta[Y])/Delta[Z];
  const FVector3d A=FVector3d(T.A)-FVector3d(Start),B=FVector3d(T.B)-FVector3d(Start),C=FVector3d(T.C)-FVector3d(Start);
  const double AX=A[X]-SX*A[Z],AY=A[Y]-SY*A[Z],BX=B[X]-SX*B[Z],BY=B[Y]-SY*B[Z],CX=C[X]-SX*C[Z],CY=C[Y]-SY*C[Z];
  const double U=CX*BY-CY*BX,V=AX*CY-AY*CX,W=BX*AY-BY*AX;
  if((U<0||V<0||W<0)&&(U>0||V>0||W>0))return false;
  const double Det=U+V+W;if(Det==0)return false;
  const double D=(U*A[Z]+V*B[Z]+W*C[Z])/(Det*Delta[Z]);
  if(D<0||D>=Distance)return false;Distance=float(D);return true;
 }
 bool Trace(FVector3f Start,FVector3f Delta,FVector3f Inv,float& Best,FVector3f& Normal,TWeakObjectPtr<UPrimitiveComponent>& Primitive) const
 {
  if(Nodes.IsEmpty())return false;float Entry=0;if(!BoxHit(Nodes[0].Box,Start,Delta,Inv,Best,Entry))return false;
  bool Found=false;int Stack[64],Count=0;Stack[Count++]=0;
  while(Count)
  {
   const auto& N=Nodes[Stack[--Count]];if(!BoxHit(N.Box,Start,Delta,Inv,Best,Entry))continue;
   if(N.Count)
   {
    for(int I=N.First;I<N.First+N.Count;++I)
    {
     const auto& T=Triangles[Order[I]];const auto E1=T.B-T.A,E2=T.C-T.A,P=FVector3f::CrossProduct(Delta,E2);const float Det=FVector3f::DotProduct(E1,P);if(FMath::Abs(Det)<1.e-8f)continue;
     const float R=1.f/Det;const auto V=Start-T.A;const float U=FVector3f::DotProduct(V,P)*R;
     // This selects precision, not an acceptance epsilon. With long beams and
     // 0.5m cells, float dot-product cancellation moved a genuine shared-edge
     // barycentric by5.2e-6. The exact projected-edge test below still decides
     // inside/outside; no expanded triangle or relaxed distance gate is used.
     constexpr float EdgeBand=1.e-3f;
     if(U< -EdgeBand||U>1+EdgeBand)continue;
     const auto Q=FVector3f::CrossProduct(V,E1);const float W=FVector3f::DotProduct(Delta,Q)*R;
     if(W< -EdgeBand||U+W>1+EdgeBand)continue;
     float Distance;
     if(U<=EdgeBand||W<=EdgeBand||1-U-W<=EdgeBand)
     {Distance=Best;if(!BoundaryTriangleHit(T,Start,Delta,Distance))continue;}
     else Distance=FVector3f::DotProduct(E2,Q)*R;
     if(Distance>=0&&Distance<Best){Best=Distance;Normal=FVector3f::CrossProduct(E1,E2).GetSafeNormal();if(FVector3f::DotProduct(Normal,Delta)>0)Normal=-Normal;Primitive=Surfaces[T.Surface];Found=true;}
    }
   }
   else
   {
    float L=0,R=0;const bool A=BoxHit(Nodes[N.Left].Box,Start,Delta,Inv,Best,L),B=BoxHit(Nodes[N.Right].Box,Start,Delta,Inv,Best,R);
    if(A&&B){check(Count+2<64);Stack[Count++]=L<R?N.Right:N.Left;Stack[Count++]=L<R?N.Left:N.Right;}else if(A)Stack[Count++]=N.Left;else if(B)Stack[Count++]=N.Right;
   }
  }
  return Found;
 }
};
struct FExternalBox{FBox Box;TWeakObjectPtr<UPrimitiveComponent> Component;};
class FSnapshot final:public IVirtualSensorRayQuery
{
public:
 TSharedPtr<const FTree,ESPMode::ThreadSafe> Static,Cargo,Grab;
 TArray<FExternalBox> External;
 TWeakObjectPtr<UWorld> World;
 TWeakObjectPtr<AActor> Ignore;
 ECollisionChannel Channel=ECC_Visibility;
 mutable uint64 FallbackTraces=0;
 FName BackendName() const override{return TEXT("snapshot_cpu_bvh");}
 bool CollisionMeshes(TArray<FVirtualSensorCollisionMesh>& Out) const override
 {
  // Owned triangles run on GPU; Resolve retains the exact existing unmanaged
  // collision fallback, clipped by the nearest owned GPU hit.
  Out.Reset();for(const auto& Tree:{Static,Cargo,Grab}){if(Tree)Out.Add(Tree->ExportGpuMesh());else Out.Add(FVirtualSensorCollisionMesh());}
  if(!Out.IsEmpty())for(const auto& B:External)Out[0].Surfaces.AddUnique(B.Component);
  return true;
 }
 bool RequiresUnmanagedResolve(FVector Start,FVector End) const override
 {for(const auto& B:External)if(FMath::LineBoxIntersection(B.Box,Start,End,End-Start))return true;return false;}
 FVector2D FallbackStatistics() const override{return FVector2D(External.Num(),FallbackTraces);}
 bool Trace(FVector Start,FVector End,FHitResult& Hit) const override
 {
  FVirtualSensorRayHit Raw;TraceGeometry(Start,End,Raw);return Resolve(Start,End,Raw,Hit);
 }
 void TraceGeometry(FVector Start,FVector End,FVirtualSensorRayHit& Raw) const override
 {
  const FVector3f S(Start),D(End-Start),Inv(FMath::Abs(D.X)>1.e-8?1.f/D.X:0,FMath::Abs(D.Y)>1.e-8?1.f/D.Y:0,FMath::Abs(D.Z)>1.e-8?1.f/D.Z:0);float Best=1;FVector3f Normal=FVector3f::ZeroVector;TWeakObjectPtr<UPrimitiveComponent> Component;
  const FTree* Trees[]={Static.Get(),Cargo.Get(),Grab.Get()};for(const auto* Tree:Trees)if(Tree)Tree->Trace(S,D,Inv,Best,Normal,Component);
  Raw.Component=Component;Raw.Normal=Normal;Raw.Time=Best;
 }
 bool Resolve(FVector Start,FVector End,const FVirtualSensorRayHit& Raw,FHitResult& Hit) const override
 {
  auto* Component=Raw.Component.Get();if(Component){const FVector P=Start+(End-Start)*Raw.Time;Hit=FHitResult(Component->GetOwner(),Component,P,FVector(Raw.Normal));Hit.bBlockingHit=true;Hit.Location=Hit.ImpactPoint=P;Hit.Normal=Hit.ImpactNormal=FVector(Raw.Normal);Hit.TraceStart=Start;Hit.TraceEnd=End;Hit.Distance=FVector::Distance(Start,P);Hit.Time=Raw.Time;}
  // Unknown/non-owned collision remains on the ordinary engine path. The
  // bounds list is captured once per acquisition, never via per-ray actor scans.
  // A foreign object behind the nearest owned surface cannot occlude it.
  // Testing the full beam here needlessly retraced through the entire scene
  // whenever the fixture ground lay behind cargo or the quay.
  const FVector VisibleEnd=Component?Hit.ImpactPoint:End;
  for(const auto& B:External)if(FMath::LineBoxIntersection(B.Box,Start,VisibleEnd,VisibleEnd-Start))if(auto* W=World.Get())
  {++FallbackTraces;FHitResult Other;FCollisionQueryParams Q(SCENE_QUERY_STAT(HatchExternalQuery),false,Ignore.Get());if(W->LineTraceSingleByChannel(Other,Start,End,Channel,Q)&&(!Component||Other.Distance<Hit.Distance)){Hit=Other;return true;}break;}
  return Component!=nullptr;
 }
};
}
struct FShipHatchRayCache
{
 uint32 StaticKey=0;
 uint64 CargoRevision=MAX_uint64,GrabRevision=MAX_uint64;
 TSharedPtr<FTree,ESPMode::ThreadSafe> Static,Cargo,Grab;
 bool LoggedExternal=false;
};
TSharedPtr<const IVirtualSensorRayQuery,ESPMode::ThreadSafe> AShipHatchSimulation::CaptureRayQuery(AActor* IgnoreOwner,ECollisionChannel Channel)
{
  LLM_SCOPE_BYNAME(TEXT("Hatch/RayTree"));
 if(!bUseSnapshotRayQueries||bGenerating||bEnding||Channel!=ECC_Visibility)return {};
 if(!RayCache)RayCache=MakeShared<FShipHatchRayCache,ESPMode::ThreadSafe>();auto& C=*RayCache;
 uint32 Key=GetTypeHash(SceneScaleKey);for(double V:{HoldLength,HoldWidth,HoldDepth,OpeningLength,OpeningWidth,CoamingHeight,BoomLength,BoomHeight,CabHeight,CabX,CabY})Key=HashCombineFast(Key,GetTypeHash(V));
 if(!C.Static||C.StaticKey!=Key)
 {C.StaticKey=Key;C.Static=MakeShared<FTree,ESPMode::ThreadSafe>();for(auto A:Structure)if(A)C.Static->Add(A->Mesh);if(Vessel)C.Static->Add(Vessel->Mesh);if(UnloaderPresentation)C.Static->Add(UnloaderPresentation->Mesh);C.Static->Finish();}
 if(!C.Cargo||C.CargoRevision!=SurfaceRevision){C.CargoRevision=SurfaceRevision;C.Cargo=MakeShared<FTree,ESPMode::ThreadSafe>();if(Cargo)C.Cargo->Add(Cargo->Mesh);C.Cargo->Finish();}
 const uint64 GripKey=bShowGrab?GrabMeshCommits+1:0;
 if(!C.Grab||C.GrabRevision!=GripKey){C.GrabRevision=GripKey;C.Grab=MakeShared<FTree,ESPMode::ThreadSafe>();if(bShowGrab)C.Grab->Add(GrabPresentation);C.Grab->Finish();}
 auto R=MakeShared<FSnapshot,ESPMode::ThreadSafe>();R->Static=C.Static;R->Cargo=C.Cargo;R->Grab=C.Grab;R->World=GetWorld();R->Ignore=IgnoreOwner;R->Channel=Channel;
 TSet<const UPrimitiveComponent*> Owned;for(const auto& T:{C.Static,C.Cargo,C.Grab})for(const auto& P:T->Surfaces)Owned.Add(P.Get());
 for(TActorIterator<AActor>I(GetWorld());I;++I)if(*I!=IgnoreOwner)
 // Debug draw delegates have no trace geometry. Their inherited physics-state
 // flag alone is insufficient: GameplayDebugger advertises a20km render box.
 {TInlineComponentArray<UPrimitiveComponent*> Parts(*I);for(auto* P:Parts)if(!Owned.Contains(P)&&!P->IsA<UDebugDrawComponent>()&&P->IsPhysicsStateCreated()&&P->IsQueryCollisionEnabled()&&P->GetCollisionResponseToChannel(Channel)==ECR_Block){R->External.Add({P->Bounds.GetBox(),P});if(!C.LoggedExternal)UE_LOG(LogTemp,Display,TEXT("[HatchQueryExternal] %s/%s bounds=%s"),*GetNameSafe(*I),*GetNameSafe(P),*P->Bounds.GetBox().ToString());}}
 C.LoggedExternal=true;
 return R;
}
FIntVector AShipHatchSimulation::RayQueryTriangleCounts() const
{return RayCache?FIntVector(RayCache->Static?RayCache->Static->Triangles.Num():0,RayCache->Cargo?RayCache->Cargo->Triangles.Num():0,RayCache->Grab?RayCache->Grab->Triangles.Num():0):FIntVector::ZeroValue;}

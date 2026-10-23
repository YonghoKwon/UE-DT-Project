#include "VirtualLidarScanComponent.h"
#include "IndustrialLidarProfiles.h"
#include "SensorOwnedRayTracing.h"
#include "Components/PrimitiveComponent.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Misc/ScopeLock.h"
#include "HAL/IConsoleManager.h"
#include <atomic>
#include <memory>

struct FVirtualLidarSceneQueryBuffer
{
 TArray<FVirtualSensorRayHit> Hits;
 std::atomic<bool> Busy{false};
};
struct FVirtualLidarScenePointPool
{
 using FPoints=TArray<FVirtualLidarPoint>;
 FCriticalSection Mutex;
 TArray<FPoints*> Free;
 uint64 Bytes=0;
 explicit FVirtualLidarScenePointPool(int32 Capacity)
 {
  Free.Reserve(3);
  for(int I=0;I<3;++I){auto* P=new FPoints;P->SetNum(Capacity);P->Reset();Bytes+=P->GetAllocatedSize();Free.Add(P);}
 }
 ~FVirtualLidarScenePointPool(){for(auto* P:Free)delete P;}
 FPoints* Take(){FScopeLock Lock(&Mutex);return Free.IsEmpty()?nullptr:Free.Pop(false);}
 void Return(FPoints* P){FScopeLock Lock(&Mutex);Free.Add(P);}
};
struct FHardwareNormalizedSurface
{
 VirtualLidarPointModel::FSurface Surface;
 FVirtualLidarPoint Metadata;
 bool bIgnore=false,bValid=false;
};
struct FVirtualLidarSceneQueryJob
{
 TSharedPtr<const IVirtualSensorRayQuery,ESPMode::ThreadSafe> Scene;
 TSharedPtr<const TArray<FVector3f>,ESPMode::ThreadSafe> Directions;
 TSharedPtr<FVirtualLidarSceneQueryBuffer,ESPMode::ThreadSafe> Buffer;
 FTransform Pose;
 TArray<float> Horizontal,Vertical;
 EVirtualLidarDeviceProfile Profile=EVirtualLidarDeviceProfile::LivoxMid360S;
 int Width=0,Height=0;float Range=0;int64 Frame=0;uint64 Generation=0;
 bool Started=false;
 TSharedPtr<ISensorRtScan,ESPMode::ThreadSafe> GpuScan;
 TArray<FVirtualSensorCollisionMesh> GpuMeshes;
 double GpuSubmitted=0;
 VirtualLidarPointModel::FConfig Model;
 TArray<TArray<FHardwareNormalizedSurface>> FrozenSurfaces;
 TMap<TWeakObjectPtr<UPrimitiveComponent>,FHardwareNormalizedSurface> CpuSurfaces;
 TArray<FVirtualLidarPoint> WorkerPoints;
 TSharedPtr<TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe> PointLease;
 TArray<uint8> RequiresResolve;
 bool bWorkerNormalize=false,bReadbackNormalize=false,bCpuNormalize=false,bRollingTimestamps=false,bMultiEcho=false;
 std::atomic<bool> WorkerStarted{false},ReadbackFailed{false};
 FString ReadbackFailure;
 float Interval=0;
 std::atomic<bool> WorkerReady{false};
#if WITH_DEV_AUTOMATION_TESTS
 std::atomic<double> NormalizeFinished{0};
 std::atomic<double> ReadbackCopied{0};
#endif
 ~FVirtualLidarSceneQueryJob(){if(PointLease)*PointLease=MoveTemp(WorkerPoints);}

 int ChunkCount=0;
 std::unique_ptr<std::atomic<bool>[]> Chunks;
 std::atomic<int32> ReadyRays{0};
 std::atomic<double> GeometryMs{0};
 std::atomic<bool> Cancelled{false},Ready{false};
};
namespace
{
TAutoConsoleVariable<int32> CVarLidarReadbackNormalize(TEXT("ma0t10.LidarReadbackNormalize"),1,
 TEXT("Start bounded CPU normalization directly after HWRT readback.0 retains legacy GT polling."));
TAutoConsoleVariable<int32> CVarLidarCpuSnapshotNormalize(TEXT("ma0t10.LidarCpuSnapshotNormalize"),1,
 TEXT("Normalize immutable managed CPU snapshots on their existing worker.0 retains GT chunks."));
void InitializeWorkerPoint(const FVirtualLidarSceneQueryJob& J,int I,FVirtualLidarPoint& P,FVector& Direction,float& Range)
{
 const int Row=I/J.Width,Col=I%J.Width;
 const FVector Local=J.Directions?FVector((*J.Directions)[I]):IndustrialLidar::Beam(J.Profile,I,J.Width,J.Height,J.Frame,J.Vertical[Row],J.Horizontal[Col]).Vector();
 Direction=J.Pose.TransformVectorNoScale(Local).GetSafeNormal();Range=J.Profile==EVirtualLidarDeviceProfile::HesaiXT32&&(Row<8||Row>=24)?FMath::Min(J.Range,5000.f):J.Range;
 P=FVirtualLidarPoint();P.LocalDirection=J.Pose.InverseTransformVectorNoScale(Direction).GetSafeNormal();
 P.Row=P.Ring=Row;P.Col=P.HorizontalIndex=Col;P.ReturnIndex=P.EchoIndex=0;P.EchoCount=0;P.EchoType=EVirtualLidarEchoType::None;
 P.PointTimeOffsetNanoseconds=J.bRollingTimestamps?VirtualLidarFidelity::PointOffset(J.Profile,I,J.Width,J.Height,J.Interval):0;
 P.Validity=EVirtualLidarPointValidity::NoReturn;P.AmbientLux=J.Model.AmbientLightLux;P.bHasGridCoord=true;
 P.Distance=J.Range;P.WorldLocation=J.Pose.GetLocation()+Direction*Range;
}
void NormalizeHardware(const TSharedPtr<FVirtualLidarSceneQueryJob,ESPMode::ThreadSafe>& J,TArray<FSensorRtHit>&& Hits)
{
 if(J->Cancelled.load(std::memory_order_acquire)){J->Buffer->Busy.store(false,std::memory_order_release);J->WorkerReady.store(true,std::memory_order_release);return;}
 const int Count=Hits.Num();J->WorkerPoints.SetNum(Count);J->Buffer->Hits.SetNum(Count);J->RequiresResolve.SetNumZeroed(Count);
 ParallelFor(FMath::DivideAndRoundUp(Count,512),[J,&Hits,Count](int Chunk)
 {
  if(J->Cancelled.load(std::memory_order_acquire))return;
  for(int I=Chunk*512;I<FMath::Min(Count,(Chunk+1)*512);++I)
  {
   auto& Point=J->WorkerPoints[I];auto& Raw=J->Buffer->Hits[I];Raw=FVirtualSensorRayHit();
   FVector Direction;float Range;InitializeWorkerPoint(*J,I,Point,Direction,Range);
   const auto& Hit=Hits[I];const bool Found=Hit.NormalAndDistance.W>=0&&J->GpuMeshes.IsValidIndex(Hit.Mesh)&&J->GpuMeshes[Hit.Mesh].Surfaces.IsValidIndex(Hit.Surface);
   if(Found){Raw.Component=J->GpuMeshes[Hit.Mesh].Surfaces[Hit.Surface];Raw.Normal=FVector3f(Hit.NormalAndDistance);Raw.Time=Hit.NormalAndDistance.W/Range;}
   const auto Origin=J->Pose.GetLocation();const FVector End=Found?Origin+Direction*Hit.NormalAndDistance.W:Point.WorldLocation;
   if(J->Scene->RequiresUnmanagedResolve(Origin,End)){J->RequiresResolve[I]=1;continue;}
   if(!Found||!J->FrozenSurfaces.IsValidIndex(Hit.Mesh)||!J->FrozenSurfaces[Hit.Mesh].IsValidIndex(Hit.Surface))continue;
   const auto& Identity=J->FrozenSurfaces[Hit.Mesh][Hit.Surface];if(!Identity.bValid||Identity.bIgnore)continue;
   if(J->Model.Apply(Point,Identity.Surface,Direction,J->Pose,Hit.NormalAndDistance.W,FVector::DotProduct(-Direction,FVector(Raw.Normal).GetSafeNormal()),I,0))
   {
    Point.EchoCount=1;Point.EchoType=EVirtualLidarEchoType::Single;Point.SemanticLabel=Identity.Metadata.SemanticLabel;
    Point.HitActorName=Identity.Metadata.HitActorName;Point.HitActorClassName=Identity.Metadata.HitActorClassName;Point.HitActorTags=Identity.Metadata.HitActorTags;
   }
   else if(J->bMultiEcho)InitializeWorkerPoint(*J,I,Point,Direction,Range);
  }
 },EParallelForFlags::BackgroundPriority|EParallelForFlags::Unbalanced);
#if WITH_DEV_AUTOMATION_TESTS
 J->NormalizeFinished.store(FPlatformTime::Seconds(),std::memory_order_relaxed);
#endif
 J->Buffer->Busy.store(false,std::memory_order_release);J->WorkerReady.store(true,std::memory_order_release);
}
void NormalizeCpuSnapshot(const TSharedPtr<FVirtualLidarSceneQueryJob,ESPMode::ThreadSafe>& J)
{
 const int Count=J->Width*J->Height;J->WorkerPoints.SetNum(Count);J->RequiresResolve.SetNumZeroed(Count);
 ParallelFor(FMath::DivideAndRoundUp(Count,512),[J,Count](int Chunk)
 {
  if(J->Cancelled.load(std::memory_order_acquire))return;
  for(int I=Chunk*512;I<FMath::Min(Count,(Chunk+1)*512);++I)
  {
   auto& Point=J->WorkerPoints[I];const auto& Raw=J->Buffer->Hits[I];
   FVector Direction;float Range;InitializeWorkerPoint(*J,I,Point,Direction,Range);
   const auto Origin=J->Pose.GetLocation();const FVector RayEnd=Origin+Direction*Range;
   const FVector End=Origin+(RayEnd-Origin)*Raw.Time;
   if(J->Scene->RequiresUnmanagedResolve(Origin,End)){J->RequiresResolve[I]=1;continue;}
   if(Raw.Component.IsExplicitlyNull())continue;
   const auto* Identity=J->CpuSurfaces.Find(Raw.Component);
   if(!Identity){J->RequiresResolve[I]=1;continue;}
   if(Identity->bIgnore)continue;
   if(J->Model.Apply(Point,Identity->Surface,Direction,J->Pose,float(FVector::Distance(Origin,End)),FVector::DotProduct(-Direction,FVector(Raw.Normal).GetSafeNormal()),I,0))
   {
    Point.EchoCount=1;Point.EchoType=EVirtualLidarEchoType::Single;Point.SemanticLabel=Identity->Metadata.SemanticLabel;
    Point.HitActorName=Identity->Metadata.HitActorName;Point.HitActorClassName=Identity->Metadata.HitActorClassName;Point.HitActorTags=Identity->Metadata.HitActorTags;
   }
   else if(J->bMultiEcho)InitializeWorkerPoint(*J,I,Point,Direction,Range);
  }
 },EParallelForFlags::BackgroundPriority|EParallelForFlags::Unbalanced);
#if WITH_DEV_AUTOMATION_TESTS
 J->NormalizeFinished.store(FPlatformTime::Seconds(),std::memory_order_relaxed);
#endif
 J->Buffer->Busy.store(false,std::memory_order_release);J->WorkerReady.store(true,std::memory_order_release);
}
}
void UVirtualLidarScanComponent::BeginSceneQueryBatch()
{
 HardwareSurfaceIdentities.Reset();
 if(!ScheduledRayQuery)
 {
  if(ActiveAcquisitionBackend==EVirtualLidarAcquisitionBackend::HardwareRayTracing){ActiveAcquisitionBackend=EVirtualLidarAcquisitionBackend::AccurateCpuTrace;RuntimeStatus.AcquisitionBackendMessage=TEXT("이 장면의 충돌 형상은 GPU에서 지원하지 않아 CPU로 측정합니다.");}
  return;
 }
 SceneQueryGameSeconds=0;
 if(!SceneQueryBuffer)SceneQueryBuffer=MakeShared<FVirtualLidarSceneQueryBuffer,ESPMode::ThreadSafe>();
 if(bSceneQueryDirectionsCached&&(!SharedSceneQueryDirections||SharedSceneDirectionKey!=SceneQueryDirectionKey))
 {SharedSceneDirectionKey=SceneQueryDirectionKey;SharedSceneQueryDirections=MakeShared<const TArray<FVector3f>,ESPMode::ThreadSafe>(SceneQueryDirections);}
 auto J=MakeShared<FVirtualLidarSceneQueryJob,ESPMode::ThreadSafe>();J->Scene=ScheduledRayQuery;J->Buffer=SceneQueryBuffer;J->Pose=ScheduledScanTransform;J->Profile=DeviceProfile;J->Width=ScheduledScanWidth;J->Height=ScheduledScanHeight;J->Range=MaxDistance;J->Frame=FrameId+1;J->Generation=ScheduledGeneration;
 if(bSceneQueryDirectionsCached)J->Directions=SharedSceneQueryDirections;else {J->Horizontal=ScheduledHorizontalAnglesDegrees;J->Vertical=ScheduledVerticalAnglesDegrees;}
 if(ActiveAcquisitionBackend==EVirtualLidarAcquisitionBackend::HardwareRayTracing)
 {
  J->Model=CapturePointModel();J->Interval=ScanInterval;J->bRollingTimestamps=bUseRollingPointTimestamps;
  J->bMultiEcho=bUseMultiHit||(bUseProfileEchoCapability&&DeviceSpec.MaxEchoesPerPixel>1);
  J->bWorkerNormalize=!bEnableMotionDistortion&&!bDrawDebugRays;
  FSensorRtRequest Request;
  if(J->Scene->CollisionMeshes(J->GpuMeshes))
  {
   J->FrozenSurfaces.SetNum(J->GpuMeshes.Num());int MeshIndex=0;
   for(const auto& Mesh:J->GpuMeshes)
   {
    auto& Frozen=J->FrozenSurfaces[MeshIndex++];Frozen.SetNum(Mesh.Surfaces.Num());int SurfaceIndex=0;
    Request.Meshes.Add({Mesh.Revision,Mesh.Vertices,Mesh.NormalsAndSurfaces,Mesh.LocalToWorld});
    for(const auto& Weak:Mesh.Surfaces)
    {
     const int SurfaceId=SurfaceIndex++;auto* Component=Weak.Get();if(!Component)continue;
     FHardwareSurfaceIdentity Identity;Identity.Surface=ResolveSurfaceResponse(Component);
     FHitResult Hit(Component->GetOwner(),Component,FVector::ZeroVector,FVector::UpVector);
     PopulatePointSemanticMetadata(Identity.Metadata,Hit);Identity.bIgnore=ShouldIgnoreHitActor(Component->GetOwner());
     Frozen[SurfaceId].Surface=Identity.Surface;Frozen[SurfaceId].Metadata=Identity.Metadata;Frozen[SurfaceId].bIgnore=Identity.bIgnore;Frozen[SurfaceId].bValid=true;
     HardwareSurfaceIdentities.Add(Weak,MoveTemp(Identity));
    }
   }
   Request.Rays.SetNumUninitialized(J->Width*J->Height);const auto Origin=J->Pose.GetLocation();
   for(int I=0;I<Request.Rays.Num();++I)
   {
    const int Row=I/J->Width,Col=I%J->Width;const FVector Local=J->Directions?FVector((*J->Directions)[I]):IndustrialLidar::Beam(J->Profile,I,J->Width,J->Height,J->Frame,J->Vertical[Row],J->Horizontal[Col]).Vector();
    const float Range=J->Profile==EVirtualLidarDeviceProfile::HesaiXT32&&(Row<8||Row>=24)?FMath::Min(J->Range,5000.f):J->Range;
    Request.Rays[I]={FVector3f(Origin),Range,FVector3f(J->Pose.TransformVectorNoScale(Local).GetSafeNormal()),0};
   }
   if(J->bWorkerNormalize&&CVarLidarReadbackNormalize.GetValueOnGameThread()!=0)
   {
    // Reserve the same single job/point lease before publishing to the RT.
    J->bReadbackNormalize=true;J->PointLease=MoveTemp(ScenePointLease);J->WorkerPoints=MoveTemp(ScheduledPoints);
    Request.OnReadback=[J](TArray<FSensorRtHit>&& Hits,const FString& Error)
    {
     if(J->Cancelled.load(std::memory_order_acquire))return;
     if(!Error.IsEmpty()||Hits.Num()!=J->Width*J->Height)
     {J->ReadbackFailure=Error.IsEmpty()?TEXT("HWRT hit count differs"):Error;J->ReadbackFailed.store(true,std::memory_order_release);return;}
#if WITH_DEV_AUTOMATION_TESTS
     J->ReadbackCopied.store(FPlatformTime::Seconds(),std::memory_order_relaxed);
#endif
     J->Buffer->Busy.store(true,std::memory_order_release);J->WorkerStarted.store(true,std::memory_order_release);
     Async(EAsyncExecution::ThreadPool,[J,Hits=MoveTemp(Hits)]() mutable {NormalizeHardware(J,MoveTemp(Hits));});
    };
   }
   J->GpuSubmitted=FPlatformTime::Seconds();J->GpuScan=SensorOwnedRayTracing::Submit(MoveTemp(Request));
   if(!J->GpuScan&&J->bReadbackNormalize)
   {ScenePointLease=MoveTemp(J->PointLease);ScheduledPoints=MoveTemp(J->WorkerPoints);J->bReadbackNormalize=false;}
  }
  if(J->GpuScan){RuntimeStatus.ActiveAcquisitionBackend=TEXT("hardware_ray_tracing");RuntimeStatus.AcquisitionBackendMessage=TEXT("GPU 광선 측정 · 비동기 결과 처리");}
  else {ActiveAcquisitionBackend=EVirtualLidarAcquisitionBackend::AccurateCpuTrace;HardwareSurfaceIdentities.Reset();RuntimeStatus.AcquisitionBackendMessage=TEXT("GPU 측정 준비 또는 작업 한도를 사용할 수 없어 CPU로 측정합니다.");}
 }
 if(ActiveAcquisitionBackend==EVirtualLidarAcquisitionBackend::AccurateCpuTrace&&!bEnableMotionDistortion&&!bDrawDebugRays&&CVarLidarCpuSnapshotNormalize.GetValueOnGameThread()!=0)
 {
  TArray<FVirtualSensorCollisionMesh> Meshes;
  if(J->Scene->CollisionMeshes(Meshes))
  {
   for(const auto& Mesh:Meshes)for(const auto& Weak:Mesh.Surfaces)if(auto* Component=Weak.Get())
   {
    FHardwareNormalizedSurface Frozen;Frozen.Surface=ResolveSurfaceResponse(Component);
    FHitResult Hit(Component->GetOwner(),Component,FVector::ZeroVector,FVector::UpVector);
    PopulatePointSemanticMetadata(Frozen.Metadata,Hit);Frozen.bIgnore=ShouldIgnoreHitActor(Component->GetOwner());Frozen.bValid=true;
    J->CpuSurfaces.Add(Weak,MoveTemp(Frozen));
   }
   J->bCpuNormalize=true;J->Model=CapturePointModel();J->Interval=ScanInterval;J->bRollingTimestamps=bUseRollingPointTimestamps;
   J->bMultiEcho=bUseMultiHit||(bUseProfileEchoCapability&&DeviceSpec.MaxEchoesPerPixel>1);
   J->PointLease=MoveTemp(ScenePointLease);J->WorkerPoints=MoveTemp(ScheduledPoints);
  }
 }
 SceneQueryJob=J;
}
bool UVirtualLidarScanComponent::PollSceneQueryBatch()
{
 auto J=SceneQueryJob;if(!J)return true;
 if(J->Generation!=ScheduledGeneration||J->Cancelled.load(std::memory_order_acquire))return false;
 if(J->ReadyRays.load(std::memory_order_acquire)>ScheduledNextRayIndex)return true;
 if(J->bReadbackNormalize)
 {
  if(J->ReadbackFailed.load(std::memory_order_acquire))
  {
   RuntimeStatus.AcquisitionBackendMessage=TEXT("GPU readback failed; CPU fallback: ")+J->ReadbackFailure;
   ActiveAcquisitionBackend=EVirtualLidarAcquisitionBackend::AccurateCpuTrace;RuntimeStatus.ActiveAcquisitionBackend=TEXT("snapshot_cpu_bvh");
   ScenePointLease=MoveTemp(J->PointLease);ScheduledPoints=MoveTemp(J->WorkerPoints);J->GpuScan.Reset();J->bReadbackNormalize=false;
  }
  else
  {
   if(J->WorkerReady.load(std::memory_order_acquire)){J->ReadyRays.store(J->Width*J->Height,std::memory_order_release);return true;}
   // Drive the existing nonblocking RT poll until it has dispatched the worker.
   if(!J->WorkerStarted.load(std::memory_order_acquire)){TArray<FSensorRtHit> Unused;FString Error;J->GpuScan->Poll(Unused,Error);}
   return false;
  }
 }
 if(J->WorkerStarted)
 {
  if(!J->WorkerReady.load(std::memory_order_acquire))return false;
  J->ReadyRays.store(J->Width*J->Height,std::memory_order_release);return true;
 }
 if(J->GpuScan)
 {
  if(J->Buffer->Busy.load())return false;
  TArray<FSensorRtHit> Hits;FString Error;if(!J->GpuScan->Poll(Hits,Error))return false;
  if(!Error.IsEmpty()||Hits.Num()!=J->Width*J->Height)
  {
   RuntimeStatus.AcquisitionBackendMessage=TEXT("GPU 측정 실패 · 같은 장면을 CPU로 다시 측정합니다. ")+Error;
   ActiveAcquisitionBackend=EVirtualLidarAcquisitionBackend::AccurateCpuTrace;RuntimeStatus.ActiveAcquisitionBackend=TEXT("snapshot_cpu_bvh");J->GpuScan.Reset();
  }
  else
  {
   if(J->Buffer->Busy.load())return false;
   if(J->bWorkerNormalize)
   {
#if WITH_DEV_AUTOMATION_TESTS
    if(bTimingTraceForTests){ActiveTimingSampleForTests.ReadbackObserved=FPlatformTime::Seconds();ActiveTimingSampleForTests.ReadbackFrame=GFrameCounter;}
#endif
    J->PointLease=MoveTemp(ScenePointLease);J->WorkerPoints=MoveTemp(ScheduledPoints);J->WorkerStarted=true;J->Buffer->Busy.store(true);
    Async(EAsyncExecution::ThreadPool,[J,Hits=MoveTemp(Hits)]() mutable {NormalizeHardware(J,MoveTemp(Hits));});return false;
   }
   J->Buffer->Hits.SetNumUninitialized(Hits.Num());
   for(int I=0;I<Hits.Num();++I)
   {
    auto& Raw=J->Buffer->Hits[I];Raw=FVirtualSensorRayHit();const auto& Gpu=Hits[I];
    if(Gpu.NormalAndDistance.W<0||!J->GpuMeshes.IsValidIndex(Gpu.Mesh))continue;
    const auto& Mesh=J->GpuMeshes[Gpu.Mesh];if(!Mesh.Surfaces.IsValidIndex(Gpu.Surface))continue;
    const int Row=I/J->Width;const float Range=J->Profile==EVirtualLidarDeviceProfile::HesaiXT32&&(Row<8||Row>=24)?FMath::Min(J->Range,5000.f):J->Range;
    Raw.Component=Mesh.Surfaces[Gpu.Surface];Raw.Normal=FVector3f(Gpu.NormalAndDistance);Raw.Time=Gpu.NormalAndDistance.W/Range;
   }
   J->GeometryMs.store((FPlatformTime::Seconds()-J->GpuSubmitted)*1000);J->ReadyRays.store(Hits.Num(),std::memory_order_release);J->Started=true;J->Ready.store(true);return true;
  }
 }
 if(J->Started)return false;
 bool Expected=false;if(!J->Buffer->Busy.compare_exchange_strong(Expected,true,std::memory_order_acq_rel))return false;
 J->Started=true;J->Buffer->Hits.SetNumUninitialized(J->Width*J->Height,false);J->ChunkCount=FMath::DivideAndRoundUp(J->Width*J->Height,256);J->Chunks=std::make_unique<std::atomic<bool>[]>(J->ChunkCount);for(int I=0;I<J->ChunkCount;++I)J->Chunks[I].store(false);
 if(J->bCpuNormalize)J->WorkerStarted.store(true,std::memory_order_release);
 Async(EAsyncExecution::ThreadPool,[J]()
 {
  const double Started=FPlatformTime::Seconds();
  const auto Origin=J->Pose.GetLocation();
  ParallelFor(J->ChunkCount,[J,Origin](int Chunk)
  {
   if(J->Cancelled.load(std::memory_order_acquire))return;
   const int End=FMath::Min((Chunk+1)*256,J->Width*J->Height);
   for(int I=Chunk*256;I<End;++I)
   {
    const int Row=I/J->Width,Col=I%J->Width;const FVector Local=J->Directions?FVector((*J->Directions)[I]):IndustrialLidar::Beam(J->Profile,I,J->Width,J->Height,J->Frame,J->Vertical[Row],J->Horizontal[Col]).Vector();
    const auto Direction=J->Pose.TransformVectorNoScale(Local).GetSafeNormal();const double Range=J->Profile==EVirtualLidarDeviceProfile::HesaiXT32&&(Row<8||Row>=24)?FMath::Min(J->Range,5000.f):J->Range;
    J->Scene->TraceGeometry(Origin,Origin+Direction*Range,J->Buffer->Hits[I]);
   }
   J->Chunks[Chunk].store(true,std::memory_order_release);
   if(!J->bCpuNormalize)
   {
    int Old=J->ReadyRays.load(std::memory_order_acquire);
    for(;;){int Prefix=Old/256;while(Prefix<J->ChunkCount&&J->Chunks[Prefix].load(std::memory_order_acquire))++Prefix;const int New=FMath::Min(Prefix*256,J->Width*J->Height);if(New<=Old||J->ReadyRays.compare_exchange_weak(Old,New,std::memory_order_acq_rel))break;}
   }
  },EParallelForFlags::BackgroundPriority|EParallelForFlags::Unbalanced);
  J->GeometryMs.store((FPlatformTime::Seconds()-Started)*1000,std::memory_order_release);
  if(J->bCpuNormalize){NormalizeCpuSnapshot(J);return;}
  J->Ready.store(!J->Cancelled.load(std::memory_order_acquire),std::memory_order_release);J->Buffer->Busy.store(false,std::memory_order_release);
 });
 return false;
}
bool UVirtualLidarScanComponent::ResolveSceneQueryRay(int32 Index,FVector Start,FVector End,FHitResult& Hit) const
{return SceneQueryJob&&Index<SceneQueryJob->ReadyRays.load(std::memory_order_acquire)&&SceneQueryBuffer->Hits.IsValidIndex(Index)&&ScheduledRayQuery->Resolve(Start,End,SceneQueryBuffer->Hits[Index],Hit);}
int32 UVirtualLidarScanComponent::SceneQueryReadyRays() const
{return SceneQueryJob?SceneQueryJob->ReadyRays.load(std::memory_order_acquire):0;}
void UVirtualLidarScanComponent::CancelSceneQueryBatch()
{if(SceneQueryJob){SceneQueryJob->Cancelled.store(true,std::memory_order_release);if(SceneQueryJob->GpuScan)SceneQueryJob->GpuScan->Cancel();}SceneQueryJob.Reset();}
uint64 UVirtualLidarScanComponent::SceneQueryBufferBytes() const
{return (SceneQueryBuffer?SceneQueryBuffer->Hits.GetAllocatedSize():0)+(SharedSceneQueryDirections?SharedSceneQueryDirections->GetAllocatedSize():0);}
void UVirtualLidarScanComponent::RecordSceneQueryWork(double Seconds,bool Complete)
{
 if(!SceneQueryJob)return;SceneQueryGameSeconds+=Seconds;
 if(Complete){const auto F=ScheduledRayQuery->FallbackStatistics();LastSceneQueryStatistics=FVector4(SceneQueryJob->GeometryMs.load(std::memory_order_acquire),SceneQueryGameSeconds*1000,F.X,F.Y);}
}
void UVirtualLidarScanComponent::AcquireScenePointStorage(int32 Capacity)
{
 RecycleScenePointStorage();
 if(ScenePointCapacity!=Capacity)
 {
  ScenePointCapacity=Capacity;
  // Three bounded leases cover current, published and analysis-held frames.
  // Construct once to commit the storage during initial preparation, then reuse.
  ScenePointPool=MakeShared<FVirtualLidarScenePointPool,ESPMode::ThreadSafe>(Capacity);
 }
 if(auto* P=ScenePointPool->Take())
 {
  // A fresh shared control block per publication also expires old weak pointers.
  // Storage returns only after the last immutable consumer releases that frame.
  TWeakPtr<FVirtualLidarScenePointPool,ESPMode::ThreadSafe> WeakPool(ScenePointPool);
  ScenePointLease=MakeShareable(P,[WeakPool](TArray<FVirtualLidarPoint>* Released){if(auto Pool=WeakPool.Pin())Pool->Return(Released);else delete Released;});
  ScheduledPoints=MoveTemp(*P);ScheduledPoints.Reset();return;
 }
 // A transport/export consumer may legally retain every lease. Never mutate
 // its immutable payload or block acquisition; ordinary bounded output policy
 // continues to own that consumer's memory reservation.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
 ++ScenePointPoolFallbacksForTesting;
#endif
 ScheduledPoints.Reserve(Capacity);
}
void UVirtualLidarScanComponent::RecycleScenePointStorage()
{if(ScenePointLease){*ScenePointLease=MoveTemp(ScheduledPoints);ScenePointLease.Reset();}}
TSharedPtr<TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe> UVirtualLidarScanComponent::CompleteScenePointStorage()
{
 if(!ScenePointLease)return {};
 *ScenePointLease=MoveTemp(ScheduledPoints);ScenePublishedPool=ScenePointPool;ScenePublishedPoints=ScenePointLease;return MoveTemp(ScenePointLease);
}
uint64 UVirtualLidarScanComponent::ScheduledPointStorageBytes() const
{
 uint64 Bytes=(ScenePointPool?ScenePointPool->Bytes:0)+(ScenePointLease?0:ScheduledPoints.GetAllocatedSize());
 if(ScenePointPool&&ScenePublishedPool.Pin()==ScenePointPool&&ScenePublishedPoints.Pin()==LastPointStorage)Bytes-=LastPointStorage->GetAllocatedSize();
 // Latest published storage is reported separately under shared/latest_points.
 return Bytes;
}

bool UVirtualLidarScanComponent::CopyHardwareSemanticMetadata(FVirtualLidarPoint& Point,const FHitResult& Hit) const
{
 if(const auto* Frozen=HardwareSurfaceIdentities.Find(Hit.GetComponent()))
 {
  Point.SemanticLabel=Frozen->Metadata.SemanticLabel;Point.HitActorName=Frozen->Metadata.HitActorName;
  Point.HitActorClassName=Frozen->Metadata.HitActorClassName;Point.HitActorTags=Frozen->Metadata.HitActorTags;return true;
 }
 return false;
}
bool UVirtualLidarScanComponent::ShouldIgnoreScheduledHit(const FHitResult& Hit) const
{
 if(const auto* Frozen=HardwareSurfaceIdentities.Find(Hit.GetComponent()))return Frozen->bIgnore;
 return ShouldIgnoreHitActor(Hit.GetActor());
}

int32 UVirtualLidarScanComponent::CompleteHardwareNormalizedFrame()
{
 auto J=SceneQueryJob;if(!J||!J->WorkerStarted||!J->WorkerReady.load(std::memory_order_acquire))return -1;
#if WITH_DEV_AUTOMATION_TESTS
 if(bTimingTraceForTests)
 {
  ActiveTimingSampleForTests.Normalized=J->NormalizeFinished.load(std::memory_order_relaxed);
  if(J->bReadbackNormalize){ActiveTimingSampleForTests.ReadbackObserved=J->ReadbackCopied.load(std::memory_order_relaxed);ActiveTimingSampleForTests.ReadbackFrame=GFrameCounter;}
 }
#endif
 const double Started=FPlatformTime::Seconds();ScheduledPoints=MoveTemp(J->WorkerPoints);ScenePointLease=MoveTemp(J->PointLease);
 TMap<FName,int32> Counts;ScheduledHitPointCount=0;ScheduledSemanticCounts.Reset();
 for(int I=0;I<ScheduledPoints.Num();++I)
 {
  auto& Point=ScheduledPoints[I];
  if(J->RequiresResolve[I])
  {
   FVector Direction;float Range;InitializeWorkerPoint(*J,I,Point,Direction,Range);FHitResult Hit;
   bool bHit=J->Scene->Resolve(J->Pose.GetLocation(),J->Pose.GetLocation()+Direction*Range,J->Buffer->Hits[I],Hit)&&!ShouldIgnoreScheduledHit(Hit);
   if(bHit)bHit=ApplyPhysicalHitModel(Point,Hit,Direction,J->Pose,I,0);
   if(bHit){PopulatePointSemanticMetadata(Point,Hit);Point.EchoCount=1;Point.EchoType=EVirtualLidarEchoType::Single;}
   else if(J->bMultiEcho)InitializeWorkerPoint(*J,I,Point,Direction,Range);
  }
  if(Point.bHit){++ScheduledHitPointCount;++Counts.FindOrAdd(Point.SemanticLabel.IsNone()?FName(TEXT("Unclassified")):Point.SemanticLabel);}
  WriteHeatmapPixel(ScheduledHeatmapPixels,GetHeatmapPixelIndex(I%J->Width,I/J->Width,J->Width,J->Height),Point);
 }
 for(const auto& Count:Counts)ScheduledSemanticCounts.Add(Count.Key.ToString(),Count.Value);
 ScheduledNextRayIndex=J->Width*J->Height;if(J->GpuSubmitted>0)J->GeometryMs.store((FPlatformTime::Seconds()-J->GpuSubmitted)*1000);
 RecordSceneQueryWork(FPlatformTime::Seconds()-Started,true);CompleteScheduledScan(GetWorld()->GetTimeSeconds());return J->Width*J->Height;
}

#include "VirtualLidarScanComponent.h"
#include "IndustrialLidarProfiles.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Misc/ScopeLock.h"
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
 int ChunkCount=0;
 std::unique_ptr<std::atomic<bool>[]> Chunks;
 std::atomic<int32> ReadyRays{0};
 std::atomic<double> GeometryMs{0};
 std::atomic<bool> Cancelled{false},Ready{false};
};
void UVirtualLidarScanComponent::BeginSceneQueryBatch()
{
 if(!ScheduledRayQuery)return;
 SceneQueryGameSeconds=0;
 if(!SceneQueryBuffer)SceneQueryBuffer=MakeShared<FVirtualLidarSceneQueryBuffer,ESPMode::ThreadSafe>();
 if(bSceneQueryDirectionsCached&&(!SharedSceneQueryDirections||SharedSceneDirectionKey!=SceneQueryDirectionKey))
 {SharedSceneDirectionKey=SceneQueryDirectionKey;SharedSceneQueryDirections=MakeShared<const TArray<FVector3f>,ESPMode::ThreadSafe>(SceneQueryDirections);}
 auto J=MakeShared<FVirtualLidarSceneQueryJob,ESPMode::ThreadSafe>();J->Scene=ScheduledRayQuery;J->Buffer=SceneQueryBuffer;J->Pose=ScheduledScanTransform;J->Profile=DeviceProfile;J->Width=ScheduledScanWidth;J->Height=ScheduledScanHeight;J->Range=MaxDistance;J->Frame=FrameId+1;J->Generation=ScheduledGeneration;
 if(bSceneQueryDirectionsCached)J->Directions=SharedSceneQueryDirections;else {J->Horizontal=ScheduledHorizontalAnglesDegrees;J->Vertical=ScheduledVerticalAnglesDegrees;}
 SceneQueryJob=J;
}
bool UVirtualLidarScanComponent::PollSceneQueryBatch()
{
 auto J=SceneQueryJob;if(!J)return true;
 if(J->Generation!=ScheduledGeneration||J->Cancelled.load(std::memory_order_acquire))return false;
 if(J->ReadyRays.load(std::memory_order_acquire)>ScheduledNextRayIndex)return true;
 if(J->Started)return false;
 bool Expected=false;if(!J->Buffer->Busy.compare_exchange_strong(Expected,true,std::memory_order_acq_rel))return false;
 J->Started=true;J->Buffer->Hits.SetNumUninitialized(J->Width*J->Height,false);J->ChunkCount=FMath::DivideAndRoundUp(J->Width*J->Height,256);J->Chunks=std::make_unique<std::atomic<bool>[]>(J->ChunkCount);for(int I=0;I<J->ChunkCount;++I)J->Chunks[I].store(false);
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
   int Old=J->ReadyRays.load(std::memory_order_acquire);
   for(;;){int Prefix=Old/256;while(Prefix<J->ChunkCount&&J->Chunks[Prefix].load(std::memory_order_acquire))++Prefix;const int New=FMath::Min(Prefix*256,J->Width*J->Height);if(New<=Old||J->ReadyRays.compare_exchange_weak(Old,New,std::memory_order_acq_rel))break;}
  },EParallelForFlags::BackgroundPriority|EParallelForFlags::Unbalanced);
  J->GeometryMs.store((FPlatformTime::Seconds()-Started)*1000,std::memory_order_release);
  J->Ready.store(!J->Cancelled.load(std::memory_order_acquire),std::memory_order_release);J->Buffer->Busy.store(false,std::memory_order_release);
 });
 return false;
}
bool UVirtualLidarScanComponent::ResolveSceneQueryRay(int32 Index,FVector Start,FVector End,FHitResult& Hit) const
{return SceneQueryJob&&Index<SceneQueryJob->ReadyRays.load(std::memory_order_acquire)&&SceneQueryBuffer->Hits.IsValidIndex(Index)&&ScheduledRayQuery->Resolve(Start,End,SceneQueryBuffer->Hits[Index],Hit);}
int32 UVirtualLidarScanComponent::SceneQueryReadyRays() const
{return SceneQueryJob?SceneQueryJob->ReadyRays.load(std::memory_order_acquire):0;}
void UVirtualLidarScanComponent::CancelSceneQueryBatch()
{if(SceneQueryJob)SceneQueryJob->Cancelled.store(true,std::memory_order_release);SceneQueryJob.Reset();}
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

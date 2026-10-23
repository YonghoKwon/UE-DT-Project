#include "AgentTestSetPcdPreview.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualPcdCodec.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Json.h"
#include "Async/Async.h"

FTestSetPcdFrame FAgentTestSetPcdPreview::ReadFrame(const FAgentTestSetItem& Item,int32 Index,const std::atomic<bool>& Cancel)
{
    FTestSetPcdFrame Out;Out.Index=Index;Out.Error=TEXT("저장된 PCD 또는 라벨이 올바르지 않습니다");
    const FString Root=Item.Directory/(TEXT("run_")+Item.RunId)/FString::Printf(TEXT("scene_%05d"),Index);
    if(!FPaths::IsUnderDirectory(Root,FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("TestSets")))||Cancel.load())return Out;
    FString Text;TSharedPtr<FJsonObject> Label;
    if(IFileManager::Get().FileSize(*(Root/TEXT("label.json")))>1024*1024||!FFileHelper::LoadFileToString(Text,*(Root/TEXT("label.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Label)||!Label)return Out;
    FString Scenario,Run,Schema;
    if(!Label->TryGetStringField(TEXT("schema_version"),Schema)||Schema!=TEXT("ma0t10.dataset.scene.v1")||!Label->TryGetStringField(TEXT("scenario_id"),Scenario)||Scenario!=Item.ScenarioId||!Label->TryGetStringField(TEXT("run_uuid"),Run)||Run!=Item.RunId)return Out;
    const TSharedPtr<FJsonObject>* Pose=nullptr;const TSharedPtr<FJsonObject>* Intrinsics=nullptr;
    const TArray<TSharedPtr<FJsonValue>>* XYZ=nullptr;const TArray<TSharedPtr<FJsonValue>>* Quat=nullptr;
    double Width=0,Height=0;
    if(!Label->TryGetObjectField(TEXT("lidar_pose"),Pose)||!(*Pose)->TryGetArrayField(TEXT("xyz_m"),XYZ)||XYZ->Num()!=3||!(*Pose)->TryGetArrayField(TEXT("quaternion_xyzw"),Quat)||Quat->Num()!=4||
       !Label->TryGetObjectField(TEXT("lidar_intrinsics"),Intrinsics)||!(*Intrinsics)->TryGetNumberField(TEXT("horizontal_samples"),Width)||!(*Intrinsics)->TryGetNumberField(TEXT("vertical_channels"),Height)||!(*Intrinsics)->TryGetNumberField(TEXT("max_range_m"),Out.Range)||!Label->TryGetNumberField(TEXT("scenario_elapsed_s"),Out.Elapsed))return Out;
    if(!FMath::IsFinite(Width)||!FMath::IsFinite(Height)||Width<1||Height<1||Width*Height>250000||!FMath::IsFinite(Out.Range)||Out.Range<=0||Out.Range>1000000||!FMath::IsFinite(Out.Elapsed))return Out;
    double P[3],Q[4];for(int32 I=0;I<3;++I)if(!(*XYZ)[I]->TryGetNumber(P[I])||!FMath::IsFinite(P[I]))return Out;
    for(int32 I=0;I<4;++I)if(!(*Quat)[I]->TryGetNumber(Q[I])||!FMath::IsFinite(Q[I]))return Out;
    FQuat Rotation(Q[0],Q[1],Q[2],Q[3]);if(!Rotation.IsNormalized())return Out;
    Out.Pose=FTransform(Rotation,FVector(P[0],P[1],P[2])*100);Out.Width=int32(Width);Out.Height=int32(Height);if(Out.Pose.ContainsNaN()||Out.Pose.GetLocation().GetAbsMax()>1.e12)return Out;
    TArray<uint8> Bytes;const int64 Size=IFileManager::Get().FileSize(*(Root/TEXT("lidar.pcd")));
    if(Size<1||Size>16ll*1024*1024||Cancel.load()||!FFileHelper::LoadFileToArray(Bytes,*(Root/TEXT("lidar.pcd"))))return Out;
    FVirtualPcdDocument Pcd;FString PcdError;
    if(!FVirtualPcdCodec::Decode(Bytes,Pcd,PcdError)||Pcd.PointCount>250000||!Pcd.ReadPoints(Out.Points))return Out;
    for(int32 I=0;I<Out.Points.Num();++I)
    {
        if((I&1023)==0&&Cancel.load())return Out;
        auto& V=Out.Points[I];const auto M=V.SensorLocalPositionMeters;const FVector Local(M.X*100.,-M.Y*100.,M.Z*100.);
        if(Local.SizeSquared()>FMath::Square(Out.Range*200.))return Out;
        V.WorldLocation=Out.Pose.TransformPosition(Local);V.Distance=Local.Size();V.LocalDirection=Local.GetSafeNormal();V.RangeMillimeters=FMath::RoundToInt(V.Distance*10);
    }
    auto Snapshot=MakeShared<FVirtualLidarFrameSnapshot,ESPMode::ThreadSafe>();
    Snapshot->Points=MakeShared<const TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe>(MoveTemp(Out.Points));
    Snapshot->AcquisitionTransform=Out.Pose;Snapshot->HorizontalSamples=Out.Width;Snapshot->VerticalChannels=Out.Height;Snapshot->MaxDistanceCm=Out.Range*100;
    FString FrameId,Utc;
    if(!Label->TryGetStringField(TEXT("lidar_frame_id"),FrameId)||!LexTryParseString(Snapshot->FrameId,*FrameId)||!Label->TryGetStringField(TEXT("lidar_utc"),Utc))return Out;
    FDateTime Timestamp;if(!FDateTime::ParseIso8601(*Utc,Timestamp))return Out;
    const int64 Ticks=(Timestamp-FDateTime(1970,1,1)).GetTicks();if(Ticks<0||Ticks>MAX_int64/100)return Out;
    Snapshot->AcquisitionStartUnixNanoseconds=Ticks*100;
    Out.Snapshot=Snapshot;Out.Error.Reset();return Out;
}
FAgentTestSetPcdPreview::FAgentTestSetPcdPreview()=default;
FAgentTestSetPcdPreview::~FAgentTestSetPcdPreview(){Stop();}
void FAgentTestSetPcdPreview::Queue(int32 Index)
{
    const auto Copy=Source;const auto Cancel=Cancelled;View.Loading=View.Frame==INDEX_NONE;
    Read=Async(EAsyncExecution::ThreadPool,[Copy,Index,Cancel](){return ReadFrame(Copy,Index,*Cancel);});
}
void FAgentTestSetPcdPreview::Start(UWorld* World,const FAgentTestSetItem& Item,int32 Index)
{
    Source=Item;View.Scenario=Index;View.Frames=Item.Frames;View.Playing=true;View.Message=TEXT("저장된 점군 불러오는 중");
    Scan.Reset(NewObject<UVirtualLidarScanComponent>());Scan->bAutoStartScan=false;Scan->OutputMode=EVirtualLidarOutputMode::None;Scan->bPointCloudPreviewEnabled=false;Scan->RegisterComponentWithWorld(World);
    Visualization.Reset(NewObject<UVirtualLidarVisualizationComponent>());Visualization->RegisterComponentWithWorld(World);Visualization->BindScanComponent(Scan.Get());
    auto Settings=Visualization->GetVisualizationSettings();Settings.ProjectionMode=ELidarMonitorProjectionMode::WorldTopDown;Settings.bShowWorldPointCloud=false;Settings.bShowGrid=false;Visualization->SetVisualizationSettings(Settings);Queue(0);
}
void FAgentTestSetPcdPreview::Pump(UWorld* World)
{
    if(Read.IsValid()&&Read.IsReady()){Ready=Read.Get();Read=TFuture<FTestSetPcdFrame>();}
    if(!View.Playing||!Ready.IsSet())return;
    auto& Frame=Ready.GetValue();
    if(!Frame.Error.IsEmpty()){View.Playing=false;View.Loading=false;View.Message=Frame.Error;Ready.Reset();return;}
    const double Now=FPlatformTime::Seconds();if(View.Frame!=INDEX_NONE&&Now-AppliedAt<FMath::Max(.001,Frame.Elapsed-View.Elapsed))return;
    Scan->HorizontalSamples=Frame.Width;Scan->VerticalChannels=Frame.Height;Scan->MaxDistance=Frame.Range*100;Scan->SetWorldTransform(Frame.Pose);
    Scan->SetReadOnlyPreviewFrame(Frame.Snapshot.ToSharedRef());Visualization->RefreshLatestFrame();
    View.Frame=Frame.Index;View.Points=Frame.Snapshot->Points->Num();View.Elapsed=Frame.Elapsed;View.Loading=false;AppliedAt=Now;Ready.Reset();
    View.Message=TEXT("저장된 점군 · 보기 전용");
    if(View.Frame+1<View.Frames)Queue(View.Frame+1);else {View.Playing=false;View.Message=TEXT("점군 재생 완료");}
}
void FAgentTestSetPcdPreview::SetPaused(bool Paused)
{
    if(!Paused&&View.Frame+1==View.Frames&&!Read.IsValid()&&!Ready.IsSet()){View.Frame=INDEX_NONE;View.Elapsed=0;Queue(0);}
    View.Playing=!Paused;AppliedAt=FPlatformTime::Seconds();View.Message=Paused?TEXT("점군 일시정지"):TEXT("저장된 점군 재생 중");
}
bool FAgentTestSetPcdPreview::NeedsPump() const{return Read.IsValid()||View.Playing;}
UTexture2D* FAgentTestSetPcdPreview::Texture() const{return Visualization.IsValid()?Visualization->GetPreviewTexture():nullptr;}
void FAgentTestSetPcdPreview::Stop()
{
    Cancelled->store(true);View.Playing=false;View.Loading=false;Ready.Reset();
    if(Visualization.IsValid()){Visualization->DestroyComponent();Visualization.Reset();}
    if(Scan.IsValid()){Scan->DestroyComponent();Scan.Reset();}
}

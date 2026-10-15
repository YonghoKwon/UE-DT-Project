#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Uxr15ToggleTestHelpers.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorLayoutSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "JsonObjectConverter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "ma0t10_dt/MA0T10/Slab/SlabMotionComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUxr15OptionBindingsTest,"MA0T10.UXR15.OptionBindingsAndPersistence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUxr15OptionBindingsTest::RunTest(const FString&)
{
    using namespace Uxr15ToggleTests;
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* C=World->SpawnActor<AVirtualSensorCoordinator>();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();W->SetCoordinator(C);
    auto* Camera=World->SpawnActor<AVirtualCameraSensorActor>();auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>();for(auto* Actor:TArray<AActor*>{C,Camera,Lidar})Actor->Tags.Add(TEXT("SensorTestManaged"));Camera->StopSensor();Lidar->StopSensor();C->RegisterSensorActor(Camera);C->RegisterSensorActor(Lidar);
    auto* Settings=NewObject<UVirtualSensorSettingsPanelWidget>(World);Settings->Initialize();W->RegisterOwnedPanel(ESensorToolPanelRole::Settings,Settings);Settings->BindSensorManager(C);Settings->SetVisibility(ESlateVisibility::Visible);const auto Slate=Settings->TakeWidget();
    C->SelectSensorActor(Camera);Settings->SelectTargetKind(EVirtualSensorTargetKind::Camera);Settings->SelectOwnedSettingsTab(0);
    Toggle(Slate,TEXT("정답 데이터"),this);
    TestTrue(TEXT("CAM-GT-FIX: UI checkbox enables actual capture component immediately"),Camera->CaptureComponent->bGroundTruthEnabled);
    FVirtualSensorEditableState CameraOn;Camera->ReadEditableState(CameraOn);TestTrue(TEXT("Actor readback agrees with enabled UI"),CameraOn.bCameraGroundTruth);
    Settings->ApplySelectedProfileAndQualityPreset();TestTrue(TEXT("Camera preset reapply preserves explicit GT"),Camera->CaptureComponent->bGroundTruthEnabled);
    Toggle(Slate,TEXT("정답 데이터"),this);TestFalse(TEXT("UI off disables actual component"),Camera->CaptureComponent->bGroundTruthEnabled);
    Toggle(Slate,TEXT("정답 데이터"),this);
    C->SelectSensorActor(Lidar);Settings->SelectTargetKind(EVirtualSensorTargetKind::Lidar);
    Toggle(Slate,TEXT("점별 시간차"),this);TestTrue(TEXT("point time reaches scan component"),Lidar->ScanComponent->bUseRollingPointTimestamps);
    Toggle(Slate,TEXT("움직임 왜곡"),this);TestTrue(TEXT("motion reaches scan and keeps time dependency"),Lidar->ScanComponent->bEnableMotionDistortion&&Lidar->ScanComponent->bUseRollingPointTimestamps);
    Toggle(Slate,TEXT("재질 반사"),this);TestTrue(TEXT("material reaches scan component"),Lidar->ScanComponent->bEnableMaterialResponse);
    Settings->ApplySelectedProfileAndQualityPreset();TestTrue(TEXT("LiDAR preset reapply preserves all three explicit options"),Lidar->ScanComponent->bUseRollingPointTimestamps&&Lidar->ScanComponent->bEnableMotionDistortion&&Lidar->ScanComponent->bEnableMaterialResponse);
    auto* Layout=World->GetSubsystem<UVirtualSensorLayoutSubsystem>();FVirtualSensorLayoutDocument Document,Loaded;FString Error,Json;
    TestTrue(TEXT("existing layout snapshots runtime flags"),Layout->BuildLayoutSnapshot(Document,Error));
    TestTrue(TEXT("existing v1 serializer writes settings"),FJsonObjectConverter::UStructToJsonObjectString(Document,Json));
    TestTrue(TEXT("existing v1 serializer restores settings"),FJsonObjectConverter::JsonObjectStringToUStruct(Json,&Loaded));
    Toggle(Slate,TEXT("점별 시간차"),this);TestFalse(TEXT("time off also clears dependent motion"),Lidar->ScanComponent->bUseRollingPointTimestamps||Lidar->ScanComponent->bEnableMotionDistortion);
    Toggle(Slate,TEXT("재질 반사"),this);C->SelectSensorActor(Camera);Settings->SelectTargetKind(EVirtualSensorTargetKind::Camera);Toggle(Slate,TEXT("정답 데이터"),this);
    for(const auto& Entry:Loaded.Sensors){auto* Actor=Entry.State.TargetKind==EVirtualSensorTargetKind::Camera?static_cast<AVirtualSensorActorBase*>(Camera):static_cast<AVirtualSensorActorBase*>(Lidar);TestTrue(TEXT("layout state applies through Actor transaction"),Actor->ApplyEditableState(Entry.State,Error));}
    TestTrue(TEXT("layout roundtrip restores GT and every fidelity flag"),Camera->CaptureComponent->bGroundTruthEnabled&&Lidar->ScanComponent->bUseRollingPointTimestamps&&Lidar->ScanComponent->bEnableMotionDistortion&&Lidar->ScanComponent->bEnableMaterialResponse);
    TestEqual(TEXT("layout schema version stays one"),Loaded.Version,1);TestNull(TEXT("sensor runtime settings do not migrate into UI SaveGame"),UVirtualSensorUiPreferencesSaveGame::StaticClass()->FindPropertyByName(TEXT("bCameraGroundTruth")));
    // Exercise output from the actual scan model after each UI checkbox callback.
    auto* Wall=World->SpawnActor<AStaticMeshActor>();Wall->Tags.Add(TEXT("SensorTestManaged"));auto* Mesh=Wall->GetStaticMeshComponent();Mesh->SetMobility(EComponentMobility::Movable);Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Wall->SetActorScale3D(FVector(1,200,200));Wall->Tags.Add(TEXT("Slab"));Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Mesh->SetCollisionResponseToAllChannels(ECR_Block);
    auto* Motion=NewObject<USlabMotionComponent>(Wall);Wall->AddInstanceComponent(Motion);Motion->RegisterComponent();auto MotionData=MakeShared<FSlabScenarioData,ESPMode::ThreadSafe>();FSlabScenarioRow RowA,RowB;RowA.CenterX=1000;RowA.ElapsedSec=0;RowB.CenterX=2000;RowB.ElapsedSec=1;MotionData->Rows={RowA,RowB};MotionData->DurationSec=1;Motion->OnPoseApplied.BindLambda([](const FSlabScenarioRow&,int32,double,bool){});Motion->StartPlayback(MotionData,FTransform::Identity,FVector(100,20000,20000),ESlabInputUnit::Centimeters);
    auto* Scan=Lidar->ScanComponent.Get();Lidar->SetActorLocation(FVector(0,0,10000));Scan->DeviceProfile=EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE;Scan->HorizontalSamples=16;Scan->VerticalChannels=4;Scan->HorizontalFov=2;Scan->MinVerticalAngle=-1;Scan->MaxVerticalAngle=1;Scan->ScanInterval=.05;Scan->MaxDistance=5000;Scan->FidelityMode=EVirtualSensorFidelityMode::IdealTruth;Scan->bUseProfileEchoCapability=false;Scan->bEnableRangeNoise=false;Scan->SimulationQuality=EVirtualSensorSimulationQuality::Custom;Scan->bUseRollingPointTimestamps=Scan->bEnableMotionDistortion=Scan->bEnableMaterialResponse=false;
    C->SelectSensorActor(Lidar);Settings->SelectTargetKind(EVirtualSensorTargetKind::Lidar);
    TArray<FVirtualLidarPoint> Base,Timed,Moved,Material,Restored;TArray<uint8> Heat;
    Scan->ExecuteScan(Base,Heat);TestEqual(TEXT("UI output fixture has 64 real hits"),Base.FilterByPredicate([](const auto& Point){return Point.bHit;}).Num(),64);
    Toggle(Slate,TEXT("점별 시간차"),this);Scan->ExecuteScan(Timed,Heat);TestTrue(TEXT("UI point-time produces nonzero acquisition offsets"),Timed.ContainsByPredicate([](const auto& Point){return Point.PointTimeOffsetNanoseconds>0;}));
    Toggle(Slate,TEXT("움직임 왜곡"),this);Scan->ExecuteScan(Moved,Heat);double Distortion=0;if(Moved.Num()==Base.Num())for(int32 I=0;I<Base.Num();++I)Distortion=FMath::Max(Distortion,FMath::Abs(Moved[I].WorldLocation.X-Base[I].WorldLocation.X));TestTrue(TEXT("UI motion changes actual measured geometry"),Distortion>1.0);
    Toggle(Slate,TEXT("재질 반사"),this);Scan->ExecuteScan(Material,Heat);double Signal=0;if(Material.Num()==Moved.Num())for(int32 I=0;I<Moved.Num();++I)Signal+=FMath::Abs(Material[I].NormalizedIntensity-Moved[I].NormalizedIntensity);TestTrue(TEXT("UI material changes measured optical intensity"),Signal>.01);
    Toggle(Slate,TEXT("점별 시간차"),this);Toggle(Slate,TEXT("재질 반사"),this);Scan->ExecuteScan(Restored,Heat);
    auto Pcd=[this](const TArray<FVirtualLidarPoint>& Points){FVirtualSensorFrameEnvelope Frame;Frame.SensorId=TEXT("uxr15-output");Frame.FrameId=7;Frame.TimestampUtc=FDateTime(2026,1,1);Frame.PointSnapshot=MakeShared<const TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe>(Points);FVirtualSensorStreamConfig Config;Config.PointCloudFormat=EVirtualPointCloudStreamFormat::PCD;Config.PcdDataMode=EVirtualPcdDataMode::Binary;TArray<uint8> Bytes;FString Extension,Reason;int32 Count;TestTrue(TEXT("unchanged production PCD writer accepts UI-selected output"),UVirtualSensorStreamPublisherComponent::SerializePointCloudFrame(Frame,Config,Extension,Bytes,Count,Reason));return Bytes;};
    TestTrue(TEXT("all three UI toggles off restore every original PCD byte"),Pcd(Base)==Pcd(Restored));TestTrue(TEXT("UI point-time is reflected in actual PCD output"),Pcd(Base)!=Pcd(Timed));
    auto* Data=NewObject<UVirtualSensorCaptureExportPanelWidget>(World);Data->Initialize();W->RegisterOwnedPanel(ESensorToolPanelRole::Data,Data);Data->BindSensorManager(C);Data->SetVisibility(ESlateVisibility::Visible);const auto DataSlate=Data->TakeWidget();
    auto* P=C->StreamPublisherComponent.Get();auto& Runtime=P->FindOrAddRuntime(EVirtualSensorStreamKind::PointCloud,TEXT("uxr15-binding"));Runtime.Config.bEnabled=true;Runtime.Config.TransportBackend=EVirtualSensorStreamTransportBackend::TcpStompHighThroughput;Runtime.Status.RawOutstandingFrameCount=10;
    P->UpdatePointCloudRateControl(Runtime,100,{});TestTrue(TEXT("enabled pressure activates rate control"),Runtime.Status.bRateControlActive);
    const auto Transfer=Data->BuildOwnedLiveTab();Toggle(Transfer,TEXT("PCD 전송률 자동 조절"),this);P->UpdatePointCloudRateControl(Runtime,101,{});
    TestFalse(TEXT("UI off disables actual publisher and admission"),P->bAutoAdjustPointCloudSendRate||Runtime.Status.bRateControlActive);
    const auto Reopened=Data->BuildOwnedLiveTab();if(auto CheckBox=Check(Reopened,TEXT("PCD 전송률 자동 조절"),this))TestFalse(TEXT("rebuilding panel reads component off state"),CheckBox->IsChecked());
    Toggle(Reopened,TEXT("PCD 전송률 자동 조절"),this);P->UpdatePointCloudRateControl(Runtime,102,{});TestTrue(TEXT("UI on reactivates actual admission under pressure"),P->bAutoAdjustPointCloudSendRate&&Runtime.Status.bRateControlActive);
    TestTrue(TEXT("new publisher keeps approved default on"),NewObject<UVirtualSensorStreamPublisherComponent>()->bAutoAdjustPointCloudSendRate);P->StopAllStreams(FString());
    // Existing file-output toggles are persistent UI preferences, unlike runtime-only options.
    for(int32 I=0;I<4;++I)
    {
        C->SelectSensorActor(I<2?static_cast<AVirtualSensorActorBase*>(Camera):static_cast<AVirtualSensorActorBase*>(Lidar));
        const TCHAR* Labels[]={TEXT("JPEG 이미지"),TEXT("Camera JSON"),TEXT("포인트 클라우드"),TEXT("LiDAR JSON")};const auto File=Data->BuildOwnedFileTab();Toggle(File,Labels[I],this);
        const auto* Saved=UVirtualSensorUiPreferencesSaveGame::LoadOrCreate();
        const bool SelectionFlags[]={Data->CaptureSelection.bCameraImage,Data->CaptureSelection.bCameraPayload,Data->CaptureSelection.bPointCloud,Data->CaptureSelection.bLidarPayload};
        const bool Disk[]={Saved->bLocalCaptureCameraImage,Saved->bLocalCaptureCameraPayload,Saved->bLocalCapturePointCloud,Saved->bLocalCaptureLidarPayload};
        TestEqual(TEXT("actual UI file-output toggle is restored from existing SaveGame"),Disk[I],SelectionFlags[I]);TestEqual(TEXT("UI preference version remains six"),Saved->Version,6);Toggle(File,Labels[I],this);
    }
    const auto Dataset=Data->BuildOwnedDatasetControls();Toggle(Dataset,TEXT("데이터셋 생성 사용"),this);Toggle(Dataset,TEXT("카메라 정답 포함"),this);Toggle(Dataset,TEXT("LiDAR 사실성 포함"),this);
    TestTrue(TEXT("dataset enable/truth/fidelity reach the Start draft"),Data->DatasetDraft.Enabled&&Data->DatasetDraft.CameraTruth&&Data->DatasetDraft.LidarFidelity);
    const TArray<TPair<FString,FVirtualDatasetRange*>> Ranges={{TEXT("Slab 수평 회전"),&Data->DatasetDraft.Yaw},{TEXT("Slab 앞뒤 기울기"),&Data->DatasetDraft.Pitch},{TEXT("Slab 좌우 기울기"),&Data->DatasetDraft.Roll},{TEXT("Slab X 오프셋"),&Data->DatasetDraft.X},{TEXT("Slab Y 오프셋"),&Data->DatasetDraft.Y},{TEXT("Slab Z 오프셋"),&Data->DatasetDraft.Z},{TEXT("Slab 크기 배율"),&Data->DatasetDraft.Size},{TEXT("표면 0 기본"),&Data->DatasetDraft.Surface},{TEXT("표면 거칠기"),&Data->DatasetDraft.Roughness},{TEXT("산화도"),&Data->DatasetDraft.Oxidation},{TEXT("조명 강도"),&Data->DatasetDraft.LightIntensity},{TEXT("조명 상하 각도"),&Data->DatasetDraft.LightPitch},{TEXT("조명 수평 회전"),&Data->DatasetDraft.LightYaw},{TEXT("센서 설치 위치 오차"),&Data->DatasetDraft.SensorPosition},{TEXT("센서 설치 각도 오차"),&Data->DatasetDraft.SensorAngle}};
    for(const auto& Range:Ranges){Toggle(Dataset,Range.Key,this);TestTrue(*(Range.Key+TEXT(" reaches draft")),Range.Value->Enabled);Toggle(Dataset,Range.Key,this);TestFalse(*(Range.Key+TEXT(" off reaches draft")),Range.Value->Enabled);}
    const auto DatasetAgain=Data->BuildOwnedDatasetControls();if(auto CheckBox=Check(DatasetAgain,TEXT("데이터셋 생성 사용"),this))TestTrue(TEXT("same panel reopening preserves opt-in draft"),CheckBox->IsChecked());
    auto* Fresh=NewObject<UVirtualSensorCaptureExportPanelWidget>(World);TestFalse(TEXT("new panel starts disabled; dataset is never auto-restored from SaveGame"),Fresh->DatasetDraft.Enabled||Fresh->DatasetDraft.CameraTruth||Fresh->DatasetDraft.LidarFidelity);
    W->UnregisterPanel(Data);W->UnregisterPanel(Settings);Wall->Destroy();C->Destroy();Camera->Destroy();Lidar->Destroy();return true;
}
#endif

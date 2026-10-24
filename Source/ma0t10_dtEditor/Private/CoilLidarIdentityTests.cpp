#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorIdentity.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Scenario/CoilLoadingSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorLayoutSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPlacementSubsystem.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiPreferences.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualPointCloudStreamReceiverTC.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualPcdCodec.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/SecureHash.h"
#include "Json.h"
#include "JsonObjectConverter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilIdentityRecording,"MA0T10.CoilLoading.Identity.LegacyRecording",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilIdentityRecording::RunTest(const FString&)
{
    TestEqual(TEXT("exact legacy alias"),VirtualSensorIdentity::Canonical(TEXT("Coil.Quay.XT32")),FString(TEXT("os_lidar_coil_quay")));
    TestEqual(TEXT("custom IDs are preserved"),VirtualSensorIdentity::Canonical(TEXT("Coil.Quay.XT32.custom")),FString(TEXT("Coil.Quay.XT32.custom")));
    FVirtualSensorEditableState State;FString Error;State.TargetKind=EVirtualSensorTargetKind::Lidar;State.SensorId=VirtualSensorIdentity::LegacyCoilQuay;
    TestFalse(TEXT("old ID cannot bypass UI duplicate validation"),UVirtualSensorSettingsPanelWidget::ValidateEditableStateValues(State,{VirtualSensorIdentity::CoilQuay},Error));
    State.SensorId=VirtualSensorIdentity::CoilQuay;
    TestFalse(TEXT("new ID conflicts with a legacy-loaded peer"),UVirtualSensorSettingsPanelWidget::ValidateEditableStateValues(State,{VirtualSensorIdentity::LegacyCoilQuay},Error));
    TMap<FString,int32> BothKeys={{VirtualSensorIdentity::LegacyCoilQuay,2},{VirtualSensorIdentity::CoilQuay,4}};
    VirtualSensorIdentity::MigrateCoilQuayKey(BothKeys);
    TestEqual(TEXT("explicit canonical saved choice wins over legacy fallback"),BothKeys.FindRef(VirtualSensorIdentity::CoilQuay),4);
    TestEqual(TEXT("legacy duplicate key retired in memory"),BothKeys.Num(),1);
    const FString Directory=FPaths::ProjectSavedDir()/TEXT("Reports/COIL-LIDAR-ID/fixtures");
    IFileManager::Get().MakeDirectory(*Directory,true);
    const FString Payload=TEXT("{\"sensorId\":\"Coil.Quay.XT32\",\"frameId\":17,\"note\":\"Coil.Quay.XT32\",\"points\":[1,2,3]}");
    auto Frame=MakeShared<FJsonObject>();Frame->SetStringField(TEXT("sensorId"),TEXT("Coil.Quay.XT32"));Frame->SetStringField(TEXT("sensorType"),TEXT("virtual_lidar"));
    Frame->SetNumberField(TEXT("frameId"),17);Frame->SetStringField(TEXT("timestampUtc"),TEXT("2026-10-25T00:00:00.000Z"));Frame->SetStringField(TEXT("payloadJson"),Payload);
    auto Session=MakeShared<FJsonObject>();Session->SetArrayField(TEXT("frames"),{MakeShared<FJsonValueObject>(Frame)});
    FString Text;FJsonSerializer::Serialize(Session,TJsonWriterFactory<>::Create(&Text));const FString Path=Directory/TEXT("legacy-recording.json");
    TestTrue(TEXT("write legacy fixture"),FFileHelper::SaveStringToFile(Text,*Path));auto* Recorder=NewObject<UVirtualSensorRecorderComponent>();
    TestTrue(TEXT("production session loader accepts old ID"),Recorder->LoadSessionFromFile(Path));FVirtualSensorRecordedFrame Loaded;
    TestTrue(TEXT("loaded frame"),Recorder->GetRecordedFrame(0,Loaded));TestEqual(TEXT("recording wrapper canonical"),Loaded.SensorId,FString(VirtualSensorIdentity::CoilQuay));
    TSharedPtr<FJsonObject> Object;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Loaded.PayloadJson),Object);
    if(TestTrue(TEXT("loaded payload remains valid JSON"),Object.IsValid()))
    {
        TestEqual(TEXT("payload identity canonical"),Object->GetStringField(TEXT("sensorId")),FString(VirtualSensorIdentity::CoilQuay));
        TestEqual(TEXT("unrelated old-ID text preserved"),Object->GetStringField(TEXT("note")),FString(VirtualSensorIdentity::LegacyCoilQuay));
        TestEqual(TEXT("frame identity preserved"),int32(Object->GetNumberField(TEXT("frameId"))),17);
        TestEqual(TEXT("points preserved"),Object->GetArrayField(TEXT("points")).Num(),3);
    }
    FString Original;FFileHelper::LoadFileToString(Original,*Path);TestEqual(TEXT("archived session not rewritten"),Original,Text);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilIdentityPcd,"MA0T10.CoilLoading.Identity.LegacyPcd",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilIdentityPcd::RunTest(const FString&)
{
    const FString Utc=TEXT("2026-10-25T00:00:00.000Z");
    auto Meta=MakeShared<FJsonObject>();Meta->SetStringField(TEXT("schema"),TEXT("virtual-pointcloud.context.v1"));Meta->SetStringField(TEXT("sensor_id"),VirtualSensorIdentity::LegacyCoilQuay);
    Meta->SetStringField(TEXT("sensor_frame_id"),TEXT("17"));Meta->SetStringField(TEXT("timestamp_utc"),Utc);FString Json;FJsonSerializer::Serialize(Meta,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json));
    FVirtualLidarPoint Point;TArray<const FVirtualLidarPoint*> Points={&Point};TArray<uint8> Body;
    TestTrue(TEXT("old metadata encoded by production writer"),FVirtualPcdCodec::Encode(Points,Json,FVirtualPcdSendOptions(),Body));
    const auto Original=Body;uint8 Hash[FSHA1::DigestSize];FSHA1::HashBuffer(Body.GetData(),Body.Num(),Hash);
    TMap<FName,FString> Headers={{TEXT("schema"),TEXT("virtual-pointcloud.pcd.v1")},{TEXT("sensor-id"),VirtualSensorIdentity::LegacyCoilQuay},{TEXT("x-sensor-id"),VirtualSensorIdentity::CoilQuay},
        {TEXT("frame-id"),TEXT("17")},{TEXT("point-count"),TEXT("1")},{TEXT("source-point-count"),TEXT("1")},{TEXT("filter-revision"),TEXT("0")},
        {TEXT("checksum"),BytesToHex(Hash,FSHA1::DigestSize).ToLower()},{TEXT("acquisition-profile"),TEXT("fixture")},{TEXT("timestamp-utc"),Utc},{TEXT("content-type"),TEXT("application/vnd.pcd")}};
    auto Parsed=StaticCastSharedPtr<FVirtualPointCloudStreamReceiverData>(UVirtualPointCloudStreamReceiverTC::ParseBinaryPcdToStruct(Body,Headers));
    TestTrue(TEXT("mixed old/new aliases and old embedded identity accepted with checksum validation"),Parsed&&Parsed->bValid&&Parsed->bDeepValidated);
    if(Parsed)TestEqual(TEXT("PCD read identity canonical"),Parsed->SensorId,FString(VirtualSensorIdentity::CoilQuay));
    TestTrue(TEXT("PCD archived bytes untouched"),Body==Original);
    Headers.Add(TEXT("sensor-id"),TEXT("different-device"));
    Parsed=StaticCastSharedPtr<FVirtualPointCloudStreamReceiverData>(UVirtualPointCloudStreamReceiverTC::ParseBinaryPcdToStruct(Body,Headers));
    TestTrue(TEXT("unrelated conflicting sensor remains rejected"),Parsed&&!Parsed->bValid);return true;
}

class FCoilIdentityLoad final:public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Start=FPlatformTime::Seconds();
public:
    explicit FCoilIdentityLoad(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>45){Test->AddError(TEXT("coil identity PIE timeout"));return true;}
        for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE&&FinalAcceptanceViewportReady(C.World()))
        for(TActorIterator<ACoilLoadingSimulation> It(C.World());It;++It)if(It->Lidar)
        {
            auto* Layout=C.World()->GetSubsystem<UVirtualSensorLayoutSubsystem>();
            auto* Coordinator=Layout?Layout->Coordinator():nullptr;
            if(!Coordinator||!Coordinator->StreamPublisherComponent)return false;
            auto* Sensor=It->Lidar.Get();
            Test->TestEqual(TEXT("new scene displays canonical role ID"),Sensor->GetSensorId(),FString(VirtualSensorIdentity::CoilQuay));
            auto* Publisher=Coordinator->StreamPublisherComponent.Get();
            auto Stream=Publisher->GetEffectiveStreamConfig(EVirtualSensorStreamKind::PointCloud,Sensor->GetSensorId());
            Stream.bEnabled=false;Stream.SensorId=VirtualSensorIdentity::LegacyCoilQuay;Publisher->ConfigureStream(Stream);Sensor->StopSensor();
            Test->TestEqual(TEXT("old output configuration resolves to same stream"),Publisher->GetEffectiveStreamConfig(Stream.StreamKind,VirtualSensorIdentity::CoilQuay).SensorId,FString(VirtualSensorIdentity::CoilQuay));
            FVirtualSensorEditableState State;Sensor->ReadEditableState(State);State.SensorId=VirtualSensorIdentity::LegacyCoilQuay;FString Error;
            Test->TestTrue(TEXT("legacy editable settings load through actor API"),Sensor->ApplyEditableState(State,Error));
            Test->TestEqual(TEXT("old editable ID canonicalized"),Sensor->GetSensorId(),FString(VirtualSensorIdentity::CoilQuay));
            const FString Dir=FPaths::ProjectSavedDir()/TEXT("Reports/COIL-LIDAR-ID/fixtures");IFileManager::Get().MakeDirectory(*Dir,true);
            Layout->StorageRootOverrideForTests=Dir;
            FVirtualSensorLayoutDocument D;D.Name=TEXT("legacy");D.MapPackage=UVirtualSensorPlacementSubsystem::CanonicalMap(C.World());
            auto& Entry=D.Sensors.AddDefaulted_GetRef();Entry.ActorPath=UVirtualSensorLayoutSubsystem::CanonicalActorPath(Sensor);Entry.State=State;
            FString Json;FJsonObjectConverter::UStructToJsonObjectString(D,Json);const FString Path=Dir/(FMD5::HashAnsiString(*(D.MapPackage+TEXT("|legacy")))+TEXT(".layout.v1.json"));FFileHelper::SaveStringToFile(Json,*Path);
            const FString Id=Layout->PreviewLayoutRestore(TEXT("legacy"));
            Test->TestEqual(TEXT("legacy layout validated before application"),int32(Layout->GetOperationStatus(Id).State),int32(EVirtualSensorLayoutOperation::ReadyToApply));
            Test->TestTrue(TEXT("legacy layout restored"),Layout->ApplyLayoutRestore(Id,Error));Test->TestEqual(TEXT("restored layout ID canonical"),Sensor->GetSensorId(),FString(VirtualSensorIdentity::CoilQuay));
            const auto Duplicate=D.Sensors[0];D.Sensors.Add(Duplicate);D.Sensors.Last().State.SensorId=VirtualSensorIdentity::CoilQuay;
            FJsonObjectConverter::UStructToJsonObjectString(D,Json);FFileHelper::SaveStringToFile(Json,*Path);
            Test->TestEqual(TEXT("old/new aliases cannot create duplicate sensor identities"),int32(Layout->GetOperationStatus(Layout->PreviewLayoutRestore(TEXT("legacy"))).State),int32(EVirtualSensorLayoutOperation::Failed));
            auto* Placement=C.World()->GetSubsystem<UVirtualSensorPlacementSubsystem>();Placement->StorageRootOverrideForTests=Dir;Placement->bLoaded=false;
            FVirtualSensorPoseBookmark B;B.BookmarkId=FGuid::NewGuid().ToString();B.Name=TEXT("legacy pose");B.SensorKind=EVirtualSensorKind::Lidar;B.SensorId=VirtualSensorIdentity::LegacyCoilQuay;B.MapPackage=D.MapPackage;
            auto Root=MakeShared<FJsonObject>();Root->SetStringField(TEXT("schema"),TEXT("ma0t10.sensor-poses.v1"));Root->SetNumberField(TEXT("version"),1);Root->SetStringField(TEXT("mapPackage"),D.MapPackage);
            Root->SetArrayField(TEXT("bookmarks"),{MakeShared<FJsonValueObject>(FJsonObjectConverter::UStructToJsonObject(B))});
            FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(Dir/(FMD5::HashAnsiString(*D.MapPackage)+TEXT(".poses.v1.json"))));
            const auto Bookmarks=Placement->GetSensorPoseBookmarks(Sensor);Test->TestEqual(TEXT("old bookmark associated with canonical sensor"),Bookmarks.Num(),1);
            if(Bookmarks.Num())Test->TestEqual(TEXT("bookmark read identity canonical"),Bookmarks[0].SensorId,FString(VirtualSensorIdentity::CoilQuay));
            // Exercise the actual SaveGame slot loader with a backed-up, temporarily owned fixture.
            const FString Slot=UVirtualSensorUiPreferencesSaveGame::SlotName,SavePath=FPaths::ProjectSavedDir()/TEXT("SaveGames")/(Slot+TEXT(".sav"));
            TArray<uint8> OriginalBytes;const bool HadSave=UGameplayStatics::DoesSaveGameExist(Slot,0);
            if(HadSave&&!FFileHelper::LoadFileToArray(OriginalBytes,*SavePath)){Test->AddError(TEXT("personal SaveGame backup failed"));return true;}
            auto* Save=NewObject<UVirtualSensorUiPreferencesSaveGame>();FVirtualPcdSendOptions Options;Options.SendDivisor=2;Save->PcdSendOptionsBySensor.Add(VirtualSensorIdentity::LegacyCoilQuay,Options);
            Test->TestTrue(TEXT("legacy SaveGame fixture"),UGameplayStatics::SaveGameToSlot(Save,Slot,0));TArray<uint8> LegacyBytes;FFileHelper::LoadFileToArray(LegacyBytes,*SavePath);
            auto* Loaded=UVirtualSensorUiPreferencesSaveGame::LoadOrCreate();
            Test->TestTrue(TEXT("actual SaveGame load migrates old output key"),Loaded&&Loaded->PcdSendOptionsBySensor.Contains(VirtualSensorIdentity::CoilQuay)&&!Loaded->PcdSendOptionsBySensor.Contains(VirtualSensorIdentity::LegacyCoilQuay));
            if(Loaded&&Loaded->PcdSendOptionsBySensor.Contains(VirtualSensorIdentity::CoilQuay))Test->TestEqual(TEXT("stored output choice preserved"),Loaded->PcdSendOptionsBySensor.FindChecked(VirtualSensorIdentity::CoilQuay).SendDivisor,2);
            TArray<uint8> AfterBytes;FFileHelper::LoadFileToArray(AfterBytes,*SavePath);Test->TestTrue(TEXT("read-only migration does not rewrite SaveGame"),LegacyBytes==AfterBytes);
            if(HadSave)Test->TestTrue(TEXT("personal SaveGame bytes restored"),FFileHelper::SaveArrayToFile(OriginalBytes,*SavePath));else Test->TestTrue(TEXT("owned fixture slot removed"),UGameplayStatics::DeleteGameInSlot(Slot,0));
            return true;
        }
        return false;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoilIdentitySettings,"MA0T10.CoilLoading.Identity.LegacySettingsRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCoilIdentitySettings::RunTest(const FString&)
{
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/CoilLoadingTestMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FCoilIdentityLoad(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif

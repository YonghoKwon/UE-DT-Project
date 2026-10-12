#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/UI/SlabInformationCard.h"
#include "ma0t10_dt/MA0T10/UI/SlabProgressPanelWidget.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSurfaceResponseComponent.h"
#include "ma0t10_dt/MA0T10/Scenario/SlabRunResultsSubsystem.h"
#include "Json.h"
#include "ma0t10_dt/MA0T10/Slab/SlabUiRegistry.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabInformationCardTest,"MA0T10.UXR08.InformationUsesSourceRowAndMissingValues",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabInformationCardTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Slab=World->SpawnActor<ASlabActor>();
    TestTrue(TEXT("unbound data is explicitly missing"),SlabInformationCard::Build(Slab).Contains(TEXT("소재번호: —")));
    FSlabScenarioDataPtr Data;FString Error;TestTrue(TEXT("source parsed"),FSlabScenarioCodec::Parse(FSlabScenarioCodec::MakeSyntheticJson(FGuid::NewGuid().ToString()),Data,Error));
    Slab->Scenario=Data;Slab->Status.State=ESlabSimulationState::Paused;Slab->Status.ScenarioUUID=Data->ScenarioUUID;Slab->Status.RowIndex=400;Slab->Status.RowCount=600;Slab->Status.ElapsedSec=20;Slab->Status.DurationSec=30;Slab->Status.Progress=2.f/3;
    Slab->DimensionUnit=ESlabInputUnit::Millimeters;Slab->PositionUnit=ESlabInputUnit::Centimeters;
    const auto& R=Data->Rows[400];Slab->InitialDimensions=FVector(R.Length,R.Width,R.Thickness);
    const FString Card=SlabInformationCard::Build(Slab);
    for(const FString& Value:{R.MtlNo,FString::Printf(TEXT("%.2f° / %.2f°"),R.LeftAngle,R.RightAngle),FString(TEXT("행 401/600")),FString(TEXT("예 (원본 값)")),FString(TEXT("66.7 %")),FString(TEXT("0.0 cm/s (일시정지)"))})TestTrue(TEXT("card agrees with immutable source row and display precision"),Card.Contains(Value));
    const auto Basic=SlabInformationCard::BuildRows(Slab,false);
    TestEqual(TEXT("dimensions use grouping and omit empty fractional digits"),Basic[1].Value,FString(TEXT("10,830 × 1,100 × 250 mm")));
    TestEqual(TEXT("weight retains source unit with grouping"),Basic[2].Value,FString(TEXT("23,290 (원본 단위)")));
    TestEqual(TEXT("basic card contains twelve label/value rows"),Basic.Num(),12);
    TestFalse(TEXT("source position/speed stay out of basic card"),Basic.ContainsByPredicate([](const auto& Row){return Row.bSourceDetail||Row.Label.ToString().Contains(TEXT("원본 행 위치"))||Row.Label.ToString().Contains(TEXT("원본 행 속도"));}));
    TestTrue(TEXT("source position and speed are explicitly labeled in details"),Card.Contains(TEXT("원본 행 위치:"))&&Card.Contains(TEXT("원본 행 속도:")));
    auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* P=NewObject<USlabProgressPanelWidget>(World);W->RegisterOwnedPanel(ESensorToolPanelRole::SlabProgress,P);P->BindSlabActor(Slab);P->SetVisibility(ESlateVisibility::Hidden);P->CachedInformationText=TEXT("hidden card sentinel");P->NativeTick(FGeometry(),.3);
    TestEqual(TEXT("hidden information card is not formatted"),P->CachedInformationText,FString(TEXT("hidden card sentinel")));
    TSharedPtr<FJsonObject> Json;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Data->OriginalJson),Json);
    for(const auto& Value:Json->GetArrayField(TEXT("DATA_MAP"))){Value->AsObject()->RemoveField(TEXT("center_y"));Value->AsObject()->RemoveField(TEXT("is_meandering"));}
    FString Missing;FJsonSerializer::Serialize(Json.ToSharedRef(),TJsonWriterFactory<>::Create(&Missing));FSlabScenarioDataPtr MissingData;TestTrue(TEXT("optional omissions remain valid input"),FSlabScenarioCodec::Parse(Missing,MissingData,Error));Slab->Scenario=MissingData;
    const FString MissingCard=SlabInformationCard::Build(Slab);TestTrue(TEXT("absent Y is not reported as zero"),MissingCard.Contains(TEXT("Y —")));TestTrue(TEXT("absent meandering is not reported as a decision"),MissingCard.Contains(TEXT("사행 여부: —")));
    W->UnregisterPanel(P);Slab->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabSurfacePresetTest,"MA0T10.UXR08.SurfaceOptInAndOptionalRunRecord",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabSurfacePresetTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Slab=World->SpawnActor<ASlabActor>();FString Reason;
    const float Rough=Slab->Roughness,Emissive=Slab->EmissiveStrength;
    TestTrue(TEXT("default/link-off is a no-op"),Slab->ApplySurfacePreset(ESlabSurfacePreset::Default,false,Reason));TestNull(TEXT("default creates no optical component"),Slab->FindComponentByClass<UVirtualLidarSurfaceResponseComponent>());
    auto* Existing=NewObject<UVirtualLidarSurfaceResponseComponent>(Slab);Existing->RegisterComponent();Existing->Reflectivity940Nm=.31f;Existing->IntensityGain=2;
    TestTrue(TEXT("hot preset uses existing material parameters"),Slab->ApplySurfacePreset(ESlabSurfacePreset::HotRed,false,Reason));TestEqual(TEXT("link off preserves reflectivity"),Existing->Reflectivity940Nm,.31f);
    Existing->Reflectivity940Nm=.47f;Slab->ApplySurfacePreset(ESlabSurfacePreset::CooledSteel,false,Reason);TestEqual(TEXT("visual-only changes do not overwrite later optical edits"),Existing->Reflectivity940Nm,.47f);
    Slab->ApplySurfacePreset(ESlabSurfacePreset::HotRed,true,Reason);TestEqual(TEXT("explicit link applies preset response"),Existing->Reflectivity940Nm,.08f);TestEqual(TEXT("link preserves existing intensity gain"),Existing->IntensityGain,2.f);
    Slab->ApplySurfacePreset(ESlabSurfacePreset::Default,false,Reason);TestEqual(TEXT("opt-out restores response at opt-in time"),Existing->Reflectivity940Nm,.47f);TestEqual(TEXT("default restores original roughness"),Slab->Roughness,Rough);TestEqual(TEXT("default restores original emissive"),Slab->EmissiveStrength,Emissive);
    TestTrue(TEXT("dedicated material enables oxidation scale without replacing Plain"),Slab->CanApplySurfacePreset(ESlabSurfacePreset::OxidationScale,Reason));
    auto* Results=World->GetSubsystem<USlabRunResultsSubsystem>();const FString Run=FGuid::NewGuid().ToString();Results->BeginRun(Run,TEXT("source"),ESlabExecutionPolicy::ObservationAllowed,FVirtualSlabSensorOutputSelection::ObservationOnly());FSlabRunDeliverySummary Summary;Results->GetRunResult(Run,Summary);
    TestFalse(TEXT("default result JSON has no new optional field"),Results->SerializeSummary(Summary).Contains(TEXT("surfaceAppearance")));
    FSlabSurfaceAppearanceRecord A;A.Preset=TEXT("hot-red");A.bLidarLinked=true;A.bHasOpticalResponse=true;A.Reflectivity940Nm=.08f;A.IntensityGain=2;Results->RecordSurfaceAppearance(Run,A);Results->RecordSurfaceAppearance(Run,A);Results->GetRunResult(Run,Summary);
    TestEqual(TEXT("duplicate start handoff does not count as a change"),Summary.SurfaceAppearance.ChangeCount,0);
    TSharedPtr<FJsonObject> Json;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Results->SerializeSummary(Summary)),Json);auto Surface=Json->GetObjectField(TEXT("surfaceAppearance"))->GetObjectField(TEXT("latest"));
    TestEqual(TEXT("optional record retains used preset"),Surface->GetStringField(TEXT("preset")),FString(TEXT("hot-red")));TestTrue(TEXT("record retains explicit linkage"),Surface->GetBoolField(TEXT("lidarLinked")));TestTrue(TEXT("record identifies intensity response change"),FMath::IsNearlyEqual(Surface->GetNumberField(TEXT("reflectivity940Nm")),.08,1.e-6));
    TestEqual(TEXT("schema remains version one"),Json->GetStringField(TEXT("schema")),FString(TEXT("ma0t10.slab-run.v1")));Slab->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabClickRegistryTest,"MA0T10.UXR08.ClickUsesActiveWeakHost",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabClickRegistryTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Slab=World->SpawnActor<ASlabActor>();auto* Host=World->SpawnActor<AActor>();int32 Activations=0;
    SlabUiRegistry::RegisterHost(Host,[Slab](){return Slab;},true);SlabUiRegistry::RegisterActivation(Host,[&](ASlabActor* Selected){++Activations;return Selected==Slab;});
    Slab->Click();TestEqual(TEXT("DTCore Actor Click reaches the project UI adapter"),Activations,1);
    SlabUiRegistry::UnregisterHost(Host);Slab->Click();TestEqual(TEXT("inactive/ended host cannot reactivate its panel"),Activations,1);
    Host->Destroy();Slab->Click();TestEqual(TEXT("destroyed weak host receives no callback"),Activations,1);Slab->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabGpuLinkGuardTest,"MA0T10.UXR08.GpuLinkDoesNotAddSemanticCapture",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabGpuLinkGuardTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Slab=World->SpawnActor<ASlabActor>();auto* Lidar=World->SpawnActor<AVirtualLidarSensorActor>();auto* Scan=Lidar->ScanComponent.Get();Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::GpuDepthProjection;Scan->ViewMode=EVirtualLidarViewMode::DepthGradient;Scan->bEnableSemanticClassification=true;
    FString Reason;const auto Backend=Scan->GetResolvedAcquisitionBackend(Reason);
    if(Backend!=EVirtualLidarAcquisitionBackend::GpuDepthProjection){AddError(TEXT("D3D12 fixture must resolve the real GPU backend"));Slab->Destroy();Lidar->Destroy();return false;}
    TestFalse(TEXT("default view does not silently request a new semantic capture"),Scan->IsSemanticCaptureRequested());
    TestFalse(TEXT("unsupported link is rejected atomically"),Slab->ApplySurfacePreset(ESlabSurfacePreset::HotRed,true,Reason));TestFalse(TEXT("rejected link does not change appearance"),Slab->GetHotAppearance());TestFalse(TEXT("rejected link does not change view"),Scan->ViewMode==EVirtualLidarViewMode::ActorClassColor);TestNull(TEXT("rejected link creates no response component"),Slab->FindComponentByClass<UVirtualLidarSurfaceResponseComponent>());
    Scan->ViewMode=EVirtualLidarViewMode::ActorClassColor;TestTrue(TEXT("explicit existing capture demand enables link"),Slab->ApplySurfacePreset(ESlabSurfacePreset::HotRed,true,Reason));Slab->ApplySurfacePreset(ESlabSurfacePreset::Default,false,Reason);
    for(int32 I=0;I<3;++I){TestTrue(TEXT("repeated opt-in creates only its own response"),Slab->ApplySurfacePreset(ESlabSurfacePreset::HotRed,true,Reason));TestTrue(TEXT("repeated opt-out removes only owned response"),Slab->ApplySurfacePreset(ESlabSurfacePreset::Default,false,Reason));TestNull(TEXT("no leaked optical component"),Slab->FindComponentByClass<UVirtualLidarSurfaceResponseComponent>());}
    Slab->Destroy();Lidar->Destroy();return true;
}
#endif

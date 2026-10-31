#include "DTCoreContractListener.h"
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Crane/CraneDataSyncComp.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Core/DTCoreSettings.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPanelWidgetBase.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Json.h"
#include "Engine/World.h"

class FDTCoreCallbackCommand : public IAutomationLatentCommand
{
    TFunction<bool()> Callback;
public:
    explicit FDTCoreCallbackCommand(TFunction<bool()> In) : Callback(MoveTemp(In)) {}
    bool Update() override { return Callback(); }
};

bool UDTCoreContractListener::CompileBlueprintForValidation(UBlueprint* Blueprint)
{
    if (!Blueprint) return false;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
    return Blueprint->Status != BS_Error && Blueprint->GeneratedClass != nullptr;
}

void UDTCoreContractListener::PrepareFinalAcceptanceEditor()
{
    auto* S=GetMutableDefault<UDTCoreSettings>();S->WebSocketUrl=TEXT("ws://127.0.0.1:61616");S->WebSocketLogin=TEXT("artemis");S->WebSocketPasscode=TEXT("artemis");S->WebSocketTopics.AddUnique(TEXT("topic.scenario"));
    auto* P=GetMutableDefault<ULevelEditorPlaySettings>();P->NewWindowWidth=1286;P->NewWindowHeight=760;
    UE_LOG(LogTemp,Display,TEXT("Acceptance GUI process-local settings configured; no SaveConfig"));
}

FString UDTCoreContractListener::GetFinalAcceptanceRuntimeState()
{
    UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)W=C.World();
    if(!W)return TEXT("{\"pie\":false}");
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("pie"),true);
    auto* View=W->GetGameInstance()->GetGameViewportClient();const auto Size=View&&View->Viewport?View->Viewport->GetSizeXY():FIntPoint::ZeroValue;
    J->SetNumberField(TEXT("width"),Size.X);J->SetNumberField(TEXT("height"),Size.Y);
    auto* Workspace=W->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
    if(auto* C=Workspace->GetCoordinator())if(auto* A=C->GetSelectedSensorActor())
    {J->SetStringField(TEXT("sensor"),A->GetSensorId());J->SetStringField(TEXT("pose"),A->GetActorTransform().ToString());J->SetBoolField(TEXT("interactive"),A->IsInteractiveManipulationActive());J->SetNumberField(TEXT("frame"),A->GetSensorRuntimeStatus().FrameId);}
    if(auto* PC=W->GetFirstPlayerController())J->SetStringField(TEXT("player_view"),PC->GetControlRotation().ToString());
    TArray<TSharedPtr<FJsonValue>> Panels;for(int32 I=0;I<6;++I)if(auto* P=Workspace->GetOwnedPanel(static_cast<ESensorToolPanelRole>(I)))
    {auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("role"),I);R->SetBoolField(TEXT("open"),Workspace->IsPanelOpen(static_cast<ESensorToolPanelRole>(I)));R->SetBoolField(TEXT("visible"),P->IsVisible());R->SetStringField(TEXT("geometry"),P->GetCachedGeometry().GetLocalSize().ToString());Panels.Add(MakeShared<FJsonValueObject>(R));}
    J->SetArrayField(TEXT("panels"),Panels);FString Text;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Text));return Text;
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorCameraCoherenceUnitTest,"MA0T10.SensorFiles.CameraAcquisitionSnapshot",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorCameraCoherenceUnitTest::RunTest(const FString&)
{
    auto Camera=MakeShared<TStrongObjectPtr<UVirtualCameraCaptureComponent>>(NewObject<UVirtualCameraCaptureComponent>());
    auto* C=Camera->Get(); C->CaptureMode=EVirtualCameraCaptureMode::Payload; C->OutputMode=EVirtualCameraOutputMode::None;
    FVirtualCameraPayloadSnapshot Snapshot; Snapshot.FrameId=17;Snapshot.SensorId=TEXT("snapshot-fixture");
    Snapshot.Width=8;Snapshot.Height=8;Snapshot.Location=FVector(10,20,30);Snapshot.TimestampUtc=FDateTime(2026,10,2,12,0,0);
    C->ScheduledAcquisitionSnapshots.Add(17,Snapshot);C->EncodeOrder.Add(17);
    C->SetWorldLocation(FVector(900,800,700));
    TArray<FColor> Pixels;Pixels.Init(FColor::Blue,64);
    TestTrue(TEXT("delayed encode accepted"),C->StartScheduledEncode(MoveTemp(Pixels),8,8,17,FPlatformTime::Seconds()));
    const double Start=FPlatformTime::Seconds();
    ADD_LATENT_AUTOMATION_COMMAND(FDTCoreCallbackCommand([this,Camera,Snapshot,Start]()
    {
        if(FPlatformTime::Seconds()-Start>5){AddError(TEXT("JPEG encode did not finish"));return true;}
        if(!Camera->Get()->GetLastJpegSnapshot().IsValid())return false;
        const auto& Result=Camera->Get()->GetLastCompletedAcquisition();
        TestEqual(TEXT("encode retains acquisition pose rather than current pose"),Result.Location,Snapshot.Location);
        TestEqual(TEXT("acquisition UTC survives encode"),Camera->Get()->GetRuntimeStatus().LastUpdateUtc,Snapshot.TimestampUtc);
        TestEqual(TEXT("acquisition frame id survives encode"),Camera->Get()->GetRuntimeStatus().FrameId,int64(17));
        UVirtualCameraCaptureComponent::FPendingReadbackRequest Missing;Missing.FrameId=18;
        Camera->Get()->PendingReadbackRequests.Add(Missing);Camera->Get()->EncodeOrder.Add(18);
        Camera->Get()->LastFileAcquisition.FrameId=19;
        Camera->Get()->QueuePendingGpuReadbacks();
        TestTrue(TEXT("old metadata request cannot wait for a newer target"),Camera->Get()->PendingReadbackRequests.IsEmpty());
        TestFalse(TEXT("failed old frame is removed from ordered completion queue"),Camera->Get()->EncodeOrder.Contains(18));
        return true;
    }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreCraneContractTest, "MA0T10.DTCoreIntegration.CraneDispatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDTCoreCraneContractTest::RunTest(const FString&)
{
    auto* Sync = NewObject<UCraneDataSyncComp>();
    auto* Listener = NewObject<UDTCoreContractListener>();
    Sync->OnCranePositionChanged.AddDynamic(Listener, &UDTCoreContractListener::ReceivePosition);
    auto Data = MakeShared<FCraneStateData>();
    Data->CraneId = TEXT("compatibility-fixture");
    Data->Position.AnchorPosition = 12.5f;
    Data->Position.HookRopeHeight = 33.0f;
    Sync->OnReceiveData(Data);
    TestEqual(TEXT("new int32 type dispatch emits one event"), Listener->ReceivedCount, 1);
    TestEqual(TEXT("position is preserved"), Listener->LastPosition.AnchorPosition, 12.5f);
    TestEqual(TEXT("old CraneState numerical meaning retained"), Data->GetType(), 1);
    struct FOtherData : FDxDataBase { int32 GetType() const override { return 1001; } };
    Sync->OnReceiveData(MakeShared<FOtherData>());
    Sync->OnReceiveData(nullptr);
    TestEqual(TEXT("other/null messages cannot dispatch as Crane"), Listener->ReceivedCount, 1);
    return true;
}

#endif

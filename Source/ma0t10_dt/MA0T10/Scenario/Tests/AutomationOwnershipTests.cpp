#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Scenario/Automation/AutomationControlSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "UObject/StrongObjectPtr.h"

class FAutomationOwnershipCommand final:public IAutomationLatentCommand
{
public:
    explicit FAutomationOwnershipCommand(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        if(Stage==0)
        {
            W=FAutomationEditorCommonUtils::CreateNewMap();M=W->SpawnActor<AVirtualSensorCoordinator>();A=W->SpawnActor<AVirtualLidarSensorActor>();M->RegisterSensorActor(A);
            auto* T=M->SharedTransportComponent.Get();auto P=T->GetTransportProfile();P.BrokerUrl=TEXT("tcp://127.0.0.1:1");T->ConfigureTransportProfile(P);T->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
            Control.Reset(NewObject<UAutomationControlSubsystem>(W));
            if(!Start())return Finish();
            // The normal UI's Stop changes this same publisher revision.
            M->StreamPublisherComponent->StopStream(EVirtualSensorStreamKind::PointCloud,A->GetSensorId());Started=FPlatformTime::Seconds();Stage=1;return false;
        }
        if(FPlatformTime::Seconds()-Started<.3)return false;
        const auto Status=Control->GetStatus();Test->TestTrue(TEXT("external UI edit completes operation instead of leaving it busy"),Status->GetBoolField(TEXT("terminal")));Test->TestFalse(TEXT("external changes are not reported as success"),Status->GetBoolField(TEXT("success")));
        if(Stage==1)
        {
            Test->TestFalse(TEXT("external stop is preserved"),M->StreamPublisherComponent->IsStreamEnabled(EVirtualSensorStreamKind::PointCloud,A->GetSensorId()));
            Test->TestTrue(TEXT("UI stop does not let API stop the user's continuing measurement"),A->IsSensorRunning());A->StopSensor();
            if(!Start())return Finish();FVirtualSensorEditableState S;A->ReadEditableState(S);S.LidarScanInterval=.2f;FString Error;Test->TestTrue(TEXT("ordinary Actor configuration edit accepted"),A->ApplyEditableState(S,Error));Started=FPlatformTime::Seconds();Stage=2;return false;
        }
        Test->TestFalse(TEXT("own route closed after a sensor configuration change"),M->StreamPublisherComponent->IsStreamEnabled(EVirtualSensorStreamKind::PointCloud,A->GetSensorId()));
        FVirtualSensorEditableState S;A->ReadEditableState(S);Test->TestTrue(TEXT("external setting is never restored to a stale value"),FMath::IsNearlyEqual(S.LidarScanInterval,.2f));Test->TestTrue(TEXT("externally edited measurement remains running"),A->IsSensorRunning());return Finish();
    }
private:
    bool Start()
    {
        auto Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("sensor_id"),A->GetSensorId());int32 Code=0;
        const auto R=Control->Dispatch(TEXT("POST"),TEXT("/v1/pcd/start"),Body,Code);Test->TestEqual(TEXT("PCD control request admitted"),Code,202);return Code==202;
    }
    bool Finish()
    {if(Control.IsValid()){Control->Deinitialize();Control.Reset();}if(A)A->StopSensor();if(W)W->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>()->StopHighThroughputTransport();if(M)M->Destroy();if(A)A->Destroy();return true;}
    FAutomationTestBase* Test;UWorld* W=nullptr;AVirtualSensorCoordinator* M=nullptr;AVirtualLidarSensorActor* A=nullptr;TStrongObjectPtr<UAutomationControlSubsystem> Control;int32 Stage=0;double Started=0;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAutomationOwnershipTest,"MA0T10.Automation.ExternalUiOwnership",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAutomationOwnershipTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FAutomationOwnershipCommand(this));return true;}
#endif

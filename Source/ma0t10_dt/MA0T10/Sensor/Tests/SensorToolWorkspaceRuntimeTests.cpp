#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiHostActor.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
namespace {
class FWorkspaceRuntimeCheck : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	double Start=FPlatformTime::Seconds(); int32 Step=0;
	TWeakObjectPtr<UVirtualSensorMonitorPanelWidget> Monitor;
	TSharedPtr<SWidget> OriginalSlate;
public:
	explicit FWorkspaceRuntimeCheck(FAutomationTestBase* T):Test(T){}
	bool Update() override
	{
		if(FPlatformTime::Seconds()-Start>20){Test->AddError(TEXT("Workspace runtime timeout"));return true;}
		UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();if(!World)return false;
		auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=W->GetCoordinator();if(!C)return false;
		if(Step==0)
		{
			if(FPlatformTime::Seconds()-Start<3)return false;
			W->ResetOwnedWorkspaceLayout();Monitor=Cast<UVirtualSensorMonitorPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Monitor));
			if(!Monitor.IsValid()){Test->AddError(TEXT("No owned monitor"));return true;}
			OriginalSlate=Monitor->GetCachedWidget();
			C->SetViewMode(EVirtualSensorViewMode::Camera);
			FVirtualSensorCaptureSelection S;S.bCameraImage=true;S.bCameraPayload=false;S.bLidarPayload=false;S.bPointCloud=false;
			Monitor->ConfigureLocalCapture(S);Monitor->CaptureConfiguredOutputsOnce();
			W->SetPanelOpen(ESensorToolPanelRole::Monitor,false);Step=1;return false;
		}
		if(Step==1)
		{
			if(Monitor->IsConfiguredCapturePending())return false;
			TArray<FString> Images;const FString Directory=Monitor->GetLocalCaptureSessionDirectory();
			if(!Directory.IsEmpty())IFileManager::Get().FindFilesRecursive(Images,*Directory,TEXT("*.jpg"),true,false);
			if(Images.IsEmpty())return false;
			Test->TestTrue(TEXT("hidden monitor completes async camera save"),Images.Num()>0);
			W->SetPanelOpen(ESensorToolPanelRole::Monitor,true);
			Test->TestTrue(TEXT("show does not rebuild Slate instance"),OriginalSlate==Monitor->GetCachedWidget());
			W->SetPanelOpen(ESensorToolPanelRole::Settings,true);
			auto* Settings=Cast<UVirtualSensorSettingsPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Settings));
			Settings->SetSensorManipulationEnabled(true);W->SetPanelOpen(ESensorToolPanelRole::Settings,false);
			Test->TestFalse(TEXT("hide settings ends manipulation only"),C->GetSelectedSensorActor()->IsInteractiveManipulationActive());
			W->SetPanelOpen(ESensorToolPanelRole::Data,true);W->SetPanelOpen(ESensorToolPanelRole::Replay,true);
			Test->TestTrue(TEXT("multiple tools can stay open"),W->IsPanelOpen(ESensorToolPanelRole::Data)&&W->IsPanelOpen(ESensorToolPanelRole::Replay));
			Test->TestNotNull(TEXT("replay lazily created"),W->GetOwnedPanel(ESensorToolPanelRole::Replay));
			W->ResetOwnedPanelLayout(ESensorToolPanelRole::Monitor);const auto P=Monitor->GetCurrentPanelPosition();const auto Size=Monitor->GetPanelExpandedSize();
			Monitor->SetPanelExpandedSize(Size*.7,true);W->ResetOwnedPanelLayout(ESensorToolPanelRole::Monitor);
			Test->TestTrue(TEXT("size then anchor reset is stable"),P.Equals(Monitor->GetCurrentPanelPosition(),1)&&Size.Equals(Monitor->GetPanelExpandedSize(),1));
			W->SetPanelOpen(ESensorToolPanelRole::Replay,false);W->SetPanelOpen(ESensorToolPanelRole::Data,false);
			C->SetViewMode(EVirtualSensorViewMode::Lidar);W->SynchronizeOwnedSelection();W->SetPanelOpen(ESensorToolPanelRole::Settings,true);
			Test->TestTrue(TEXT("real PIE settings display selected LiDAR"),Settings->GetPendingState().TargetKind==EVirtualSensorTargetKind::Lidar);
			Test->TestEqual(TEXT("real PIE settings ID matches selected actor"),Settings->GetPendingState().SensorId,C->GetSelectedSensorActor()->GetSensorId());
			const FString Path=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_WORKSPACE_SCREENSHOT"));if(!Path.IsEmpty())FScreenshotRequest::RequestScreenshot(Path,true,false);
			return true;
		}
		return true;
	}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorWorkspaceRuntimeTest,"MA0T10.SensorWorkspace.Runtime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorWorkspaceRuntimeTest::RunTest(const FString&)
{
	if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_WORKSPACE_RHI"))!=TEXT("1")){AddInfo(TEXT("Workspace actual RHI test skipped; set MA0T10_WORKSPACE_RHI=1."));return true;}
	if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SensorRefactorTestMap"),true))return false;
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FWorkspaceRuntimeCheck(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif

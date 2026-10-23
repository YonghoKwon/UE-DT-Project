#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Json.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderingThread.h"

class FPcdBandwidthUiRun final:public IAutomationLatentCommand
{
	FAutomationTestBase* Test;int32 Phase=0;double Started=FPlatformTime::Seconds(),Ready=0;
	TUniquePtr<TGuardValue<bool>> Debug;
	FString Root()const{FString Path;FParse::Value(FCommandLine::Get(),TEXT("PcdBandwidthUiOutput="),Path);return Path.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/PCD-BANDWIDTH-01/progress"):Path;}
public:
	explicit FPcdBandwidthUiRun(FAutomationTestBase* T):Test(T){}
	bool Update() override
	{
		const bool Manual=FParse::Param(FCommandLine::Get(),TEXT("PcdBandwidthUiManual"));
		if(FPlatformTime::Seconds()-Started>(Manual?1800:120)){Test->AddError(TEXT("PCD bandwidth UI fixture timeout"));return true;}
		UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
		if(!FinalAcceptanceViewportReady(World))return false;
		auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* C=W->GetCoordinator();
		auto* Data=Cast<UVirtualSensorCaptureExportPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Data));auto* Bar=Cast<USensorToolToolbarWidget>(W->GetOwnedNavigation());
		if(!C||!Data||!Bar)return false;
		if(!Ready)
		{
			Debug=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
			for(auto* A:C->GetSensorActors())if(A){A->StopSensor();C->UnregisterSensorActor(A);}
			for(int32 I=0;I<3;++I)
			{
				auto* L=World->SpawnActor<AVirtualLidarSensorActor>();L->Tags.Add(TEXT("SensorTestManaged"));L->StopSensor();
				L->ScanComponent->SensorId=FString::Printf(TEXT("BANDWIDTH.OS1.%d"),I+1);L->ScanComponent->ApplyDeviceProfile(EVirtualLidarDeviceProfile::OusterOS1_64_Rev7);L->ScanComponent->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);L->ScanComponent->ScanInterval=.05f;C->RegisterSensorActor(L);
				if(I==0)C->SelectSensorActor(L);
			}
			W->ResetOwnedWorkspaceLayout();W->SetOwnedPanelFontScale(1.f);W->SetSidebarMode(true);W->SynchronizeOwnedSelection();Bar->OpenOwnedDestination(ESensorToolDestination::PcdTransfer);
			FString Error;Test->TestTrue(TEXT("Full baseline UI options"),C->SetPcdBandwidthOptions(TEXT(""),FVirtualPcdSendOptions(),true,Error));
			Ready=FPlatformTime::Seconds()+2;return false;
		}
		if(FPlatformTime::Seconds()<Ready)return false;
		if(Phase<3)
		{
			const TCHAR* Names[]={TEXT("full-100.png"),TEXT("compact-compressed-100.png"),TEXT("defaults-100.png")};
			TArray<FColor> Pixels;FIntVector Size;Test->TestTrue(TEXT("capture production owned data viewport"),FSlateApplication::Get().TakeScreenshot(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size));
			TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);IFileManager::Get().MakeDirectory(*Root(),true);FFileHelper::SaveArrayToFile(Png,*(Root()/(FString(TEXT("options-"))+Names[Phase])));
			auto Meta=MakeShared<FJsonObject>();Meta->SetNumberField(TEXT("client_width"),Size.X);Meta->SetNumberField(TEXT("client_height"),Size.Y);Meta->SetNumberField(TEXT("font_scale"),1.);
			const auto Window=FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef());Meta->SetNumberField(TEXT("window_dpi"),Window?Window->GetDPIScaleFactor():0.);
			Meta->SetBoolField(TEXT("automated_capture_not_mouse_acceptance"),true);FString Text;FJsonSerializer::Serialize(Meta,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(Root()/TEXT("capture-conditions.json")));
			// Explicit fallback: render the production controls independently of the
			// unavailable desktop monitor. This adds no product capture/readback path.
			auto* Renderer=new FWidgetRenderer(true,true);
			const auto Controls=Data->BuildPcdBandwidthOptions();auto* Target=Renderer->DrawWidget(Controls,FVector2D(720,720));
			if(Target)Renderer->DrawWidget(Target,Controls,FVector2D(720,720),0);
			FlushRenderingCommands();TArray<FColor> ControlPixels;
			Test->TestTrue(TEXT("offscreen production option controls readable"),Target&&Target->GameThread_GetRenderTargetResource()->ReadPixels(ControlPixels));
			if(Target&&ControlPixels.Num()==720*720)
			{
				TArray64<uint8> ControlPng;FImageUtils::PNGCompressImageArray(720,720,ControlPixels,ControlPng);
				FFileHelper::SaveArrayToFile(ControlPng,*(Root()/(FString(TEXT("controls-"))+Names[Phase])));
			}
			BeginCleanup(Renderer);
			if(++Phase<3){FVirtualPcdSendOptions O;O.Profile=Phase==1?EVirtualPcdFieldProfile::Compact:EVirtualPcdFieldProfile::Full;O.bCompressed=Phase==1||C->StreamPublisherComponent->bDefaultPcdCompression;FString Error;Test->TestTrue(TEXT("comparison/default UI options"),C->SetPcdBandwidthOptions(TEXT(""),O,true,Error));Ready=FPlatformTime::Seconds()+2;return false;}
		}
		if(Manual){FFileHelper::SaveStringToFile(TEXT("Production options ready; automatic preparation/captures are separate from direct input."),*(Root()/TEXT("manual-ready.txt")));return FPaths::FileExists(Root()/TEXT("manual-finish.flag"));}
		return true;
	}
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPcdBandwidthUiTest,"MA0T10.PcdBandwidth.UiRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPcdBandwidthUiTest::RunTest(const FString&)
{
	if(!FParse::Param(FCommandLine::Get(),TEXT("PcdBandwidthUi"))){AddInfo(TEXT("[OPT-IN SKIP] -PcdBandwidthUi is required for owned UI acceptance/captures"));return true;}
	if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
	ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FPcdBandwidthUiRun(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorWidget.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchMonitorHost.h"
#include "ma0t10_dt/MA0T10/UI/ShipHatchOverlay.h"
#include "ma0t10_dt/MA0T10/Scenario/ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

struct FHatch07Probe
{
    static void Check(FAutomationTestBase& T,UShipHatchMonitorWidget& M)
    {
        auto& S=*M.Simulation;const auto Revision=S.Revision();const auto Hash=S.Surface().Hash();
        const auto F=S.SensorAt(0)->ScanComponent->GetLastFrameSnapshot();const uint32 CRC=F&&F->Points?FCrc::MemCrc32(F->Points->GetData(),F->Points->Num()*sizeof(FVirtualLidarPoint)):0;
        T.TestFalse(TEXT("site mode starts off"),M.IsSiteVisualization());
        M.SetSiteVisualization(true);T.TestNotNull(TEXT("site model uses read-only render overlay"),M.SiteModel.Get());
        if(M.SiteModel)T.TestTrue(TEXT("site proxy never participates in measurement collision"),M.SiteModel->GetCollisionEnabled()==ECollisionEnabled::NoCollision&&M.SiteModel->BatchedMeshes.Num()>0);
        T.TestTrue(TEXT("green mode owns only player-view hides"),M.SiteHidden.Num()>0);
        T.TestEqual(TEXT("site visualization leaves acquisition geometry revision unchanged"),S.Revision(),Revision);T.TestEqual(TEXT("site visualization leaves cargo immutable"),S.Surface().Hash(),Hash);
        T.TestTrue(TEXT("tint never mutates acquired snapshot"),F&&F->Points&&FCrc::MemCrc32(F->Points->GetData(),F->Points->Num()*sizeof(FVirtualLidarPoint))==CRC);
        const TCHAR* Ids[]={TEXT("os_lidar_boom"),TEXT("os_lidar_cabin_01"),TEXT("os_lidar_cabin_02")};
        for(int I=0;I<3;++I){auto* A=S.SensorAt(I);T.TestEqual(TEXT("field TF identity"),A->GetSensorId(),FString(Ids[I]));T.TestTrue(TEXT("all default devices are full64 channel20Hz Ouster"),A->ScanComponent->DeviceProfile==EVirtualLidarDeviceProfile::OusterOS1_64_Rev7&&A->ScanComponent->VerticalChannels==64&&A->ScanComponent->HorizontalSamples==1024&&FMath::IsNearlyEqual(A->ScanComponent->ScanInterval,.05f));}
        M.SetFreeCamera(true);const FVector P=M.Host->ViewCamera->GetActorLocation();const FGeometry G;
        M.NativeOnKeyDown(G,FKeyEvent(EKeys::W,FModifierKeysState(),0,false,0,0));T.TestTrue(TEXT("W moves actual view camera50cm"),FMath::IsNearlyEqual(FVector::Distance(P,M.Host->ViewCamera->GetActorLocation()),50.,.01));
        M.NativeOnKeyDown(G,FKeyEvent(EKeys::S,FModifierKeysState(),0,false,0,0));T.TestTrue(TEXT("S reverses forward movement"),M.Host->ViewCamera->GetActorLocation().Equals(P,.01));
        M.NativeOnKeyDown(G,FKeyEvent(EKeys::A,FModifierKeysState(),0,false,0,0));T.TestFalse(TEXT("A moves laterally"),M.Host->ViewCamera->GetActorLocation().Equals(P,.01));M.NativeOnKeyDown(G,FKeyEvent(EKeys::D,FModifierKeysState(),0,false,0,0));T.TestTrue(TEXT("D reverses lateral movement"),M.Host->ViewCamera->GetActorLocation().Equals(P,.01));
        M.NativeOnKeyDown(G,FKeyEvent(EKeys::E,FModifierKeysState(),0,false,0,0));const auto Up=M.Host->ViewCamera->GetActorLocation();M.NativeOnKeyDown(G,FKeyEvent(EKeys::Q,FModifierKeysState(),0,false,0,0));T.TestTrue(TEXT("Q/E move vertically in world space"),FMath::IsNearlyEqual(Up.Z-M.Host->ViewCamera->GetActorLocation().Z,50.,.01));
        const auto Geometry=M.GetCachedGeometry();const auto Point=Geometry.LocalToAbsolute(FVector2D((M.SceneRect.Left+M.SceneRect.Right)*.5,(M.SceneRect.Top+M.SceneRect.Bottom)*.5));
        static const TSet<FKey> Pressed{EKeys::RightMouseButton},Released;
        const auto BeforePan=M.Host->ViewCamera->GetActorTransform();
        M.NativeOnMouseButtonDown(Geometry,FPointerEvent(0,Point,Point,Pressed,EKeys::RightMouseButton,0,FModifierKeysState()));
        M.NativeOnMouseMove(Geometry,FPointerEvent(0,Point+FVector2D(30,20),Point,Pressed,EKeys::RightMouseButton,0,FModifierKeysState()));
        M.NativeOnMouseButtonUp(Geometry,FPointerEvent(0,Point+FVector2D(30,20),Point,Released,EKeys::RightMouseButton,0,FModifierKeysState()));
        T.TestTrue(TEXT("right drag pans without rotating or excavating"),!M.Host->ViewCamera->GetActorLocation().Equals(BeforePan.GetLocation(),.01)&&M.Host->ViewCamera->GetActorQuat().Equals(BeforePan.GetRotation(),.001)&&S.Surface().Hash()==Hash);
        M.SetCameraPreset(1);T.TestFalse(TEXT("preset leaves free mode"),M.bFreeCamera);M.SetFreeCamera(true);M.OnDigToggle();T.TestTrue(TEXT("dig lock and free movement are exclusive"),!M.bFreeCamera&&M.bDigEnabled);M.OnDigToggle();
        M.SetSiteVisualization(false);T.TestTrue(TEXT("mode off restores only owned hides"),M.SiteHidden.IsEmpty()&&!M.SiteModel);M.SetSiteVisualization(true);M.SetSplitView(false);M.SetCameraPreset(1);
    }
    static void Gray(UShipHatchMonitorWidget& M){M.bSiteGray=true;M.LastLayout=FVector2D::ZeroVector;M.Layout();}
};
namespace
{
class FHatch07Command final:public IAutomationLatentCommand
{
    FAutomationTestBase* T;int32 Phase=0;double Start=FPlatformTime::Seconds(),Changed=0;
    void Shot(UWorld* W,const TCHAR* Name)
    {
        TArray<FColor> Pixels;FIntVector Size;if(!T->TestTrue(TEXT("actual RHI screenshot"),FSlateApplication::Get().TakeScreenshot(W->GetGameViewport()->GetGameViewportWidget().ToSharedRef(),Pixels,Size)))return;
        const FString Root=FPaths::ProjectSavedDir()/TEXT("Reports/HATCH07-COIL02/progress");IFileManager::Get().MakeDirectory(*Root,true);TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Root/Name));
    }
public:
    explicit FHatch07Command(FAutomationTestBase* Test):T(Test){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>60){T->AddError(TEXT("HATCH07 fixture timeout"));return true;}
        for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE&&FinalAcceptanceViewportReady(C.World()))
        for(TActorIterator<AShipHatchMonitorHost> H(C.World());H;++H)if(H->Monitor&&H->Monitor->GetAnalysis())
        {
            if(Phase==0){FHatch07Probe::Check(*T,*H->Monitor);Changed=FPlatformTime::Seconds();Phase=1;return false;}
            if(FPlatformTime::Seconds()-Changed<1)return false;
            Shot(C.World(),Phase==1?TEXT("hatch-rviz.png"):TEXT("hatch-rviz-gray.png"));
            if(Phase==1){FHatch07Probe::Gray(*H->Monitor);Changed=FPlatformTime::Seconds();Phase=2;return false;}
            H->Monitor->SetSiteVisualization(false);return true;
        }
        return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHatch07Rhi,"MA0T10.Hatch.Hatch07Rhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHatch07Rhi::RunTest(const FString&)
{if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/ShipHatchTestMap"),true))return false;ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FHatch07Command(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
#endif

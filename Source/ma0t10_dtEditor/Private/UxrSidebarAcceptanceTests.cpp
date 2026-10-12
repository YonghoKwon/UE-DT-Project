#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPlacementPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorCaptureExportPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Kismet/GameplayStatics.h"
#include "UxrScenarioUiFixture.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/SlabScenarioValidationRig.h"
#include "EngineUtils.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/UI/SensorOwnedPresentationStyle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSidebarOwnershipTest,"MA0T10.UXR09.SidebarOwnershipAndFloatingCompatibility",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSidebarOwnershipTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
    auto* P=NewObject<UVirtualSensorPlacementPanelWidget>(World);auto* S=NewObject<UVirtualSensorSettingsPanelWidget>(World);auto* D=NewObject<UVirtualSensorCaptureExportPanelWidget>(World);
    W->RegisterOwnedPanel(ESensorToolPanelRole::Placement,P);W->RegisterOwnedPanel(ESensorToolPanelRole::Settings,S);W->RegisterOwnedPanel(ESensorToolPanelRole::Data,D);
    TestTrue(TEXT("native workspace defaults to sidebar"),W->IsSidebarMode());
    W->SetPanelOpen(ESensorToolPanelRole::Placement,true);W->SetPanelOpen(ESensorToolPanelRole::Settings,true);
    TestFalse(TEXT("new task hides previous native body"),W->IsPanelOpen(ESensorToolPanelRole::Placement));TestTrue(TEXT("settings task opens"),W->IsPanelOpen(ESensorToolPanelRole::Settings));
    W->CollapseSidebar();TestFalse(TEXT("rail toggle collapses active body"),W->IsPanelOpen(ESensorToolPanelRole::Settings));
    W->SetSidebarMode(false);W->SetPanelOpen(ESensorToolPanelRole::Placement,true);W->SetPanelOpen(ESensorToolPanelRole::Data,true);
    TestTrue(TEXT("floating mode permits concurrent windows"),W->IsPanelOpen(ESensorToolPanelRole::Placement)&&W->IsPanelOpen(ESensorToolPanelRole::Data));
    TArray<uint8> Before,After;UGameplayStatics::SaveGameToMemory(W->Preferences,Before);
    W->SetSidebarMode(true);P->ApplyWorkspaceDockRect(FVector2D(64,0),FVector2D(308,684));W->SavePanel(ESensorToolPanelRole::Placement);W->SetSidebarMode(false);
    UGameplayStatics::SaveGameToMemory(W->Preferences,After);TestTrue(TEXT("sidebar switching preserves serialized floating preferences byte-for-byte"),Before==After);
    TestFalse(TEXT("floating mode leaves transient dock"),P->IsWorkspaceDocked());
    // Editor test has no arranged Canvas; saved preference remains unchanged and no dock is persisted.
    TestTrue(TEXT("no save schema or slot changes"),USensorToolWorkspaceSaveGame::StaticClass()->FindPropertyByName(TEXT("Version"))!=nullptr);
    W->UnregisterPanel(P);W->UnregisterPanel(S);W->UnregisterPanel(D);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSidebarFontTokensTest,"MA0T10.UXR09.LateOwnedFontTokens",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSidebarFontTokensTest::RunTest(const FString&)
{
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
    auto* P=NewObject<UVirtualSensorPlacementPanelWidget>(World);auto Text=SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",30));
    P->RegisterSensorNativeFont(Text,STextBlock::FArguments());
    TestEqual(TEXT("unowned control retains its font"),Text->GetFont().Size,30.f);
    W->RegisterOwnedPanel(ESensorToolPanelRole::Placement,P);P->ApplySensorToolFontScale(1);Text->SlatePrepass(1);
    TestEqual(TEXT("late native ownership normalizes body token"),Text->GetFont().Size,10.f);
    P->ApplySensorToolFontScale(1.5f);Text->SlatePrepass(1);TestEqual(TEXT("150 percent uses the same original token, no cumulative scaling"),Text->GetFont().Size,15.f);
    TestEqual(TEXT("existing font-scale API still returns stored 150 percent"),P->GetSensorToolFontScale(),1.5f);
    W->UnregisterPanel(P);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSidebarSupplementDesignTest,"MA0T10.UXR09.SupplementDesignContract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSidebarSupplementDesignTest::RunTest(const FString&)
{
    const float Contrast=(FSensorOwnedPresentationStyle::Text().GetLuminance()+.05f)/(FSensorOwnedPresentationStyle::Selection().GetLuminance()+.05f);
    TestTrue(TEXT("selected equipment card text contrast is at least 4.5:1"),Contrast>=4.5f);
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
    auto* P=NewObject<UVirtualSensorPlacementPanelWidget>(World);P->Initialize();W->RegisterOwnedPanel(ESensorToolPanelRole::Placement,P);
    TArray<FString> Texts;int32 InvalidationRoots=0;
    TFunction<void(const TSharedRef<SWidget>&)> Visit=[&](const TSharedRef<SWidget>& Item)
    {
        InvalidationRoots+=Item->GetType()==TEXT("SInvalidationPanel");
        if(Item->GetType()==TEXT("STextBlock"))Texts.Add(StaticCastSharedRef<STextBlock>(Item)->GetText().ToString());
        auto* Children=Item->GetChildren();for(int32 I=0;I<Children->Num();++I)Visit(Children->GetChildAt(I));
    };
    Visit(P->TakeWidget());
    TestEqual(TEXT("unverified paint caches are not installed"),InvalidationRoots,0);
    for(const FString& Label:{FString(TEXT("+ LiDAR")),FString(TEXT("+ Camera")),FString(TEXT("+ 현재 위치 저장")),FString(TEXT("전체 배치 저장")),FString(TEXT("불러오기"))})TestTrue(*FString::Printf(TEXT("default surface exposes %s"),*Label),Texts.Contains(Label));
    for(const auto* Explanation:{TEXT("현재 관찰 위치에 추가 · PCD 전송은 별도"),TEXT("레벨 장비는 삭제할 수 없습니다."),TEXT("선택은 위치를 바꾸지 않습니다 · 이동 버튼으로 적용하세요.")})TestFalse(TEXT("persistent explanations are absent"),Texts.Contains(Explanation));
    W->UnregisterPanel(P);return true;
}

class FUxrSidebarRender : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;double Started=FPlatformTime::Seconds(),ReadyAt=0;int32 Phase=0;bool Requested=false;FString Path;FDateTime Previous;
    TUniquePtr<TGuardValue<bool>> Debug;
public:
    explicit FUxrSidebarRender(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>160){Test->AddError(TEXT("Sidebar render timeout"));return true;}
        UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)World=C.World();
        if(!FinalAcceptanceViewportReady(World))return false;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();auto* Bar=Cast<USensorToolToolbarWidget>(W->GetOwnedNavigation());if(!Bar)return false;
        const int32 View=Phase%8;const float Scale=Phase<8?1:Phase<16?1.25f:1.5f;
        const TCHAR* Names[]={TEXT("placement"),TEXT("preview"),TEXT("scenario"),TEXT("pcd"),TEXT("results"),TEXT("tools"),TEXT("collapsed-warning"),TEXT("floating")};
        if(!ReadyAt)
        {
            if(!Debug)Debug=MakeUnique<TGuardValue<bool>>(GAreScreenMessagesEnabled,false);
            if(Phase==0)W->ResetOwnedWorkspaceLayout(); // Default presentation, not the reviewer's explicitly saved closed card.
            FSlateApplication::Get().DismissAllMenus();W->SetSidebarMode(View!=7);W->SetOwnedPanelFontScale(Scale);
            for(TActorIterator<ASlabScenarioValidationRig> It(World);It;++It)It->SetValidationControlsVisibleForTests(false);
            if(Phase==0)for(TActorIterator<ASlabActor> It(World);It;++It){Test->TestTrue(TEXT("memory-only observation scenario supplies card values"),StartUxrScenarioUiFixture(*It));break;}
            if(Phase==0)if(auto* C=W->GetCoordinator())for(auto* A:C->GetSensorActors())if(A&&A->GetSensorKind()==EVirtualSensorKind::Lidar){C->SelectSensorActor(A);C->SetViewMode(EVirtualSensorViewMode::Lidar);W->SynchronizeOwnedSelection();break;}
            if(View<5)Bar->OpenOwnedDestination(static_cast<ESensorToolDestination>(View));
            if(View==5){W->CollapseSidebar();FSlateApplication::Get().PushMenu(Bar->TakeWidget(),FWidgetPath(),Bar->BuildToolsMenu(),FVector2D(100,100),FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));}
            if(View==6)W->CollapseSidebar();
            if(View==7){W->SetPanelOpen(ESensorToolPanelRole::Placement,true);W->SetPanelOpen(ESensorToolPanelRole::Monitor,true);}
            ReadyAt=FPlatformTime::Seconds()+1;return false;
        }
        if(FPlatformTime::Seconds()<ReadyAt)return false;
        if(!Requested)
        {
            const auto V=World->GetGameViewport()->Viewport->GetSizeXY();const FString RequestedRoot=FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_UXR_REPORT_ROOT"));const FString Root=RequestedRoot.IsEmpty()?FPaths::ProjectSavedDir()/TEXT("Reports/UXR/UXR-09"):RequestedRoot;
            Path=Root/FString::Printf(TEXT("rendered-%dx%d-%d-%s.png"),V.X,V.Y,FMath::RoundToInt(Scale*100),Names[View]);Previous=IFileManager::Get().GetTimeStamp(*Path);
            FSlateApplication::Get().CloseToolTip();FScreenshotRequest::RequestScreenshot(Path,true,false);Requested=true;
            Test->AddInfo(FString::Printf(TEXT("actual client %dx%d font %d view %s; model font=%s slab=%s scenario=%s sidebar=%d; automated render only"),V.X,V.Y,FMath::RoundToInt(Scale*100),Names[View],*Bar->OwnedFontScaleText.ToString(),*Bar->SlabCardTitle.ToString(),*Bar->GlobalStatus.Scenario.ToString(),W->IsSidebarMode()));return false;
        }
        if(IFileManager::Get().GetTimeStamp(*Path)<=Previous)return false;
        Requested=false;ReadyAt=0;return ++Phase==24;
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSidebarRenderTest,"UXR09.PresentationRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSidebarRenderTest::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("Uxr09Capture"))){AddInfo(TEXT("SKIP explicit sidebar capture only"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());ADD_LATENT_AUTOMATION_COMMAND(FUxrSidebarRender(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif

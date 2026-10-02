#pragma once
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "PlayInEditorDataTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "UnrealClient.h"
#include "Slate/SceneViewport.h"

inline FIntPoint FinalAcceptanceViewportSize()
{
    const int32 X=FCString::Atoi(*FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ACCEPTANCE_RES_X")));
    const int32 Y=FCString::Atoi(*FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_ACCEPTANCE_RES_Y")));
    return FIntPoint(X>0?X:1280,Y>0?Y:720);
}
class FStartFinalAcceptancePIE : public IAutomationLatentCommand
{
public:
    bool Update() override
    {
        const auto Size=FinalAcceptanceViewportSize();
        auto Window=SNew(SWindow).Title(FText::FromString(TEXT("Sensor Final Acceptance PIE")))
            .ClientSize(FVector2D(Size.X,Size.Y)).ScreenPosition(FVector2D(0,0)).AutoCenter(EAutoCenter::None)
            .UseOSWindowBorder(false).CreateTitleBar(false).SizingRule(ESizingRule::FixedSize);
        FSlateApplication::Get().AddWindow(Window,true);
        FRequestPlaySessionParams Params;Params.CustomPIEWindow=Window;
        GUnrealEd->RequestPlaySession(Params);
        return true;
    }
};
inline bool FinalAcceptanceViewportReady(UWorld* World)
{
    auto* View=World&&World->GetGameInstance()?World->GetGameInstance()->GetGameViewportClient():nullptr;
    if(!View||!View->Viewport)return false;
    const auto Size=FinalAcceptanceViewportSize();
    if(View->Viewport->GetSizeXY()!=Size){if(auto* Scene=View->GetGameViewport())Scene->ResizeFrame(Size.X,Size.Y,EWindowMode::Windowed);return false;}
    return true;
}
#endif

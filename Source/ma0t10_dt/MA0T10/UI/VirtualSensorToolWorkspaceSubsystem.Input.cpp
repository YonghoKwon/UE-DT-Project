#include "VirtualSensorToolWorkspaceSubsystem.h"
#include "VirtualSensorPanelWidgetBase.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Widgets/SViewport.h"

namespace
{
// A native window activation can give the game viewport permanent capture before
// Slate hit-testing. Release only that captor, only over this workspace's actual
// hit path, then let Slate route the original event normally. No input-mode or
// GameViewportClient policy is replaced; DTCore input blockers remain registered.
class FSensorWorkspacePointerRouter final : public IInputProcessor
{
    TWeakObjectPtr<UVirtualSensorToolWorkspaceSubsystem> Workspace;
public:
    explicit FSensorWorkspacePointerRouter(UVirtualSensorToolWorkspaceSubsystem* W):Workspace(W){}
    void Tick(const float,FSlateApplication&,TSharedRef<ICursor>) override {}
    bool HandleMouseButtonDownEvent(FSlateApplication& App,const FPointerEvent& E) override
    {
        auto* W=Workspace.Get();
        auto* View=W&&W->GetWorld()?W->GetWorld()->GetGameViewport():nullptr;
        const auto ViewWidget=View?View->GetGameViewportWidget():nullptr;
        if(E.IsTouchEvent()||!ViewWidget.IsValid()||!ViewWidget->HasMouseCaptureByUser(E.GetUserIndex(),E.GetPointerIndex()))return false;
        const auto Hit=App.LocateWindowUnderMouse(E.GetScreenSpacePosition(),App.GetInteractiveTopLevelWindows(),false,E.GetUserIndex());
        if(!Hit.IsValid())return false;
        auto Contains=[&Hit](const TSharedPtr<SWidget>& Widget)
        {
            if(Widget.IsValid())for(int32 I=0;I<Hit.Widgets.Num();++I)if(Hit.Widgets[I].Widget==Widget)return true;
            return false;
        };
        bool Owned=Contains(W->GetOwnedNavigation()?W->GetOwnedNavigation()->GetCachedWidget():nullptr);
        for(int32 I=0;!Owned&&I<7;++I)
        {
            const auto Role=static_cast<ESensorToolPanelRole>(I);const auto* Panel=W->GetOwnedPanel(Role);
            Owned=Panel&&Panel->SupportsOwnedDocking()&&W->IsPanelOpen(Role)&&Contains(Panel->GetCachedWidget());
        }
        if(Owned)if(auto User=App.GetUser(E.GetUserIndex()))
        {
            const auto CaptorPath=User->GetCaptorPath(E.GetPointerIndex());
            // Process a release reply as well as clearing Slate capture: it turns
            // off native high-precision input and the viewport's cursor lock.
            App.ProcessReply(CaptorPath,FReply::Handled().ReleaseMouseCapture().ReleaseMouseLock(),&Hit,&E,E.GetUserIndex());
        }
        return false;
    }
};
}

void UVirtualSensorToolWorkspaceSubsystem::RegisterOwnedPointerRouting()
{
    if(!OwnedPointerRouter&&FSlateApplication::IsInitialized())
    {
        OwnedPointerRouter=MakeShared<FSensorWorkspacePointerRouter>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(OwnedPointerRouter,0);
    }
}
void UVirtualSensorToolWorkspaceSubsystem::UnregisterOwnedPointerRouting()
{
    if(OwnedPointerRouter&&FSlateApplication::IsInitialized())FSlateApplication::Get().UnregisterInputPreProcessor(OwnedPointerRouter);
    OwnedPointerRouter.Reset();
}

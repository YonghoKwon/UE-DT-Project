#include "VirtualSensorToolRig.h"
#include "VirtualSensorUiHostActor.h"
#include "SlabSimulationUiHostActor.h"
#include "SlabScenarioReplayUiHostActor.h"
#include "VirtualSensorToolWorkspaceSubsystem.h"
#include "VirtualSensorMonitorPanelWidget.h"
#include "VirtualSensorSettingsPanelWidget.h"
#include "VirtualSensorCaptureExportPanelWidget.h"
#include "VirtualSensorPlacementPanelWidget.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace
{
template<class T> bool ResolveUnique(UWorld* World,TObjectPtr<T>& Target,FString& Error)
{
    int32 Count=0;T* Only=nullptr;
    for(TActorIterator<T> It(World);It;++It)if(IsValid(*It)&&!It->IsActorBeingDestroyed()){Only=*It;++Count;}
    if(Count>1){Error=TEXT("도구 구성이 중복되었습니다 · 같은 종류의 구성 Actor를 하나만 두세요.");return false;}
    if(Target&&(!IsValid(Target)||Target->GetWorld()!=World||Target->IsActorBeingDestroyed()))Target=nullptr;
    if(!Target)Target=Only;
    return true;
}
template<class T> T* SpawnOwned(UWorld* World,AActor* Owner,TArray<TWeakObjectPtr<AActor>>& Owned)
{
    FActorSpawnParameters Params;Params.Owner=Owner;Params.ObjectFlags|=RF_Transient;
    T* Actor=World->SpawnActor<T>(T::StaticClass(),FTransform::Identity,Params);
    if(Actor)Owned.Add(Actor);
    return Actor;
}
}
AVirtualSensorToolRig::AVirtualSensorToolRig(){PrimaryActorTick.bCanEverTick=false;}
void AVirtualSensorToolRig::BeginPlay()
{
    Super::BeginPlay();
    // Existing level owners finish BeginPlay first. Deferred hosts below get their bindings before BeginPlay.
    GetWorldTimerManager().SetTimerForNextTick(this,&AVirtualSensorToolRig::RetrySetup);
}
void AVirtualSensorToolRig::RetrySetup()
{
    if(bEnding)return;
    EnsureSetup();
    // Late Slab placement and destroyed hosts are detected at 1 Hz, never on a measurement callback.
    GetWorldTimerManager().SetTimer(SetupTimer,this,&AVirtualSensorToolRig::RetrySetup,1.f,false);
}
bool AVirtualSensorToolRig::EnsureSetup()
{
    auto* World=GetWorld();if(bEnding||!World||!World->IsGameWorld())return false;
    int32 Rigs=0;for(TActorIterator<AVirtualSensorToolRig> It(World);It;++It)if(!It->IsActorBeingDestroyed())++Rigs;
    if(Rigs!=1){bReady=false;SetupMessage=TEXT("센서 도구 설치가 중복되었습니다 · 맵에 Rig를 하나만 두세요.");return false;}
    if(!ResolveUnique(World,Coordinator,SetupMessage)||!ResolveUnique(World,SensorUiHost,SetupMessage)||
       !ResolveUnique(World,SlabUiHost,SetupMessage)||!ResolveUnique(World,ReplayUiHost,SetupMessage)){bReady=false;return false;}
    if(!Coordinator)Coordinator=SpawnOwned<AVirtualSensorCoordinator>(World,this,OwnedActors);
    if(!Coordinator){SetupMessage=TEXT("센서 구성 생성 실패 · 맵 점검을 확인하세요.");return false;}
    if(!SensorUiHost)
    {
        SensorUiHost=World->SpawnActorDeferred<AVirtualSensorUiHostActor>(AVirtualSensorUiHostActor::StaticClass(),FTransform::Identity,this);
        if(SensorUiHost){SensorUiHost->SetFlags(RF_Transient);OwnedActors.Add(SensorUiHost);SensorUiHost->SensorManager=Coordinator;SensorUiHost->FinishSpawning(FTransform::Identity);}
    }
    else if(!OwnedActors.Contains(SensorUiHost)&&!bReusedSensor){PreviousManager=SensorUiHost->SensorManager;bReusedSensor=true;}
    if(!SensorUiHost){SetupMessage=TEXT("화면 생성 실패 · 센서 UI 구성을 확인하세요.");return false;}
    if(!bReady||SensorUiHost->SensorManager!=Coordinator)
    {
        SensorUiHost->SensorManager=Coordinator;
        auto* W=World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();W->SetCoordinator(Coordinator);
        SensorUiHost->CreateAndBindMonitorWidget();SensorUiHost->CreateAndBindToolWidgets();
        if(auto* P=SensorUiHost->GetMonitorWidget())P->BindSensorManager(Coordinator);
        if(auto* P=SensorUiHost->GetSettingsWidget())P->BindSensorManager(Coordinator);
        if(auto* P=SensorUiHost->GetCaptureExportWidget())P->BindSensorManager(Coordinator);
    }
    if(TargetSlab&&(!IsValid(TargetSlab)||TargetSlab->GetWorld()!=World||TargetSlab->IsActorBeingDestroyed()))TargetSlab=nullptr;
    // Preserve UXR-08's explicit 3D panel selection. A changed Rig property still wins.
    if(SlabUiHost&&TargetSlab==LastBoundSlab.Get()&&IsValid(SlabUiHost->SlabActor)&&SlabUiHost->SlabActor!=LastBoundSlab.Get())TargetSlab=SlabUiHost->SlabActor;
    if(!TargetSlab&&SlabUiHost&&IsValid(SlabUiHost->SlabActor)&&!SlabUiHost->SlabActor->IsActorBeingDestroyed())TargetSlab=SlabUiHost->SlabActor;
    if(!TargetSlab){int32 Count=0;ASlabActor* Only=nullptr;for(TActorIterator<ASlabActor> It(World);It;++It)if(!It->IsActorBeingDestroyed()){Only=*It;++Count;}if(Count==1)TargetSlab=Only;}
    if(TargetSlab)
    {
        if(!SlabUiHost)
        {
            SlabUiHost=World->SpawnActorDeferred<ASlabSimulationUiHostActor>(ASlabSimulationUiHostActor::StaticClass(),FTransform::Identity,this);
            if(SlabUiHost){SlabUiHost->SetFlags(RF_Transient);OwnedActors.Add(SlabUiHost);SlabUiHost->SlabActor=TargetSlab;SlabUiHost->FinishSpawning(FTransform::Identity);}
        }
        else if(!OwnedActors.Contains(SlabUiHost)&&!bReusedSlab){PreviousSlab=SlabUiHost->SlabActor;bReusedSlab=true;}
        if(SlabUiHost&&SlabUiHost->SlabActor!=TargetSlab)SlabUiHost->BindSlabActor(TargetSlab);
        if(SlabUiHost&&!SlabUiHost->ProgressWidget)SlabUiHost->ShowSimulationPanels();
        if(!ReplayUiHost)ReplayUiHost=SpawnOwned<ASlabScenarioReplayUiHostActor>(World,this,OwnedActors);
        if(ReplayUiHost)ReplayUiHost->ShowReplayPanel();
    }
    if(auto* Data=SensorUiHost->GetCaptureExportWidget())
    {
        if(BoundDataWidget.Get()!=Data){BoundDataWidget=Data;PreviousDataSlab=Data->GetExplicitScenarioSlabActor();}
        if(Data->GetExplicitScenarioSlabActor()!=TargetSlab)Data->BindScenarioSlabActor(TargetSlab);
    }
    LastBoundSlab=TargetSlab;
    bReady=SensorUiHost->GetPlacementWidget()!=nullptr;
    SetupMessage=bReady?TEXT("센서 도구 준비 완료"):TEXT("화면 준비 대기 · PlayerController와 센서 UI를 확인하세요.");
    return bReady;
}
FString AVirtualSensorToolRig::GetSlabSetupMessage() const
{
    if(IsValid(TargetSlab))return TEXT("Slab 연결 완료");
    int32 Count=0;for(TActorIterator<ASlabActor> It(GetWorld());It;++It)if(!It->IsActorBeingDestroyed())++Count;
    return Count==0?TEXT("Slab이 없습니다 · SlabActor와 TrackReference를 배치하세요."):TEXT("Slab이 여러 개입니다 · Rig의 TargetSlab에서 대상을 선택하세요.");
}
int32 AVirtualSensorToolRig::GetOwnedActorCount() const
{int32 Count=0;for(const auto& A:OwnedActors)Count+=A.IsValid()&&!A->IsActorBeingDestroyed();return Count;}
void AVirtualSensorToolRig::EndPlay(const EEndPlayReason::Type Reason)
{
    bEnding=true;GetWorldTimerManager().ClearTimer(SetupTimer);
    if(bReusedSensor&&IsValid(SensorUiHost))
    {
        SensorUiHost->SensorManager=PreviousManager.Get();
        if(auto* P=SensorUiHost->GetMonitorWidget())P->BindSensorManager(PreviousManager.Get());
        if(auto* P=SensorUiHost->GetSettingsWidget())P->BindSensorManager(PreviousManager.Get());
        if(auto* P=SensorUiHost->GetCaptureExportWidget())P->BindSensorManager(PreviousManager.Get());
        if(auto* W=GetWorld()->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>())W->SetCoordinator(PreviousManager.Get());
    }
    if(bReusedSlab&&IsValid(SlabUiHost))SlabUiHost->BindSlabActor(PreviousSlab.Get());
    if(bReusedSensor&&BoundDataWidget.IsValid())BoundDataWidget->BindScenarioSlabActor(PreviousDataSlab.Get());
    for(int32 I=OwnedActors.Num()-1;I>=0;--I)if(auto* A=OwnedActors[I].Get())if(!A->IsActorBeingDestroyed())A->Destroy();
    OwnedActors.Reset();bReady=false;Super::EndPlay(Reason);
}

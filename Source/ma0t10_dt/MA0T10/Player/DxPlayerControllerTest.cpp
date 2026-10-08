// Fill out your copyright notice in the Description page of Project Settings.


#include "DxPlayerControllerTest.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EnhancedInputComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorTransformGizmoActor.h"
#include "ma0t10_dt/MA0T10/Player/DxPlayerTest.h"

void ADxPlayerControllerTest::SetupInputComponent()
{
    Super::SetupInputComponent();
    if(auto* Input=Cast<UEnhancedInputComponent>(InputComponent))
    {
        TArray<uint32> Handles;
        for(const auto& Binding:Input->GetActionEventBindings())
            if(Binding->IsBoundToObject(this)&&(Binding->GetAction()==MovementAction||Binding->GetAction()==LookAction||Binding->GetAction()==UpDownAction||Binding->GetAction()==RightMouseButtonAction))Handles.Add(Binding->GetHandle());
        for(uint32 Handle:Handles)Input->RemoveBindingByHandle(Handle);
        for(auto Event:{ETriggerEvent::Triggered,ETriggerEvent::Completed,ETriggerEvent::Canceled})
        {
            if(MovementAction)Input->BindAction(MovementAction,Event,this,&ThisClass::PlacementMove);
            if(UpDownAction)Input->BindAction(UpDownAction,Event,this,&ThisClass::PlacementMoveUpDown);
        }
        if(LookAction)Input->BindAction(LookAction,ETriggerEvent::Triggered,this,&ThisClass::PlacementLook);
        for(auto Event:{ETriggerEvent::Triggered,ETriggerEvent::Completed,ETriggerEvent::Canceled})
            if(RightMouseButtonAction)Input->BindAction(RightMouseButtonAction,Event,this,&ThisClass::PlacementRightMouse);
    }
}
AVirtualSensorTransformGizmoActor* ADxPlayerControllerTest::GetPlacementInputOwner() const
{auto* G=PlacementOwner.Get();return G&&G->IsResponsivePlacement()&&G->IsManipulationEnabled()?G:nullptr;}
void ADxPlayerControllerTest::RegisterPlacementInputOwner(AVirtualSensorTransformGizmoActor* G){PlacementOwner=G;}
void ADxPlayerControllerTest::UnregisterPlacementInputOwner(AVirtualSensorTransformGizmoActor* G)
{if(PlacementOwner.Get()==G){FinishOwnedNavigationInput();PlacementOwner.Reset();}}
void ADxPlayerControllerTest::PlacementMove(const FInputActionValue& Value)
{if(auto* G=GetPlacementInputOwner()){if(G->IsObserverNavigationActive()&&G->IsInputFocusOwned())if(auto* P=Cast<ADxPlayerTest>(GetPawn()))P->Move(Value.Get<FVector2D>());return;}Super::Move(Value);}
void ADxPlayerControllerTest::PlacementMoveUpDown(const FInputActionValue& Value)
{if(auto* G=GetPlacementInputOwner()){if(G->IsObserverNavigationActive()&&G->IsInputFocusOwned())if(auto* P=Cast<ADxPlayerTest>(GetPawn()))P->MoveUpDown(Value.Get<float>());return;}Super::MoveUpDown(Value);}
void ADxPlayerControllerTest::PlacementLook(const FInputActionValue& Value)
{if(auto* G=GetPlacementInputOwner()){if(G->IsObserverNavigationActive()&&G->IsInputFocusOwned())if(auto* P=Cast<ADxPlayerTest>(GetPawn()))P->Look(Value.Get<FVector2D>());return;}Super::Look(Value);}
void ADxPlayerControllerTest::PlacementRightMouse(const FInputActionValue& Value)
{
    if(auto* G=GetPlacementInputOwner())
    {
        if(Value.Get<bool>()&&G->IsObserverNavigationActive())
        {if(!bOwnedRightInput){SavedCursor=bShowMouseCursor;SavedClickEvents=bEnableClickEvents;SavedMouseOverEvents=bEnableMouseOverEvents;bOwnedRightInput=true;}Super::ClickRightMouseButton(Value);}
        else if(!Value.Get<bool>())FinishOwnedNavigationInput();
        return;
    }
    if(bOwnedRightInput)FinishOwnedNavigationInput();else Super::ClickRightMouseButton(Value);
}
void ADxPlayerControllerTest::FinishOwnedNavigationInput()
{if(bOwnedRightInput){Super::ClickRightMouseButton(FInputActionValue(false));bShowMouseCursor=SavedCursor;bEnableClickEvents=SavedClickEvents;bEnableMouseOverEvents=SavedMouseOverEvents;bOwnedRightInput=false;}}

ADxPlayerControllerTest::ADxPlayerControllerTest()
{
	ClickActivationPolicy=EDxClickActivationPolicy::SingleRelease;
}

void ADxPlayerControllerTest::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		if (World->HasBegunPlay()) ApplyProjectViewportPolicy();
		else WorldBeginPlayHandle = World->OnWorldBeginPlay.AddUObject(this, &ADxPlayerControllerTest::ApplyProjectViewportPolicy);
	}
}

void ADxPlayerControllerTest::ApplyProjectViewportPolicy()
{
	if (UWorld* World = GetWorld(); GEngine && World && World->GetGameViewport())
	{
		GEngine->SetEngineStat(World, World->GetGameViewport(), TEXT("FPS"), false);
	}
}

void ADxPlayerControllerTest::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if(auto* G=PlacementOwner.Get())G->EndObserverNavigation();FinishOwnedNavigationInput();PlacementOwner.Reset();
	if (UWorld* World = GetWorld()) World->OnWorldBeginPlay.Remove(WorldBeginPlayHandle);
	Super::EndPlay(EndPlayReason);
}

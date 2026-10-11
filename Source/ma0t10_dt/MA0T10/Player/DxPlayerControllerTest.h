// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Player/DxPlayerControllerBase.h"
#include "DxPlayerControllerTest.generated.h"
class AVirtualSensorTransformGizmoActor;
class IVirtualSensorPlacementInputOwner;

/**
 * 
 */
UCLASS()
class MA0T10_DT_API ADxPlayerControllerTest : public ADxPlayerControllerBase
{
	GENERATED_BODY()
public:
	ADxPlayerControllerTest();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void RegisterPlacementInputOwner(AVirtualSensorTransformGizmoActor* Gizmo);
	void UnregisterPlacementInputOwner(AVirtualSensorTransformGizmoActor* Gizmo);
	AVirtualSensorTransformGizmoActor* GetPlacementInputOwner() const;
	IVirtualSensorPlacementInputOwner* GetPlacementInputView() const;
	void FinishOwnedNavigationInput();
protected:
	virtual void SetupInputComponent() override;
private:
	IVirtualSensorPlacementInputOwner* GetRegisteredPlacementInputView() const;
	void PlacementMove(const FInputActionValue& Value);
	void PlacementLook(const FInputActionValue& Value);
	void PlacementMoveUpDown(const FInputActionValue& Value);
	void PlacementRightMouse(const FInputActionValue& Value);
	TWeakObjectPtr<AVirtualSensorTransformGizmoActor> PlacementOwner;
	bool bOwnedRightInput=false;
	bool SavedCursor=true,SavedClickEvents=true,SavedMouseOverEvents=true;
	void ApplyProjectViewportPolicy();
	FDelegateHandle WorldBeginPlayHandle;
};

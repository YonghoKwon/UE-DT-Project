// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InteractableActor/FacilityBase.h"
#include "Crane.generated.h"

class UPoseableMeshComponent;
class UStaticMeshComponent;
class UCraneStatusVisualizerComp;
class UCraneMechDriverComp;
class UCraneDataSyncComp;

UCLASS()
class M7AT10_DT_API ACrane : public AFacilityBase
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ACrane();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Function
public:
private:
protected:

	// Variable
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crane")
	FString CraneId;
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPoseableMeshComponent> MainMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCraneDataSyncComp> DataSyncComp;
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCraneMechDriverComp> MechDriverComp;
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCraneStatusVisualizerComp> StatusVisualizerComp;

	// UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	// TObjectPtr<UPoseableMeshComponent> CraneMesh;
protected:
};

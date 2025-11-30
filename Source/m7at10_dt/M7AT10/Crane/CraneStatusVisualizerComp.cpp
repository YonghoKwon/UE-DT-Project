// Fill out your copyright notice in the Description page of Project Settings.


#include "CraneDataSyncComp.h"
#include "CraneStatusVisualizerComp.h"

#include "Components/StaticMeshComponent.h"


// Sets default values for this component's properties
UCraneStatusVisualizerComp::UCraneStatusVisualizerComp()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UCraneStatusVisualizerComp::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		if (UCraneDataSyncComp* DataSyncComp = Owner->FindComponentByClass<UCraneDataSyncComp>())
		{
			DataSyncComp->OnCraneStatusColorChanged.AddDynamic(this, &UCraneStatusVisualizerComp::OnColorChanged);
		}
	}
}


// Called every frame
void UCraneStatusVisualizerComp::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UCraneStatusVisualizerComp::OnColorChanged(const FLinearColor& NewColor)
{

}

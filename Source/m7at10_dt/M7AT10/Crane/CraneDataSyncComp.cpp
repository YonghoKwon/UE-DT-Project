// Fill out your copyright notice in the Description page of Project Settings.


#include "CraneDataSyncComp.h"

#include "m7at10_dt/m7at10_dt.h"
#include "Core/DxDataType.h"


// Sets default values for this component's properties
UCraneDataSyncComp::UCraneDataSyncComp()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

void UCraneDataSyncComp::OnReceiveCraneStateData(const FCraneStateData& InData)
{
	UE_LOG(LogM7AT10, Log, TEXT("[CraneDataSyncComp] Received data for Crane: %s"), *InData.CraneId);

	OnCranePositionChanged.Broadcast(InData.Position);
	// OnCraneStatusColorChanged.Broadcast(FLinearColor::Red);
}

void UCraneDataSyncComp::OnReceiveData(EDxDataType DataType, const void* Data)
{
	if (!Data)
	{
		UE_LOG(LogM7AT10, Warning, TEXT("[CraneDataSyncComp] OnReceiveData: Data is null"));
		return;
	}

	if (DataType == EDxDataType::CraneState)
	{
		const FCraneStateData* CraneData = static_cast<const FCraneStateData*>(Data);
		OnReceiveCraneStateData(*CraneData);
	}
	else
	{
		UE_LOG(LogM7AT10, Warning, TEXT("[CraneDataSyncComp] Unknown DataType: %s"), *UEnum::GetValueAsString(DataType));
	}
}

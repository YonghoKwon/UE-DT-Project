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

void UCraneDataSyncComp::OnReceiveData(const TSharedPtr<FDxDataBase>& DataPtr)
{
	// 1. 데이터 포인터 유효성 검사
	if (!DataPtr.IsValid()) return;

	// 2. 타입 확인 (데이터 자체가 자신의 타입을 알고 있음)
	if (DataPtr->GetType() == EDxDataType::CraneState)
	{
		// 3. 안전한 캐스팅 (StaticCastSharedPtr)
		// FDxDataBase -> FCraneStateData로 포인터 변환
		// TSharedPtr를 사용하므로 메모리가 도중에 삭제될 위험이 없음
		TSharedPtr<FCraneStateData> CraneData = StaticCastSharedPtr<FCraneStateData>(DataPtr);

		if (CraneData.IsValid())
		{
			// 실제 데이터 처리
			OnReceiveCraneStateData(*CraneData);
		}
	}
}

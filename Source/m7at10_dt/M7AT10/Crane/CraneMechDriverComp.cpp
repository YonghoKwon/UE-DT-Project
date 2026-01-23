// Fill out your copyright notice in the Description page of Project Settings.

#include "CraneMechDriverComp.h"

#include "Components/PoseableMeshComponent.h"
#include "m7at10_dt/M7AT10/Crane/CraneDataSyncComp.h"
#include "m7at10_dt/m7at10_dt.h"


// Sets default values for this component's properties
UCraneMechDriverComp::UCraneMechDriverComp()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// ...
}


// Called when the game starts
void UCraneMechDriverComp::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		if (UCraneDataSyncComp* DataSyncComp = Owner->FindComponentByClass<UCraneDataSyncComp>())
		{
			DataSyncComp->OnCranePositionChanged.AddDynamic(this, &UCraneMechDriverComp::SetTargetPosition);
		}

		TargetMesh = Owner->FindComponentByClass<UPoseableMeshComponent>();

		if (TargetMesh)
		{
			auto CacheTransform = [&](FName Name) {
				if (Name != NAME_None) {
					// 위치 저장
					FVector Pos = TargetMesh->GetBoneLocationByName(Name, EBoneSpaces::ComponentSpace);
					InitialComponentLocations.Add(Name, Pos);

					// 회전 저장
					FQuat Rot = TargetMesh->GetBoneQuaternion(Name, EBoneSpaces::ComponentSpace);
					InitialComponentRotations.Add(Name, Rot);

					UE_LOG(LogTemp, Warning, TEXT("Saved Init %s"), *Name.ToString());
				}
			};

			// 줄(Rope) 저장
			CacheTransform(AnchorBoneName);
			CacheTransform(HookRopeBoneName);
			CacheTransform(ControlRopeBoneName);

			// [추가] 끝부분(Hook/Controller) 저장
			CacheTransform(HookEntityBoneName);
			CacheTransform(ControlEntityBoneName);

			// =========================================================
			// [추가] 뼈 이름이 맞는지 확인하기 위해, 존재하는 모든 본 이름을 출력해봅니다.
			// =========================================================
			UE_LOG(LogTemp, Warning, TEXT("=== [Crane Bone Name Check] Start ==="));
			int32 NumBones = TargetMesh->GetNumBones();
			for (int32 i = 0; i < NumBones; ++i)
			{
				FName BoneName = TargetMesh->GetBoneName(i);
				UE_LOG(LogTemp, Warning, TEXT("Bone Index: %d, Name: %s"), i, *BoneName.ToString());
			}
			UE_LOG(LogTemp, Warning, TEXT("=== [Crane Bone Name Check] End ==="));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[CraneMechDriver] FAIL: PoseableMeshComponent NOT found!"));
		}
	}
}


// Called every frame
void UCraneMechDriverComp::TickComponent(float DeltaTime, ELevelTick TickType,
                                         FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// [추가] 자동 테스트 모드 로직
	if (bAutoTestMode)
	{
		// 1초마다 로그 출력 (너무 빠르지 않게)
		static float LogTimer = 0.0f;
		LogTimer += DeltaTime;
		if (LogTimer > 1.0f)
		{
			LogTimer = 0.0f;
			UE_LOG(LogTemp, Warning, TEXT("[CraneMechDriver] AutoTest Running. Target Anchor: %f, Rope: %f"), TargetPosition.AnchorPosition, TargetPosition.HookRopeHeight);
		}

		// (기존 자동 이동 로직...)
		RunningTime += DeltaTime * TestSpeed;
		TargetPosition.AnchorPosition = FMath::Sin(RunningTime) * TestAnchorRange;
		float NormalizedSin = (FMath::Sin(RunningTime) + 1.0f) * 0.5f;
		TargetPosition.HookRopeHeight = NormalizedSin * TestRopeRange;
		TargetPosition.ControlRopeHeight = NormalizedSin * TestRopeRange;
	}

	// 트롤리 X축 이동 처리
	UpdateBoneMovement(DeltaTime);
}

void UCraneMechDriverComp::SetTargetPosition(const FCranePositionData& InPositionData)
{
	TargetPosition = InPositionData;

	// 기존 목표 위치에 새로운 값을 누적
	// TargetPosition.TrolleyPosition += InPositionData.TrolleyPosition * MovementScale;
	// TargetPosition.HoistHeight += InPositionData.HoistHeight * MovementScale;
	// TargetPosition.GantryPosition += InPositionData.GantryPosition * MovementScale;

	// UE_LOG(LogM7AT10, Log, TEXT("[CraneMechDriverComp] Target - Trolley: %.2f, Hoist: %.2f, Gantry: %.2f"), TargetPosition.TrolleyPosition, TargetPosition.HoistHeight, TargetPosition.GantryPosition);
}

void UCraneMechDriverComp::UpdateTrolleyMovement(float DeltaTime)
{
	// AActor* Owner = GetOwner();
	// if (!Owner)
	// {
	// 	return;
	// }
	//
	// // 현재 위치와 목표 위치의 차이 계산
	// const float TargetX = TargetPosition.TrolleyPosition;
	// const float CurrentX = CurrentPosition.TrolleyPosition;
	//
	// // 목표에 거의 도달했으면 스킵
	// if (FMath::IsNearlyEqual(CurrentX, TargetX, 0.1f))
	// {
	// 	return;
	// }
	//
	// // 부드러운 보간으로 현재 위치 업데이트
	// const float NewX = FMath::FInterpTo(CurrentX, TargetX, DeltaTime, InterpSpeed);
	// CurrentPosition.TrolleyPosition = NewX;
	//
	// // 이동량 계산
	// const float DeltaX = NewX - CurrentX;
	//
	// // 액터 위치 업데이트 (X축으로만 이동)
	// FVector CurrentLocation = Owner->GetActorLocation();
	// CurrentLocation.X += DeltaX;
	// Owner->SetActorLocation(CurrentLocation);
}

void UCraneMechDriverComp::UpdateBoneMovement(float DeltaTime)
{
	if (!TargetMesh) return;

	// 1. 값 보간
	CurrentPosition.AnchorPosition = FMath::FInterpTo(CurrentPosition.AnchorPosition, TargetPosition.AnchorPosition, DeltaTime, InterpSpeed);
	CurrentPosition.HookRopeHeight = FMath::FInterpTo(CurrentPosition.HookRopeHeight, TargetPosition.HookRopeHeight, DeltaTime, InterpSpeed);
	CurrentPosition.ControlRopeHeight = FMath::FInterpTo(CurrentPosition.ControlRopeHeight, TargetPosition.ControlRopeHeight, DeltaTime, InterpSpeed);

	if (bEnableSway) SwayTime += DeltaTime * SwaySpeed;

	// -------------------------------------------------------------------------
	// [함수 1] 줄(Rope)은 오직 '위치(길이)'만 변경합니다. (회전 X)
	// -------------------------------------------------------------------------
	auto ApplyRopeMovement = [&](FName BoneName, FVector MoveAxis, float Value)
	{
		if (BoneName == NAME_None || !InitialComponentLocations.Contains(BoneName)) return;

		// 초기 위치 + 길이 연장
		FVector StartPos = InitialComponentLocations[BoneName];
		FVector FinalPos = StartPos + (MoveAxis * Value);

		// 위치만 적용! (회전은 건드리지 않음 -> 수직 유지)
		TargetMesh->SetBoneLocationByName(BoneName, FinalPos, EBoneSpaces::ComponentSpace);
	};

	// -------------------------------------------------------------------------
	// [함수 2] 갈고리(Hook)는 오직 '회전(흔들림)'만 적용합니다. (위치는 부모 따라감)
	// -------------------------------------------------------------------------
	auto ApplySwayRotation = [&](FName BoneName)
	{
		if (BoneName == NAME_None || !InitialComponentRotations.Contains(BoneName)) return;

		FQuat FinalRot;

		if (bEnableSway)
		{
			// 흔들림 계산 (좌우 5도)
			float Wave = FMath::Sin(SwayTime);
			float RotAngle = Wave * 30.0f;

			// Y축 흔들림 -> Roll 회전
			FRotator SwayRotator = FRotator::ZeroRotator;
			SwayRotator.Roll = RotAngle;
			FQuat SwayQuat = FQuat(SwayRotator);

			// 초기 회전값 + 흔들림
			FQuat InitRot = InitialComponentRotations[BoneName];
			FinalRot = InitRot * SwayQuat;
		}
		else
		{
			// 흔들림 꺼지면 원래 회전값 유지
			FinalRot = InitialComponentRotations[BoneName];
		}

		// 회전 적용
		TargetMesh->SetBoneRotationByName(BoneName, FinalRot.Rotator(), EBoneSpaces::ComponentSpace);
	};

	// ========================
	// 실제 실행 단계
	// ========================

	// 1. 앵커 이동 (좌우)
	if (InitialComponentLocations.Contains(AnchorBoneName))
	{
		FVector AnchorStart = InitialComponentLocations[AnchorBoneName];
		FVector AnchorPos = AnchorStart + (AnchorMoveAxis * CurrentPosition.AnchorPosition);
		TargetMesh->SetBoneLocationByName(AnchorBoneName, AnchorPos, EBoneSpaces::ComponentSpace);
	}

	// 2. 줄(Rope) 길이 조절 (흔들림 없음)
	ApplyRopeMovement(HookRopeBoneName, RopeMoveAxis, CurrentPosition.HookRopeHeight);
	ApplyRopeMovement(ControlRopeBoneName, RopeMoveAxis, CurrentPosition.ControlRopeHeight);

	// 3. 갈고리(Hook) 흔들림 적용 (줄 끝에서 딸랑딸랑)
	ApplySwayRotation(HookEntityBoneName);
	ApplySwayRotation(ControlEntityBoneName);
}

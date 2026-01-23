// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CraneDataTypes.h"
#include "ActorComponent/MechDriverCompBase.h"
#include "CraneMechDriverComp.generated.h"


class UPoseableMeshComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class M7AT10_DT_API UCraneMechDriverComp : public UMechDriverCompBase
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UCraneMechDriverComp();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	// Function
public:
	UFUNCTION()
	void SetTargetPosition(const FCranePositionData& InPositionData);
private:
	UFUNCTION()
	void UpdateTrolleyMovement(float DeltaTime);

	// 내부적으로 본을 움직이는 함수
	void UpdateBoneMovement(float DeltaTime);
protected:

	// Variable
public:
	// [추가] 자동 테스트 모드 켜기/끄기
	UPROPERTY(EditAnywhere, Category = "Test|AutoMove")
	bool bAutoTestMode = true;

	// [추가] 움직임 속도
	UPROPERTY(EditAnywhere, Category = "Test|AutoMove", meta = (EditCondition = "bAutoTestMode"))
	float TestSpeed = 20.0f;

	// [추가] Anchor 좌우 이동 범위 (Unreal Unit)
	UPROPERTY(EditAnywhere, Category = "Test|AutoMove", meta = (EditCondition = "bAutoTestMode"))
	float TestAnchorRange = 10000.0f;

	// [추가] Rope 상하 이동 범위 (Unreal Unit)
	UPROPERTY(EditAnywhere, Category = "Test|AutoMove", meta = (EditCondition = "bAutoTestMode"))
	float TestRopeRange = 50.0f;
private:
	// 목표 위치 (보간 등에 사용)
	UPROPERTY()
	FCranePositionData TargetPosition;
	// 현재 위치 (보간 등에 사용)
	UPROPERTY()
	FCranePositionData CurrentPosition;

	// 보간 속도 (값이 클수록 빠르게 목표에 도달)
	UPROPERTY(EditAnywhere, Category = "Movement")
	float InterpSpeed = 5.0f;

	// 이동 스케일 (입력값에 곱해짐)
	UPROPERTY(EditAnywhere, Category = "Movement")
	float MovementScale = 100.0f;

	// 사인 파형 계산을 위한 누적 시간
	float RunningTime = 0.0f;

	// [추가] 본의 초기 상대 위치(Relative Location)를 저장할 맵
	TMap<FName, FVector> InitialComponentLocations;

	TMap<FName, FQuat> InitialComponentRotations;

	float SwayTime = 0.0f; // 시간 누적용 변수
protected:
	// [추가] 제어할 메쉬 레퍼런스
	UPROPERTY()
	TObjectPtr<UPoseableMeshComponent> TargetMesh;

	// [추가] 본 이름 설정 (블루프린트에서 이름이 다를 경우 수정 가능하도록 EditAnywhere 지정)
	UPROPERTY(EditAnywhere, Category = "Settings|Bones")
	FName AnchorBoneName = TEXT("Anchor"); // 좌우 이동 본

	UPROPERTY(EditAnywhere, Category = "Settings|Bones")
	FName HookRopeBoneName = TEXT("HookRope"); // 1번 로프 본

	UPROPERTY(EditAnywhere, Category = "Settings|Bones")
	FName ControlRopeBoneName = TEXT("ControllerRope"); // 2번 로프 본


	// [추가] 흔들릴 '끝부분(갈고리)' 본 이름
	UPROPERTY(EditAnywhere, Category = "Settings|Bones")
	FName HookEntityBoneName = TEXT("Hook"); // 실제 갈고리

	UPROPERTY(EditAnywhere, Category = "Settings|Bones")
	FName ControlEntityBoneName = TEXT("Cpntroller"); // 실제 컨트롤러 (로그상의 오타 유지)


	// [추가] 이동 축 설정 (메쉬 제작 방식에 따라 축이 다를 수 있음)
	UPROPERTY(EditAnywhere, Category = "Settings|Axis")
	FVector AnchorMoveAxis = FVector(0, 1, 0); // 예: Y축으로 좌우 이동

	UPROPERTY(EditAnywhere, Category = "Settings|Axis")
	FVector RopeMoveAxis = FVector(0, 0, -1); // 예: Z축(음수)으로 아래로 내려감

	// [추가] 로프 흔들림 효과 설정
	UPROPERTY(EditAnywhere, Category = "Movement|Sway")
	bool bEnableSway = true; // 흔들림 켜기/끄기

	UPROPERTY(EditAnywhere, Category = "Movement|Sway")
	float SwaySpeed = 2.0f;  // 흔들리는 속도

	UPROPERTY(EditAnywhere, Category = "Movement|Sway")
	float SwayRadius = 30.0f; // 흔들리는 범위(반경)
};

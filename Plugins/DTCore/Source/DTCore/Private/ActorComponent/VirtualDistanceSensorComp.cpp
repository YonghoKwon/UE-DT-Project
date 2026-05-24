#include "ActorComponent/VirtualDistanceSensorComp.h"
#include "DrawDebugHelpers.h" // 디버그 라인을 그리기 위해 필요
#include "Engine/World.h"

UVirtualDistanceSensorComp::UVirtualDistanceSensorComp()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVirtualDistanceSensorComp::BeginPlay()
{
	Super::BeginPlay();

	if (MeasureInterval > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(MeasureTimerHandle, this, &UVirtualDistanceSensorComp::MeasureAndLogData, MeasureInterval, true);
	}
}

void UVirtualDistanceSensorComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(MeasureTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void UVirtualDistanceSensorComp::MeasureAndLogData()
{
	FVector StartLocation = GetComponentLocation();
	FVector ForwardDir = GetForwardVector();

	// 감지된 가장 가까운 거리를 기록할 변수 (초기값은 최대 거리보다 크게 설정)
	float ClosestDistance = MaxSensorRange + 1.0f;
	AActor* ClosestActor = nullptr;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());

	// 설정된 RayCount만큼 시야각 내에서 다중 스캔 수행
	for (int32 i = 0; i < RayCount; ++i)
	{
		// 시야각(ConeAngle) 내에서 무작위 방향 벡터 생성
		float HalfAngleRad = FMath::DegreesToRadians(SensorConeAngle * 0.5f);
		FVector RandomDir = FMath::VRandCone(ForwardDir, HalfAngleRad);

		FVector EndLocation = StartLocation + (RandomDir * MaxSensorRange);
		FHitResult HitResult;

		// 주의: Visibility 채널 감지가 안 되면 ECC_WorldDynamic 이나 ECC_Camera 로 변경해보세요.
		bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, QueryParams);

		if (bHit)
		{
			// 최소 감지 거리(사각지대) 밖인지 확인
			if (HitResult.Distance >= MinSensorRange)
			{
				// 현재까지 발견된 것 중 가장 가까운 물체인지 확인
				if (HitResult.Distance < ClosestDistance)
				{
					ClosestDistance = HitResult.Distance;
					ClosestActor = HitResult.GetActor();
				}
			}
		}

		// 에디터에서 볼 수 있도록 다중 광선 그리기 (초록: 충돌, 빨강: 미충돌)
		if (bDrawDebugLine)
		{
			DrawDebugLine(GetWorld(), StartLocation, bHit ? HitResult.ImpactPoint : EndLocation, bHit ? FColor::Green : FColor::Red, false, MeasureInterval * 0.8f);
		}
	}

	// === 최종 결과 처리 및 노이즈 추가 ===
	if (ClosestActor)
	{
		// 실제 센서처럼 설정된 범위 내에서 무작위 오차(Noise) 발생
		float RandomNoise = FMath::RandRange(-NoiseMargin, NoiseMargin);
		float FinalMeasuredDistance = ClosestDistance + RandomNoise;

		FString TargetName = ClosestActor->GetName();

		UE_LOG(LogTemp, Log, TEXT("[Advanced VirtualSensor] 거리: %.2f cm (노이즈: %+.2f) | 대상: %s"), FinalMeasuredDistance, RandomNoise, *TargetName);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[Advanced VirtualSensor] 감지 범위 내 물체 없음."));
	}
}
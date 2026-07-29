#include "VirtualSensorStressSceneActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AVirtualSensorStressSceneActor::AVirtualSensorStressSceneActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	StaticPrimitives = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("StaticPrimitives"));
	StaticPrimitives->SetupAttachment(SceneRoot);
	StaticPrimitives->SetMobility(EComponentMobility::Static);
	StaticPrimitives->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StaticPrimitives->SetCollisionResponseToAllChannels(ECR_Block);
	StaticPrimitives->SetCastShadow(false);

	MovingProxies = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("MovingProxies"));
	MovingProxies->SetupAttachment(SceneRoot);
	MovingProxies->SetMobility(EComponentMobility::Movable);
	MovingProxies->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MovingProxies->SetCollisionResponseToAllChannels(ECR_Block);
	MovingProxies->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		StaticPrimitives->SetStaticMesh(CubeMesh.Object);
		MovingProxies->SetStaticMesh(CubeMesh.Object);
	}
}

void AVirtualSensorStressSceneActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	GenerateStressScene();
}

void AVirtualSensorStressSceneActor::GenerateStressScene()
{
	if (!StaticPrimitives || !MovingProxies) return;
	StaticPrimitives->ClearInstances();
	MovingProxies->ClearInstances();
	MovingBaseTransforms.Reset();

	const int32 StaticColumns = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(StaticPrimitiveCount))));
	for (int32 Index = 0; Index < StaticPrimitiveCount; ++Index)
	{
		const int32 X = Index % StaticColumns;
		const int32 Y = Index / StaticColumns;
		const float HeightScale = 0.2f + static_cast<float>((Index * 17) % 9) * 0.05f;
		const FVector Location((X - StaticColumns / 2) * GridSpacingCm, (Y - StaticColumns / 2) * GridSpacingCm, 50.0f * HeightScale);
		StaticPrimitives->AddInstance(FTransform(FRotator::ZeroRotator, Location, FVector(0.45f, 0.45f, HeightScale)));
	}

	const int32 MovingColumns = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(MovingProxyCount))));
	MovingBaseTransforms.Reserve(MovingProxyCount);
	for (int32 Index = 0; Index < MovingProxyCount; ++Index)
	{
		const int32 X = Index % MovingColumns;
		const int32 Y = Index / MovingColumns;
		const FVector Location((X - MovingColumns / 2) * GridSpacingCm * 2.0f, (Y - MovingColumns / 2) * GridSpacingCm * 2.0f, 250.0f);
		const FTransform Instance(FRotator::ZeroRotator, Location, FVector(0.35f));
		MovingBaseTransforms.Add(Instance);
		MovingProxies->AddInstance(Instance);
	}
}

void AVirtualSensorStressSceneActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!MovingProxies || MovingBaseTransforms.IsEmpty() || !GetWorld()) return;
	const double NowSeconds = GetWorld()->GetTimeSeconds();
	if (NowSeconds < NextMovingUpdateSeconds) return;
	NextMovingUpdateSeconds = NowSeconds + 1.0 / FMath::Max(0.1f, MovingUpdateHz);

	TArray<FTransform> UpdatedTransforms;
	UpdatedTransforms.Reserve(MovingBaseTransforms.Num());
	for (int32 Index = 0; Index < MovingBaseTransforms.Num(); ++Index)
	{
		FTransform Updated = MovingBaseTransforms[Index];
		FVector Location = Updated.GetLocation();
		Location.Z += FMath::Sin(static_cast<float>(NowSeconds) * 1.7f + Index * 0.031f) * MovingAmplitudeCm;
		Updated.SetLocation(Location);
		UpdatedTransforms.Add(Updated);
	}
	MovingProxies->BatchUpdateInstancesTransforms(0, UpdatedTransforms, false, true, true);
}

#include "SlabTrackReferenceActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ASlabTrackReferenceActor::ASlabTrackReferenceActor()
{
	PrimaryActorTick.bCanEverTick=false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("TrackOrigin")));
	LeftRail=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftRailReference"));
	RightRail=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightRailReference"));
	CenterLine=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CenterlineReference"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	for(UStaticMeshComponent* Mesh:{LeftRail.Get(),RightRail.Get(),CenterLine.Get()})
	{
		Mesh->SetupAttachment(RootComponent); Mesh->SetStaticMesh(Cube.Object);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetHiddenInSceneCapture(true); Mesh->SetCastShadow(false);
		Mesh->SetAffectDynamicIndirectLighting(false); Mesh->SetAffectDistanceFieldLighting(false); Mesh->SetAffectIndirectLightingWhileHidden(false);
		Mesh->SetVisibleInRayTracing(false); Mesh->bVisibleInReflectionCaptures=false; Mesh->bVisibleInRealTimeSkyCaptures=false;
	}
}
bool ASlabTrackReferenceActor::HasValidRails() const
{
	return bRailsConfigured&&FMath::IsFinite(LeftRailYcm)&&FMath::IsFinite(RightRailYcm)&&LeftRailYcm<RightRailYcm;
}
void ASlabTrackReferenceActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const double L=FMath::Max(1.0,RailLengthCm),H=FMath::Max(1.0,RailHeightCm);
	LeftRail->SetRelativeLocation(FVector(0,LeftRailYcm-5,H/2)); RightRail->SetRelativeLocation(FVector(0,RightRailYcm+5,H/2));
	LeftRail->SetRelativeScale3D(FVector(L/100,0.1,H/100)); RightRail->SetRelativeScale3D(LeftRail->GetRelativeScale3D());
	CenterLine->SetRelativeLocation(FVector(0,0,2)); CenterLine->SetRelativeScale3D(FVector(L/100,0.015,0.015));
	for(UStaticMeshComponent* Mesh:{LeftRail.Get(),RightRail.Get(),CenterLine.Get()}) Mesh->SetVisibility(bShowReference);
}

#include "SlabVisualizationComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
USlabVisualizationComponent::USlabVisualizationComponent() { PrimaryComponentTick.bCanEverTick=false; }
void USlabVisualizationComponent::EnsureHelpers()
{
	if(!Helpers.IsEmpty()||!GetOwner()||!GetOwner()->GetRootComponent()) return;
	UStaticMesh* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	for(int32 I=0;I<7;++I)
	{
		auto* M=NewObject<UStaticMeshComponent>(GetOwner(),*FString::Printf(TEXT("SlabDiagnosticLine%d"),I));
		M->SetStaticMesh(Cube); M->SetupAttachment(GetOwner()->GetRootComponent()); M->SetMobility(EComponentMobility::Movable);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision); M->SetGenerateOverlapEvents(false); M->SetCastShadow(false); M->SetHiddenInSceneCapture(true);
		if(Material) { auto* MID=UMaterialInstanceDynamic::Create(Material,M); MID->SetVectorParameterValue(TEXT("Color"),I==6?FLinearColor::Red:FLinearColor(0,1,0.5)); M->SetMaterial(0,MID); }
		M->RegisterComponent(); M->SetVisibility(bHelpersVisible); Helpers.Add(M);
	}
}
void USlabVisualizationComponent::UpdateGeometry(const FVector& S)
{
	EnsureHelpers(); if(Helpers.Num()!=7) return;
	const double Z=S.Z/2+2,Stroke=FMath::Clamp(FMath::Min(S.X,S.Y)*0.003,0.8,4.0);
	const FVector Locations[]={FVector(0,-S.Y/2,Z),FVector(0,S.Y/2,Z),FVector(-S.X/2,0,Z),FVector(S.X/2,0,Z),FVector(0,0,Z+1),FVector(0,0,Z+1),FVector(0,0,Z+4)};
	const FVector Sizes[]={FVector(S.X,Stroke,Stroke),FVector(S.X,Stroke,Stroke),FVector(Stroke,S.Y,Stroke),FVector(Stroke,S.Y,Stroke),FVector(S.X,Stroke,Stroke),FVector(Stroke,S.Y,Stroke),FVector(Stroke*5,Stroke*5,Stroke*5)};
	for(int32 I=0;I<7;++I) { Helpers[I]->SetRelativeLocation(Locations[I]); Helpers[I]->SetRelativeScale3D(Sizes[I]/100); }
}
void USlabVisualizationComponent::SetHelpersVisible(bool bVisible)
{ bHelpersVisible=bVisible; for(UStaticMeshComponent* Helper:Helpers) if(Helper) Helper->SetVisibility(bVisible); }
void USlabVisualizationComponent::EndPlay(const EEndPlayReason::Type Reason)
{ for(UStaticMeshComponent* Helper:Helpers) if(Helper) Helper->DestroyComponent(); Helpers.Reset(); Super::EndPlay(Reason); }

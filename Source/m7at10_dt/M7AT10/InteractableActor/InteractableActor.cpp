// Fill out your copyright notice in the Description page of Project Settings.


#include "InteractableActor.h"


// Sets default values
AInteractableActor::AInteractableActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AInteractableActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AInteractableActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AInteractableActor::Click()
{
	// WidgetFlag가 비어있으면 리턴
	if (WidgetFlag.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("InteractableActor::Click - WidgetFlag is empty!"));
		return;
	}

	// DxWidgetSubsystem 가져오기
	UGameInstance* GameInstance = GetWorld()->GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("InteractableActor::Click - GameInstance is null!"));
		return;
	}

	UDxWidgetSubsystem* WidgetSubsystem = GameInstance->GetSubsystem<UDxWidgetSubsystem>();
	if (!WidgetSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("InteractableActor::Click - DxWidgetSubsystem is null!"));
		return;
	}

	// Subsystem을 통해 위젯 열기
	WidgetSubsystem->OpenWidget(this);
}

void AInteractableActor::Hover(TArray<UPrimitiveComponent*> mesh)
{
	// for (UPrimitiveComponent* Mesh : GetActorAllMesh())
	// {
	// 	if (Mesh && Mesh->ComponentHasTag(TEXT("Highlightable")))
	// 	{
	// 		HighlightTagMehes.Add(Mesh);
	// 	}
	// }
	// if (!ShortcutHighlight)
	// {
	// 	HighlightActor(true, HighlightTagMeshes, false);
	// 	ShortcutHighlight = true;
	// }
	if (!ShortcutHighlight)
	{
		HighlightActor(true, mesh, false);
		ShortcutHighlight = true;
	}

	// ReceivedHover();
}

void AInteractableActor::NotHover(TArray<UPrimitiveComponent*> mesh)
{
	// for (UPrimitiveComponent* Mesh : GetActorAllMesh())
	// {
	// 	if (Mesh && Mesh->ComponentHasTag(TEXT("Highlightable")))
	// 	{
	// 		HighlightTagMehes.Add(Mesh);
	// 	}
	// }
	// if (HighlightTagMeshe.Num() == 0)
	// {
	// 	HighlightTagMehes = GetActorAllMesh();
	// }
	// if (!ShortcutHighlight)
	// {
	// 	HighlightActor(true, HighlightTagMeshes, false);
	// 	ShortcutHighlight = true;
	// }
	if (ShortcutHighlight)
	{
		HighlightActor(false, mesh, false);
		ShortcutHighlight = false;
	}

	// ReceivedNotHover();
}

TArray<UPrimitiveComponent*> AInteractableActor::GetActorAllMesh()
{
	TArray<UPrimitiveComponent*> meshes;

	// StaticMeshComponent 수집
	TArray<UStaticMeshComponent*> staticMeshes;
	GetComponents<UStaticMeshComponent>(staticMeshes);
	for (UStaticMeshComponent* mesh : staticMeshes)
	{
		if (mesh)
		{
			meshes.Add(mesh);
		}
	}

	// SkeletalMeshComponent 수집
	TArray<USkeletalMeshComponent*> skeletalMeshes;
	GetComponents<USkeletalMeshComponent>(skeletalMeshes);
	for (USkeletalMeshComponent* mesh : skeletalMeshes)
	{
		if (mesh)
		{
			meshes.Add(mesh);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("GetActorAllMesh found %d meshes for Actor: %s"), meshes.Num(), *GetName());

	return meshes;
}

// void AInteractableActor::HighlightSingleMesh(UPrimitiveComponent* Mesh, bool bActivate, bool isError)
// {
// 	if (!Mesh)
// 	{
// 		UE_LOG(LogTemp, Warning, TEXT("HighlightSingleMesh - Mesh is null!"));
// 		return;
// 	}
//
// 	if (bActivate)
// 	{
// 		Mesh->SetRenderCustomDepth(true);
// 		Mesh->SetCustomDepthStencilValue(isError ? 254 : 100);
// 	}
// 	else
// 	{
// 		Mesh->SetRenderCustomDepth(false);
// 		Mesh->SetCustomDepthStencilValue(0);
// 	}
// }
//
// void AInteractableActor::HighlightMultipleMeshes(const TArray<UPrimitiveComponent*>& Meshes, bool bActivate
// 	, bool bIsError)
// {
// }

void AInteractableActor::HighlightActor(bool activate, TArray<UPrimitiveComponent*> mesh, bool isError)
{
	if (activate == true)
	{
		for (UPrimitiveComponent* arr : mesh)
		{
			arr->SetRenderCustomDepth(true);
			if (isError)
				arr->SetCustomDepthStencilValue(254);
			else
				arr->SetCustomDepthStencilValue(100);
		}
	}
	else
	{
		for (UPrimitiveComponent* arr : mesh)
		{
			arr->SetRenderCustomDepth(false);
			arr->SetCustomDepthStencilValue(0);
		}
	}
}

void AInteractableActor::HighlightSingleMesh(bool activate, UPrimitiveComponent* mesh, bool isError)
{
	if (!mesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("HighlightSingleMesh - Mesh is null!"));
		return;
	}

	if (activate)
	{
		mesh->SetRenderCustomDepth(true);
		mesh->SetCustomDepthStencilValue(isError ? 254 : 100);
	}
	else
	{
		mesh->SetRenderCustomDepth(false);
		mesh->SetCustomDepthStencilValue(0);
	}
}
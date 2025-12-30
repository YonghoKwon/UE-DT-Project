#include "InteractableActor.h"

AInteractableActor::AInteractableActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AInteractableActor::BeginPlay()
{
	Super::BeginPlay();

	// TODO: 생성되면서 Mesh를 추가하는 경우 별도 작업 필요
	CachedMeshes = GetActorAllMesh();
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

void AInteractableActor::OnCursorHover(UPrimitiveComponent* HoveredComponent)
{
	if (LastHoveredComponent == HoveredComponent)
	{
		return;
	}

	// 새로운 컴포넌트이므로 갱신
	LastHoveredComponent = HoveredComponent;

	// 하이라이트 업데이트 로직 실행
	UpdateHighlight(HoveredComponent);
}

void AInteractableActor::OnCursorUnhover()
{
	// 하이라이트 끄기
	if (CurrentHighlightedMeshes.Num() > 0)
	{
		HighlightActor(false, CurrentHighlightedMeshes, false);
		CurrentHighlightedMeshes.Empty();
	}

	// 마우스가 떠났으므로 초기화
	LastHoveredComponent = nullptr;
}

void AInteractableActor::HighlightActor(bool activate, const TArray<UPrimitiveComponent*> meshes, bool isError)
{
	if (activate == true)
	{
		for (const auto& mesh : meshes)
		{
			mesh->SetRenderCustomDepth(true);
			if (isError)
				mesh->SetCustomDepthStencilValue(254);
			else
				mesh->SetCustomDepthStencilValue(100);
		}
	}
	else
	{
		for (const auto& mesh : meshes)
		{
			mesh->SetRenderCustomDepth(false);
			mesh->SetCustomDepthStencilValue(0);
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

void AInteractableActor::UpdateHighlight(UPrimitiveComponent* NewHoveredComponent)
{
	if (!NewHoveredComponent) return;

	// 기존 하이라이트 끄기 (이제 컴포넌트가 바뀔 때만 실행됨)
	HighlightActor(false, CurrentHighlightedMeshes, false);
	CurrentHighlightedMeshes.Empty();

	if (HighlightMode == EHighlightMode::WholeActor || NewHoveredComponent == RootComponent)
	{
		CurrentHighlightedMeshes = CachedMeshes;
	}
	else // IndividualMesh
	{
		CurrentHighlightedMeshes.Add(NewHoveredComponent);
		WidgetFlag = NewHoveredComponent->GetName();
	}

	// 새 하이라이트 켜기
	HighlightActor(true, CurrentHighlightedMeshes, false);
}
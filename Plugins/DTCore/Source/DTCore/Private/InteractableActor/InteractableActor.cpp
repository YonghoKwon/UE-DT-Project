#include "InteractableActor/InteractableActor.h"

#include "DTCore.h"
#include "Components/PoseableMeshComponent.h"
#include "Player/DxPlayerBase.h"

AInteractableActor::AInteractableActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AInteractableActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AInteractableActor::Click()
{
	// WidgetFlag가 비어있으면 리턴
	if (WidgetFlag == EDxWidgetFlag::None)
	{
		UE_LOG(LogBase, Warning, TEXT("InteractableActor::Click - WidgetFlag is empty!"));
		return;
	}

	// DxWidgetSubsystem 가져오기
	UGameInstance* GameInstance = GetWorld()->GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogBase, Error, TEXT("InteractableActor::Click - GameInstance is null!"));
		return;
	}

	UDxWidgetSubsystem* WidgetSubsystem = GameInstance->GetSubsystem<UDxWidgetSubsystem>();
	if (!WidgetSubsystem)
	{
		UE_LOG(LogBase, Error, TEXT("InteractableActor::Click - DxWidgetSubsystem is null!"));
		return;
	}

	// Subsystem을 통해 위젯 열기
	WidgetSubsystem->OpenWidget(this);
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
	// [최적화] 루프 밖에서 로그 한 번만 출력 (디버깅 필요시에만 주석 해제)
	// UE_LOG(LogBase, Verbose, TEXT("HighlightActor: Activate=%d, MeshCount=%d"), activate, meshes.Num());
	const int32 TargetStencilValue = isError ? 254 : 100;

	for (const auto& mesh : meshes)
	{
		if (!mesh) continue;

		if (mesh->bRenderCustomDepth == activate && mesh->CustomDepthStencilValue == TargetStencilValue)
		{
			continue;
		}

		// 값 변경
		mesh->SetRenderCustomDepth(activate);
		mesh->SetCustomDepthStencilValue(TargetStencilValue);
	}
}

void AInteractableActor::HighlightSingleMesh(bool activate, UPrimitiveComponent* mesh, bool isError)
{
	if (!mesh)
	{
		UE_LOG(LogBase, Warning, TEXT("HighlightSingleMesh - Mesh is null!"));
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

TArray<UPrimitiveComponent*> AInteractableActor::GetActorAllMesh()
{
	TArray<UPrimitiveComponent*> meshes;

	// RootComponent가 PrimitiveComponent인 경우 먼저 추가
	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(RootComponent))
	{
		meshes.Add(RootPrimitive);
		UE_LOG(LogBase, Log, TEXT("GetActorAllMesh - Added RootComponent: %s"), *RootPrimitive->GetName());
	}


	// StaticMeshComponent 수집
	TArray<UStaticMeshComponent*> staticMeshes;
	GetComponents<UStaticMeshComponent>(staticMeshes);
	for (UStaticMeshComponent* mesh : staticMeshes)
	{
		if (mesh && mesh != RootComponent) // RootComponent는 이미 추가했으므로 중복 방지
		{
			meshes.Add(mesh);
		}
	}

	// SkeletalMeshComponent 수집
	TArray<USkeletalMeshComponent*> skeletalMeshes;
	GetComponents<USkeletalMeshComponent>(skeletalMeshes);
	for (USkeletalMeshComponent* mesh : skeletalMeshes)
	{
		if (mesh && mesh != RootComponent) // RootComponent는 이미 추가했으므로 중복 방지
		{
			meshes.Add(mesh);
		}
	}

	// PoseableMeshComponent 수집
	TArray<UPoseableMeshComponent*> poseableMeshes;
	GetComponents<UPoseableMeshComponent>(poseableMeshes);
	for (UPoseableMeshComponent* mesh : poseableMeshes)
	{
		if (mesh && mesh != RootComponent) // RootComponent는 이미 추가했으므로 중복 방지
		{
			meshes.Add(mesh);
		}
	}

	UE_LOG(LogBase, Warning, TEXT("GetActorAllMesh found %d meshes for Actor: %s"), meshes.Num(), *GetName());

	return meshes;
}

void AInteractableActor::BindToPlayer()
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (ADxPlayerBase* Player = Cast<ADxPlayerBase>(PC->GetPawn()))
		{
			CachedPlayer = Player;
		}
	}
}

void AInteractableActor::UnbindFromPlayer()
{
	if (CachedPlayer)
	{
		CachedPlayer = nullptr;
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
		// UpdateWidgetFlagForPlayer();
	}
	else // IndividualMesh
	{
		CurrentHighlightedMeshes.Add(NewHoveredComponent);
		// WidgetFlag = NewHoveredComponent->GetName();
	}

	// 새 하이라이트 켜기
	HighlightActor(true, CurrentHighlightedMeshes, false);
}

void AInteractableActor::BeginPlay()
{
	Super::BeginPlay();

	// TODO: 생성되면서 Mesh를 추가하는 경우 별도 작업 필요
	CachedMeshes = GetActorAllMesh();

	// Player 바인딩
	BindToPlayer();
}
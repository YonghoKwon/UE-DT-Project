#include "DxPlayerControllerBase.h"
#include "DxPlayerBase.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "m7at10_dt/M7AT10/InteractableActor/InteractableActor.h"


ADxPlayerControllerBase::ADxPlayerControllerBase()
{
}

void ADxPlayerControllerBase::BeginPlay()
{
	Super::BeginPlay();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		FModifyContextOptions Options;
		Options.bIgnoreAllPressedKeysUntilRelease = false;
		Subsystem->AddMappingContext(PlayerContext, 0, Options);
	}

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	SetInputMode(FInputModeGameAndUI());
	CurrentMouseCursor = EMouseCursor::Default;

	SetActorTickEnabled(true);
}

void ADxPlayerControllerBase::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInputComponent->BindAction(MovementAction, ETriggerEvent::Triggered, this, &ADxPlayerControllerBase::Move);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADxPlayerControllerBase::Look);
		EnhancedInputComponent->BindAction(MouseWheelAction, ETriggerEvent::Triggered, this, &ADxPlayerControllerBase::ControlMoveSpeed);
		EnhancedInputComponent->BindAction(UpDownAction, ETriggerEvent::Triggered, this, &ADxPlayerControllerBase::MoveUpDown);

		EnhancedInputComponent->BindAction(LeftMouseButtonAction, ETriggerEvent::Triggered, this, &ADxPlayerControllerBase::ClickLeftMouseButton);
		EnhancedInputComponent->BindAction(RightMouseButtonAction, ETriggerEvent::Triggered, this, &ADxPlayerControllerBase::ClickRightMouseButton);

		// 일반 Number 버튼 추가 (추가 바인딩 필요 시)
		// for (int32 i = 0; i < NumberKeyActions.Num(); ++i)
		// {
		// 	if (NumberKeyActions[i])
		// 	{
		// 		EnhancedInputComponent->BindAction(NumberKeyActions[i], ETriggerEvent::Triggered, this, &ADxPlayerControllerBase::OnNumberKeyPressed, i);
		// 	}
		// }
	}
}

void ADxPlayerControllerBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ADxPlayerBase* DxPlayer = Cast<ADxPlayerBase>(GetPawn());
	if (!DxPlayer) return;

	// UI 입력으로 이동 처리
	if (UIMoveInput != FVector2D::ZeroVector)
	{
		DxPlayer->Move(UIMoveInput);
	}

	// UI 입력으로 회전 처리
	if (UILookInput != FVector2D::ZeroVector)
	{
		DxPlayer->Look(UILookInput);
	}

	// UI 입력으로 수직 이동 처리
	if (UIVerticalInput != 0.0f)
	{
		DxPlayer->MoveUpDown(UIVerticalInput);
	}

	// 마우스 호버 감지
	CheckMouseHover();
}

// 이동 제어
void ADxPlayerControllerBase::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if (ADxPlayerBase* DxPlayer = Cast<ADxPlayerBase>(GetPawn()))
	{
		DxPlayer->Move(MovementVector);
	}
}
void ADxPlayerControllerBase::Look(const FInputActionValue& Value)
{
	const float value = Value.Get<float>();

	if (ADxPlayerBase* DxPlayer = Cast<ADxPlayerBase>(GetPawn()))
	{
		DxPlayer->MoveUpDown(value);
	}
}
// 시점 변경
void ADxPlayerControllerBase::MoveUpDown(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();
	if (ADxPlayerBase* DxPlayer = Cast<ADxPlayerBase>(GetPawn()))
	{
		DxPlayer->Look(LookVector);
	}
}
// 배속 제어
void ADxPlayerControllerBase::ControlMoveSpeed(const FInputActionValue& Value)
{
	const float value = Value.Get<float>() * ControlSpeedStep;
	if (value == 0.f) return;

	if (ADxPlayerBase* DxPlayer = Cast<ADxPlayerBase>(GetPawn()))
	{
		DxPlayer->SetControlSpeed(value);
	}
}

void ADxPlayerControllerBase::ClickLeftMouseButton(const FInputActionValue& Value)
{
	const bool value = Value.Get<bool>();

	if (PossibleClick && !value) // 마우스 버튼을 뗏을 때만 처리
	{
		// 현재 호버된 InteractableActor를 클릭 (개별 메시든 전체 액터든 CurrentHoveredActor에 저장됨)
		if (CurrentHoveredActor)
		{
			CurrentHoveredActor->Click();
		}
	}
}

// 우클릭 제어

void ADxPlayerControllerBase::ClickRightMouseButton(const FInputActionValue& Value)
{
	const bool value = Value.Get<bool>();

	if (value)
	{
		bIsClickRightMouseButton = true;

		bShowMouseCursor = false;
		bEnableClickEvents = false;
		bEnableMouseOverEvents = false;
		SetInputMode(FInputModeGameOnly());
	}
	else
	{
		bIsClickRightMouseButton = false;

		bShowMouseCursor = true;
		bEnableClickEvents = true;
		bEnableMouseOverEvents = true;
		SetInputMode(FInputModeGameAndUI());
	}
}

// 마우스 호버 감지
void ADxPlayerControllerBase::CheckMouseHover()
{
	FVector WorldLocation, WorldDirection;
	if (!DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
	{
		return;
	}

	FVector Start = WorldLocation;
	FVector End = Start + (WorldDirection * 100000.0f); // 거리율에 따라 hover 인식 달라짐

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetPawn());

	FHitResult HoverHitResult;

	// 모든 ObjectType 추가로 오브젝트 감지
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);

	bool bHit = GetWorld()->LineTraceSingleByObjectType(
		HoverHitResult,
		Start,
		End,
		ObjectQueryParams,
		QueryParams
		);

	// Hit 지점에 초록색 구체 (Hover 디버그용)
	// if (bHit)
	// {
	// 	DrawDebugSphere(GetWorld(), HoverHitResult.ImpactPoint, 100.0f, 12, FColor::Green, false, 0.1f);
	// }

	if (bHit)
	{
		UPrimitiveComponent* HoveredMesh = HoverHitResult.GetComponent();
		AActor* HoveredActor = HoverHitResult.GetActor();

		if (!HoveredMesh || !HoveredActor)
		{
			// 이전 호버 해제
			if (CurrentHoveredMesh)
			{
				AInteractableActor* InteractableActor = Cast<AInteractableActor>(CurrentHoveredMesh->GetOwner());
				if (InteractableActor && InteractableActor->HighlightMode == EHighlightMode::IndividualMesh)
				{
					InteractableActor->HighlightSingleMesh(false, CurrentHoveredMesh, false);
				}
				CurrentHoveredMesh = nullptr;
			}
			if (CurrentHoveredActor)
			{
				if (CurrentHoveredActor->HighlightMode == EHighlightMode::WholeActor)
				{
					CurrentHoveredActor->NotHover(CurrentHoveredActor->GetActorAllMesh());
				}
				CurrentHoveredActor = nullptr;
			}
			return;
		}

		AInteractableActor* InteractableActor = Cast<AInteractableActor>(HoveredActor);
		if (!InteractableActor)
		{
			// InteractableActor가 아니면 이전 호버 해제
			if (CurrentHoveredMesh)
			{
				AInteractableActor* PreActor = Cast<AInteractableActor>(CurrentHoveredMesh->GetOwner());
				if (PreActor && PreActor->HighlightMode == EHighlightMode::IndividualMesh)
				{
					TArray<UPrimitiveComponent*> meshes = {};
					meshes.Add(CurrentHoveredMesh);
					PreActor->NotHover(meshes);
				}
				CurrentHoveredMesh = nullptr;
			}
			if (CurrentHoveredActor)
			{
				if (CurrentHoveredActor->HighlightMode == EHighlightMode::WholeActor)
				{
					CurrentHoveredActor->NotHover(CurrentHoveredActor->GetActorAllMesh());
				}
				CurrentHoveredActor = nullptr;
			}
			return;
		}

		if (InteractableActor->HighlightMode == EHighlightMode::IndividualMesh)
		{
			bool bIsRootMesh = (RootComponent && HoveredMesh == RootComponent);

			if (bIsRootMesh)
			{
				// 루트 메시는 액터 전체로 처리
				if (CurrentHoveredActor != InteractableActor)
				{
					// 이전 메시 하이라이트 해제
					if (CurrentHoveredMesh)
					{
						AInteractableActor* PreActor = Cast<AInteractableActor>(CurrentHoveredMesh->GetOwner());
						if (PreActor && PreActor->HighlightMode == EHighlightMode::IndividualMesh)
						{
							TArray<UPrimitiveComponent*> meshes = {};
							meshes.Add(CurrentHoveredMesh);
							PreActor->NotHover(meshes);
						}
					}

					// 이전 액터 전체 하이라이트 해제
					if (CurrentHoveredActor && CurrentHoveredActor->HighlightMode == EHighlightMode::WholeActor)
					{
						CurrentHoveredActor->NotHover(CurrentHoveredActor->GetActorAllMesh());
					}

					// 액터 전체 하이라이트
					CurrentHoveredMesh = nullptr; // 루트는 개별 메시가 아님
					CurrentHoveredActor = InteractableActor;
					InteractableActor->Hover(InteractableActor->GetActorAllMesh());
				}
			}
			else
			{
				// 개별 메시 하이라이트 모드 (루트가 아닌 메시)
				if (CurrentHoveredMesh != HoveredMesh)
				{
					// 이전 메시 하이라이트 해제
					if (CurrentHoveredMesh)
					{
						AInteractableActor* PreActor = Cast<AInteractableActor>(CurrentHoveredMesh->GetOwner());
						if (PreActor && PreActor->HighlightMode == EHighlightMode::IndividualMesh)
						{
							TArray<UPrimitiveComponent*> meshes = {};
							meshes.Add(CurrentHoveredMesh);
							PreActor->NotHover(meshes);
						}
					}

					// 이전 액터 전체 하이라이트 해제 (모드가 바뀐 경우)
					if (CurrentHoveredActor && CurrentHoveredActor->HighlightMode == EHighlightMode::WholeActor)
					{
						CurrentHoveredActor->NotHover(CurrentHoveredActor->GetActorAllMesh());
					}

					// 새 메시 하이라이트
					CurrentHoveredMesh = HoveredMesh;
					CurrentHoveredActor = InteractableActor;
					TArray<UPrimitiveComponent*> meshes = {};
					meshes.Add(HoveredMesh);
					// 개별 메시 이름으로 WidgetFlag 설정
					InteractableActor->WidgetFlag = HoveredMesh->GetName();
					InteractableActor->Hover(meshes);
				}
			}
		}
		else // EHighlightMode::WholeActor
		{
			// 액터 전체 하이라이트 모드
			if (CurrentHoveredActor != InteractableActor)
			{
				// 이전 메시 하이라이트 해제 (모드가 바뀐 경우)
				if (CurrentHoveredMesh)
				{
					AInteractableActor* PreActor = Cast<AInteractableActor>(CurrentHoveredMesh->GetOwner());
					if (PreActor && PreActor->HighlightMode == EHighlightMode::IndividualMesh)
					{
						TArray<UPrimitiveComponent*> meshes = {};
						meshes.Add(CurrentHoveredMesh);
						PreActor->NotHover(meshes);
					}
					CurrentHoveredMesh = nullptr;
				}
			}

			// 이전 액터 하이라이트 해제
			if (CurrentHoveredActor && CurrentHoveredActor->HighlightMode == EHighlightMode::WholeActor)
			{
				CurrentHoveredActor->NotHover(CurrentHoveredActor->GetActorAllMesh());
			}

			// 새 액터 전체 하이라이트
			CurrentHoveredActor = InteractableActor;
			CurrentHoveredActor->Hover(CurrentHoveredActor->GetActorAllMesh());
		}
	}
	else
	{
		// 아무것도 호버되지 않음
		if (CurrentHoveredMesh)
		{
			AInteractableActor* PreActor = Cast<AInteractableActor>(CurrentHoveredMesh->GetOwner());
			if (PreActor && PreActor->HighlightMode == EHighlightMode::IndividualMesh)
			{
				TArray<UPrimitiveComponent*> meshes = {};
				meshes.Add(CurrentHoveredMesh);
				PreActor->NotHover(meshes);
			}
			CurrentHoveredMesh = nullptr;
		}
		if (CurrentHoveredActor)
		{
			if (CurrentHoveredActor->HighlightMode == EHighlightMode::WholeActor)
			{
				CurrentHoveredActor->NotHover(CurrentHoveredActor->GetActorAllMesh());
			}
			CurrentHoveredActor = nullptr;
		}
	}
}

// 버튼으로 이동 제어
void ADxPlayerControllerBase::SetUIMoveInput(const FVector2D& MoveInput)
{
	UIMoveInput = MoveInput;
}

void ADxPlayerControllerBase::SetUILookInput(const FVector2D& LookInput)
{
	UILookInput = LookInput;
}

void ADxPlayerControllerBase::SetUIVerticalInput(float Value)
{
	UIVerticalInput = Value;
}

// 플레이어 속도 순환
void ADxPlayerControllerBase::CyclePlayerControlSpeed()
{
	if (ADxPlayerBase* DxPlayer = Cast<ADxPlayerBase>(GetPawn()))
	{
		DxPlayer->CycleControlSpeed();
	}
}

// 플레이어 속도 가져오기
float ADxPlayerControllerBase::GetPlayerControlSpeed()
{
	if (ADxPlayerBase* DxPlayer = Cast<ADxPlayerBase>(GetPawn()))
	{
		return DxPlayer->GetControlSpeed();
	}
	return 0.0f;
}
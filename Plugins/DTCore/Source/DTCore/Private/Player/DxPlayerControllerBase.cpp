#include "Player/DxPlayerControllerBase.h"

#include "Player/DxPlayerBase.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InteractableActor/InteractableActor.h"


ADxPlayerControllerBase::ADxPlayerControllerBase()
{
}

void ADxPlayerControllerBase::BeginPlay()
{
	Super::BeginPlay();

	DxPlayerBase = Cast<ADxPlayerBase>(GetPawn());

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

		EnhancedInputComponent->BindAction(RightMouseButtonAction, ETriggerEvent::Triggered, this, &ADxPlayerControllerBase::ClickRightMouseButton);
		EnhancedInputComponent->BindAction(LeftMouseButtonAction, ETriggerEvent::Triggered, this, &ADxPlayerControllerBase::ClickLeftMouseButton);

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
	if (!DxPlayerBase) return;

	// UI 입력으로 이동 처리
	if (UIMoveInput != FVector2D::ZeroVector)
	{
		DxPlayerBase->Move(UIMoveInput);
	}

	// UI 입력으로 회전 처리
	if (UILookInput != FVector2D::ZeroVector)
	{
		DxPlayerBase->Look(UILookInput);
	}

	// UI 입력으로 수직 이동 처리
	if (UIVerticalInput != 0.0f)
	{
		DxPlayerBase->MoveUpDown(UIVerticalInput);
	}

	LastHoverCheckTime += DeltaTime;
	if (!bIsClickRightMouseButton && bShowMouseCursor && LastHoverCheckTime >= HoverCheckInterval)
	{
		CheckMouseHover();
		LastHoverCheckTime = 0.0f; // 타이머 초기화
	}
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
void ADxPlayerControllerBase::MoveUpDown(const FInputActionValue& Value)
{
	const float value = Value.Get<float>();

	if (ADxPlayerBase* DxPlayer = Cast<ADxPlayerBase>(GetPawn()))
	{
		DxPlayer->MoveUpDown(value);
	}
}
// 시점 변경
void ADxPlayerControllerBase::Look(const FInputActionValue& Value)
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
	const float value = GetPlayerControlSpeed() + Value.Get<float>() * ControlSpeedStep;
	if (value == 0.f) return;

	if (ADxPlayerBase* DxPlayer = Cast<ADxPlayerBase>(GetPawn()))
	{
		DxPlayer->SetControlSpeed(value);
	}
}

void ADxPlayerControllerBase::ClickLeftMouseButton(const FInputActionValue& Value)
{
	const bool value = Value.Get<bool>();

	if (PossibleClick && !value) // 마우스 버튼을 뗐을 때만 처리
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
	// 마우스 위치 계산 (Tick에서 호출 조건 확인 후 실행됨)
	FVector WorldLocation, WorldDirection;
	if (!DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
	{
		return;
	}

	// 트레이스 설정 (매번 생성 비용 절약)
	static const FCollisionObjectQueryParams ObjectQueryParams = []()
	{
		FCollisionObjectQueryParams Params;
		Params.AddObjectTypesToQuery(ECC_WorldStatic);
		Params.AddObjectTypesToQuery(ECC_WorldDynamic);
		Params.AddObjectTypesToQuery(ECC_Pawn);
		Params.AddObjectTypesToQuery(ECC_PhysicsBody);
		Params.AddObjectTypesToQuery(ECC_GameTraceChannel1);
		return Params;
	}();

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetPawn());

	FHitResult HitResult;
	FVector Start = WorldLocation;
	FVector End = Start + (WorldDirection * 100000.0f); // 100km

	// 라인 트레이스
	bool bHit = GetWorld()->LineTraceSingleByObjectType(
		HitResult, Start, End, ObjectQueryParams, QueryParams
	);

	AActor* HitActorRaw = HitResult.GetActor();

	// 같은 액터 위에서 마우스가 움직일 때는 로직 건너뜀
	if (CurrentHoveredActor && CurrentHoveredActor == HitActorRaw)
	{
		return;
	}

	// 기존 액터 Unhover
	if (CurrentHoveredActor)
	{
		CurrentHoveredActor->OnCursorUnhover();
		CurrentHoveredActor = nullptr;
		CurrentHoveredMesh = nullptr;
	}

	// 새 액터 Hover
	if (AInteractableActor* NewHitActor = Cast<AInteractableActor>(HitActorRaw))
	{
		UPrimitiveComponent* NewHitComp = HitResult.GetComponent();

		NewHitActor->OnCursorHover(NewHitComp);
		CurrentHoveredActor = NewHitActor;
		CurrentHoveredMesh = NewHitComp;
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
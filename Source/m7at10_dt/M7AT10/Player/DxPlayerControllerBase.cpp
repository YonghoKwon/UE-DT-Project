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
	// FHitResult HitResult;
	// bool bHit = GetHitResultUnderCursor(ECC_Visibility, false, HitResult);

	FVector WorldLocation, WorldDirection;
	if (!DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
	{
		return;
	}

	FVector Start = WorldLocation;
	FVector End = Start + (WorldDirection * 100000.0f);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetPawn());

	FHitResult HitResult;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_GameTraceChannel1);

	bool bHit = GetWorld()->LineTraceSingleByObjectType(
		HitResult,
		Start,
		End,
		ObjectQueryParams,
		QueryParams
		);

	AInteractableActor* NewHitActor = nullptr;
	UPrimitiveComponent* NewHitComp = nullptr;

	if (bHit)
	{
		NewHitActor = Cast<AInteractableActor>(HitResult.GetActor());
		NewHitComp = HitResult.GetComponent();
	}

	// 마우스가 다른 액터로 이동했거나, 허공으로 갔을 때 -> 기존 액터 Unhover
	if (CurrentHoveredActor && CurrentHoveredActor != NewHitActor)
	{
		CurrentHoveredActor->OnCursorUnhover();
		CurrentHoveredActor = nullptr;
	}

	// 새로운 InteractableActor를 가리키고 있을 때
	if (NewHitActor)
	{
		// 새로 호버링 시작 (또는 같은 액터 내에서 컴포넌트 변경)
		NewHitActor->OnCursorHover(NewHitComp);

		// 현재 액터 갱신
		CurrentHoveredActor = NewHitActor;
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
#include "SlabActor.h"
#include "SlabScenarioCodec.h"
#include "SlabDataSyncComponent.h"
#include "SlabMotionComponent.h"
#include "SlabVisualizationComponent.h"
#include "SlabMetricsComponent.h"
#include "SlabTrackReferenceActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "Core/DxProcessSubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Async/Async.h"

ASlabActor::ASlabActor()
{
	PrimaryActorTick.bCanEverTick=false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SlabRoot")));
	SlabMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SlabMesh")); SlabMesh->SetupAttachment(RootComponent);
	SlabMesh->SetMobility(EComponentMobility::Movable); SlabMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SlabMesh->SetCollisionResponseToAllChannels(ECR_Block); SlabMesh->SetGenerateOverlapEvents(false); SlabMesh->SetSimulatePhysics(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube")); SlabMesh->SetStaticMesh(Cube.Object);
	DataSyncComponent=CreateDefaultSubobject<USlabDataSyncComponent>(TEXT("SlabDataSync"));
	MotionComponent=CreateDefaultSubobject<USlabMotionComponent>(TEXT("SlabMotion"));
	VisualizationComponent=CreateDefaultSubobject<USlabVisualizationComponent>(TEXT("SlabVisualization"));
	MetricsComponent=CreateDefaultSubobject<USlabMetricsComponent>(TEXT("SlabMetrics"));
	SurfaceMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurface.M_SlabSurface")));
	HighlightMode=EHighlightMode::IndividualMesh; DisplayName=TEXT("시나리오 Slab");
}
void ASlabActor::OnConstruction(const FTransform& Transform)
{ Super::OnConstruction(Transform); UpdateDimensions(); UpdateAppearance(); }
void ASlabActor::BeginPlay()
{
	Super::BeginPlay(); InitialTrackTransform=GetActorTransform(); InitialTrackTransform.SetScale3D(FVector::OneVector);
	RunTrackTransform=GetTrackTransform(); UpdateDimensions(); UpdateAppearance(); VisualizationComponent->UpdateGeometry(GetSizeCm());
	if(UGameInstance* GI=GetGameInstance())
	{
		if(auto* Process=GI->GetSubsystem<UDxProcessSubsystem>())
		{
			if(ReceiverId.IsEmpty()||Process->FindComponent(ReceiverId)) ReportFailure(TEXT("Slab 수신 ID가 비었거나 중복되었습니다."));
			else { Process->RegisterComponent(ReceiverId,DataSyncComponent); bRegistered=true; }
		}
		if(bRegistered) if(auto* Replay=GI->GetSubsystem<USlabScenarioReplaySubsystem>()) Replay->RegisterPlaybackAdapter(this);
	}
}
void ASlabActor::EndPlay(const EEndPlayReason::Type Reason)
{
	bEnding=true; ++ParseGeneration; bParsing=false;
	if(IsSimulationActive()) FinishSimulation(true);
	if(UGameInstance* GI=GetGameInstance())
	{
		if(auto* Replay=GI->GetSubsystem<USlabScenarioReplaySubsystem>()) Replay->UnregisterPlaybackAdapter(this);
		if(auto* Process=GI->GetSubsystem<UDxProcessSubsystem>())
			if(bRegistered&&Process->FindComponent(ReceiverId)==DataSyncComponent) Process->UnregisterComponent(ReceiverId);
	}
	Scenario.Reset(); Super::EndPlay(Reason);
}
bool ASlabActor::IsSimulationActive() const { return Status.State==ESlabSimulationState::Playing||Status.State==ESlabSimulationState::Paused; }
void ASlabActor::ReportFailure(const FString& Message)
{
	// A refused incoming run must never change the active run state.
	if(!IsSimulationActive()) Status.State=ESlabSimulationState::Failed;
	Status.Message=Message; OnSlabStateChanged.Broadcast();
	UE_LOG(LogTemp,Warning,TEXT("[SlabScenario] %s"),*Message);
}
bool ASlabActor::SubmitScenarioJson(const FString& Json)
{
	if(!IsInGameThread()||bEnding||bParsing) return false;
	FTCHARToUTF8 Bytes(*Json); if(Bytes.Length()>8*1024*1024) { ReportFailure(TEXT("JSON 8MiB 한도를 초과했습니다.")); return false; }
	bParsing=true; const uint64 Revision=++ParseGeneration; TWeakObjectPtr<ASlabActor> Weak(this);
	Async(EAsyncExecution::ThreadPool,[Weak,Revision,Json]()
	{
		FSlabScenarioDataPtr Parsed; FString Error; FSlabScenarioCodec::Parse(Json,Parsed,Error,true);
		AsyncTask(ENamedThreads::GameThread,[Weak,Revision,Parsed,Error]()
		{
			if(!Weak.IsValid()||Weak->bEnding||Weak->ParseGeneration!=Revision) return;
			Weak->bParsing=false;
			if(Parsed.IsValid()) Weak->DataSyncComponent->ReceiveSlabScenario(Parsed); else Weak->ReportFailure(Error);
		});
	}); return true;
}
bool ASlabActor::StartSyntheticScenario() { return SubmitScenarioJson(FSlabScenarioCodec::MakeSyntheticJson(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower))); }
bool ASlabActor::ReceiveScenario(FSlabScenarioDataPtr Data)
{
	check(IsInGameThread()); if(!Data.IsValid()||bEnding||!GetGameInstance()) return false;
	auto* Replay=GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>();
	if(Replay&&!Replay->RegisterValidatedScenario(Data))
	{
		Status.Message=TEXT("이미 보관된 UUID입니다. 자동 재실행하지 않습니다. 재생 목록에서 다시 실행할 수 있습니다.");
		OnSlabStateChanged.Broadcast(); return false;
	}
	const auto* Session=GetWorld()?GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>():nullptr;
	const auto SessionState=Session?Session->GetSlabSensorSessionStatus().State:EVirtualSlabSessionState::Idle;
	const bool bSessionBusy=SessionState==EVirtualSlabSessionState::Ready||SessionState==EVirtualSlabSessionState::Running||SessionState==EVirtualSlabSessionState::Paused||SessionState==EVirtualSlabSessionState::Draining;
	if(IsSimulationActive()||(Replay&&Replay->IsReplayBusy())||bSessionBusy)
	{ Status.Message=TEXT("새 시나리오는 보관했습니다. 현재 재생·송신 정리 후 목록에서 실행하세요."); OnSlabStateChanged.Broadcast(); return false; }
	return StartScenario(Data,FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower),false);
}
bool ASlabActor::StartScenarioPlayback_Implementation(const FString& Json,const FString& ScenarioUUID,const FString& RunUUID)
{
	if(IsSimulationActive()||bEnding||!GetGameInstance()) return false;
	auto* Replay=GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>();
	const auto Data=Replay?Replay->GetValidatedScenario(ScenarioUUID):FSlabScenarioDataPtr();
	if(!Data.IsValid()||Data->OriginalJson!=Json) { ReportFailure(TEXT("검증된 원본 시나리오를 찾을 수 없습니다.")); return false; }
	return StartScenario(Data,RunUUID,true);
}
void ASlabActor::StopScenarioPlayback_Implementation(const FString& RunUUID)
{ if(Status.RunUUID==RunUUID&&Status.bReplay) StopSimulation(); }
bool ASlabActor::StartScenario(FSlabScenarioDataPtr Data,const FString& RunUUID,bool bReplay)
{
	if(!Data.IsValid()||Data->Rows.IsEmpty()||!GetWorld()||!GetGameInstance()) return false;
	auto* Session=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
	auto* Replay=GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>();
	if(!Session||!Replay) return false;
	const auto& First=Data->Rows[0];
	const FVector Size(FSlabScenarioCodec::ToCm(First.Length,DimensionUnit),FSlabScenarioCodec::ToCm(First.Width,DimensionUnit),FSlabScenarioCodec::ToCm(First.Thickness,DimensionUnit));
	if(Size.ContainsNaN()||Size.GetMin()<=0||Size.GetMax()>1000000) { ReportFailure(TEXT("변환된 Slab 치수는 0보다 크고 10km 이하여야 합니다.")); return false; }
	if(!bReplay)
	{
		if(!Replay->SetLiveScenarioPlaybackActive(true,Data->ScenarioUUID)) { ReportFailure(TEXT("다른 Slab 재생이 진행 중입니다.")); return false; }
		if(Session->BeginScenarioSensorSession(RunUUID,TargetSensorIds,Data->ScenarioUUID,SensorOutputs).IsEmpty())
		{ Replay->SetLiveScenarioPlaybackActive(false,FString()); ReportFailure(Session->GetSlabSensorSessionStatus().Message); return false; }
	}
	Scenario=Data; RunSizeCm=Size; RunPositionUnit=PositionUnit; RunTrackTransform=GetTrackTransform();
	Elapsed=0; LastNotifiedIndex=INDEX_NONE; Status=FSlabSimulationStatus();
	Status.State=ESlabSimulationState::Playing; Status.ScenarioUUID=Data->ScenarioUUID; Status.RunUUID=RunUUID; Status.MtlNo=First.MtlNo;
	Status.RowCount=Data->Rows.Num(); Status.DurationSec=Data->DurationSec; Status.bReplay=bReplay;
	Status.Message=Data->bDimensionsChanged?TEXT("재생 중 · 첫 행의 치수를 고정 적용합니다."):TEXT("재생 중 · 위치와 각도를 시간 기준으로 보간합니다.");
	SlabMesh->SetRelativeScale3D(RunSizeCm/100); VisualizationComponent->UpdateGeometry(RunSizeCm);
	if(bReplay&&!Replay->NotifyPlaybackStarted(RunUUID)) { Status.State=ESlabSimulationState::Failed; return false; }
	ApplyAtTime(0); if(Status.State!=ESlabSimulationState::Playing) return false;
	MotionComponent->SetComponentTickEnabled(true); OnSlabStateChanged.Broadcast(); return true;
}
FTransform ASlabActor::GetTrackTransform() const
{
	FTransform Result=TrackReference?TrackReference->GetActorTransform():InitialTrackTransform;
	Result.SetScale3D(FVector::OneVector); return Result;
}
FVector ASlabActor::GetSizeCm() const
{
	if(IsSimulationActive()) return RunSizeCm;
	const FVector Raw=Scenario.IsValid()&&!Scenario->Rows.IsEmpty()?FVector(Scenario->Rows[0].Length,Scenario->Rows[0].Width,Scenario->Rows[0].Thickness):InitialDimensions;
	return FVector(FSlabScenarioCodec::ToCm(Raw.X,DimensionUnit),FSlabScenarioCodec::ToCm(Raw.Y,DimensionUnit),FSlabScenarioCodec::ToCm(Raw.Z,DimensionUnit));
}
FTransform ASlabActor::MakePose(const FSlabScenarioRow& Row) const
{
	const FTransform Track=IsSimulationActive()?RunTrackTransform:GetTrackTransform();
	const ESlabInputUnit Units=IsSimulationActive()?RunPositionUnit:PositionUnit;
	const FVector Size=GetSizeCm();
	return FTransform(Track.GetRotation()*FRotator(0,Row.LeftAngle,0).Quaternion(),Track.TransformPositionNoScale(FVector(FSlabScenarioCodec::ToCm(Row.CenterX,Units),0,Size.Z/2)),FVector::OneVector);
}
FSlabMetrics ASlabActor::CalculateMetricsForRow(const FSlabScenarioRow& Row) const
{
	const FTransform Track=IsSimulationActive()?RunTrackTransform:GetTrackTransform();
	return USlabMetricsComponent::Calculate(Row,MakePose(Row),GetSizeCm(),Track,TrackReference?TrackReference->LeftRailYcm:0,
		TrackReference?TrackReference->RightRailYcm:0,TrackReference&&TrackReference->HasValidRails());
}
FSlabMetrics ASlabActor::GetCurrentMetrics() const { return MetricsComponent->GetMetrics(); }
uint32 ASlabActor::GetMetricsConfigurationHash() const
{
	uint32 Hash=HashCombine(GetTypeHash(static_cast<uint8>(DimensionUnit)),GetTypeHash(static_cast<uint8>(PositionUnit)));
	const FTransform Track=GetTrackTransform(); Hash=HashCombine(Hash,GetTypeHash(Track.GetLocation())); Hash=HashCombine(Hash,GetTypeHash(Track.GetRotation()));
	if(TrackReference) { Hash=HashCombine(Hash,GetTypeHash(TrackReference->LeftRailYcm)); Hash=HashCombine(Hash,GetTypeHash(TrackReference->RightRailYcm)); Hash=HashCombine(Hash,GetTypeHash(TrackReference->bRailsConfigured)); }
	return Hash;
}
void ASlabActor::ApplyAtTime(double Time)
{
	if(!Scenario.IsValid()) return; FSlabScenarioRow Row; int32 Index=0;
	if(!FSlabScenarioCodec::Sample(*Scenario,Time,Row,Index)) return;
	SetActorTransform(MakePose(Row),false,nullptr,ETeleportType::TeleportPhysics);
	Status.ElapsedSec=FMath::Clamp(Time,0.0,Status.DurationSec); Status.Progress=Status.DurationSec>0?Status.ElapsedSec/Status.DurationSec:0;
	Status.FrameNo=Row.FrameNo; Status.RowIndex=Index;
	FSlabMetrics Metrics=CalculateMetricsForRow(Row); Metrics.ElapsedSec=Status.ElapsedSec;
	if(Index+1<Scenario->Rows.Num())
	{
		const auto& A=Scenario->Rows[Index]; const auto& B=Scenario->Rows[Index+1];
		Metrics.SpeedCmPerSec=FSlabScenarioCodec::ToCm((B.CenterX-A.CenterX)/(B.ElapsedSec-A.ElapsedSec),RunPositionUnit);
	}
	MetricsComponent->Update(Metrics);
	if(Index!=LastNotifiedIndex)
	{
		// Notify only after this rendered pose has been applied; never start an extra sensor scan.
		if(auto* Session=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>())
			if(!Session->NotifySlabFrameApplied(Status.RunUUID,Row.MtlNo,Row.FrameNo,Scenario->Rows[Index].ElapsedSec))
			{ FinishSimulation(true,TEXT("Slab 프레임과 센서 세션 연동에 실패했습니다.")); return; }
		LastNotifiedIndex=Index;
	}
}
void ASlabActor::AdvanceSimulation(double DeltaSeconds)
{
	if(Status.State!=ESlabSimulationState::Playing||!FMath::IsFinite(DeltaSeconds)||DeltaSeconds<0) return;
	Elapsed=FMath::Min(Elapsed+DeltaSeconds,Status.DurationSec); ApplyAtTime(Elapsed);
	if(Status.State==ESlabSimulationState::Playing&&Elapsed>=Status.DurationSec) FinishSimulation(false);
}
bool ASlabActor::SetSimulationPaused(bool bPaused)
{
	if(!IsSimulationActive()) return false;
	if((Status.State==ESlabSimulationState::Paused)==bPaused) return true;
	auto* Session=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
	if(Session&&!Session->SetSlabSensorSessionPaused(Status.RunUUID,bPaused)) return false;
	Status.State=bPaused?ESlabSimulationState::Paused:ESlabSimulationState::Playing;
	Status.Message=bPaused?TEXT("일시정지 · 센서 세션의 신규 송신을 보류합니다."):TEXT("재생 중");
	MotionComponent->SetComponentTickEnabled(!bPaused); OnSlabStateChanged.Broadcast(); return true;
}
void ASlabActor::StopSimulation() { ++ParseGeneration; bParsing=false; if(IsSimulationActive()) FinishSimulation(true); }
void ASlabActor::FinishSimulation(bool bAborted,const FString& Error)
{
	if(!IsSimulationActive()) return;
	MotionComponent->SetComponentTickEnabled(false);
	if(auto* Replay=GetGameInstance()?GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>():nullptr)
	{
		if(Status.bReplay) Replay->NotifyPlaybackFinished(Status.RunUUID,bAborted);
		else
		{
			if(auto* Session=GetWorld()?GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>():nullptr) Session->EndSlabSensorSession(Status.RunUUID,bAborted);
			Replay->SetLiveScenarioPlaybackActive(false,FString());
		}
	}
	Status.State=Error.IsEmpty()?ESlabSimulationState::Completed:ESlabSimulationState::Failed;
	Status.Message=!Error.IsEmpty()?Error:bAborted?TEXT("사용자가 재생을 중지했습니다. 승인된 센서 데이터는 정리합니다."):TEXT("시뮬레이션 완료 · 승인된 센서 데이터 정리 중");
	OnSlabStateChanged.Broadcast();
}
bool ASlabActor::SetDimensionUnit(ESlabInputUnit Value)
{ if(IsSimulationActive()) return false; DimensionUnit=Value; UpdateDimensions(); VisualizationComponent->UpdateGeometry(GetSizeCm()); OnSlabStateChanged.Broadcast(); return true; }
bool ASlabActor::SetPositionUnit(ESlabInputUnit Value)
{ if(IsSimulationActive()) return false; PositionUnit=Value; OnSlabStateChanged.Broadcast(); return true; }
bool ASlabActor::SetSensorOutputs(FVirtualSlabSensorOutputSelection Outputs)
{ SensorOutputs=Outputs; OnSlabStateChanged.Broadcast(); return true; }
void ASlabActor::SetHotAppearance(bool bHot) { bHotAppearance=bHot; UpdateAppearance(); OnSlabStateChanged.Broadcast(); }
void ASlabActor::SetDiagnosticHelpersVisible(bool bVisible) { VisualizationComponent->SetHelpersVisible(bVisible); }
void ASlabActor::UpdateDimensions()
{ if(SlabMesh) SlabMesh->SetRelativeScale3D(GetSizeCm().ComponentMax(FVector(0.01))/100); }
void ASlabActor::UpdateAppearance()
{
	if(!SlabMesh) return;
	UMaterialInterface* Material=SurfaceMaterial.LoadSynchronous();
	if(!Material) Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if(!SurfaceInstance||SurfaceInstance->Parent!=Material) { SurfaceInstance=Material?UMaterialInstanceDynamic::Create(Material,this):nullptr; if(SurfaceInstance) SlabMesh->SetMaterial(0,SurfaceInstance); }
	if(SurfaceInstance)
	{
		SurfaceInstance->SetScalarParameterValue(TEXT("Hotness"),bHotAppearance?1:0); SurfaceInstance->SetScalarParameterValue(TEXT("Oxidation"),Oxidation);
		SurfaceInstance->SetScalarParameterValue(TEXT("Roughness"),FMath::IsFinite(Roughness)?FMath::Clamp(Roughness,0.0f,1.0f):0.65f);
		SurfaceInstance->SetScalarParameterValue(TEXT("EmissiveStrength"),FMath::IsFinite(EmissiveStrength)?FMath::Clamp(EmissiveStrength,0.0f,20.0f):3.0f);
		SurfaceInstance->SetVectorParameterValue(TEXT("Color"),bHotAppearance?FLinearColor(1,0.08,0.01):FLinearColor(0.12,0.14,0.16));
	}
}

#include "SlabActor.h"
#include "SlabScenarioCodec.h"
#include "SlabDataSyncComponent.h"
#include "SlabMotionComponent.h"
#include "SlabVisualizationComponent.h"
#include "SlabMetricsComponent.h"
#include "SlabTrackReferenceActor.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorSlabContextSubsystem.h"
#include "Core/DxProcessSubsystem.h"
#include "Core/DTCoreSettings.h"
#include "WebSocket/TransactionCodeStruct.h"
#include "WebSocket/TransactionCodeMessage.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/FactoryAgentScenarioTC.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/UI/SlabSimulationUiHostActor.h"
#include "EngineUtils.h"
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
	SurfaceMaterial=TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurfacePlain.M_SlabSurfacePlain")));
	HighlightMode=EHighlightMode::IndividualMesh; DisplayName=TEXT("시나리오 Slab");
}
void ASlabActor::OnConstruction(const FTransform& Transform)
{ Super::OnConstruction(Transform); UpdateDimensions(); UpdateAppearance(); }
void ASlabActor::BeginPlay()
{
	Super::BeginPlay(); InitialTrackTransform=GetActorTransform(); InitialTrackTransform.SetScale3D(FVector::OneVector);
	MotionComponent->OnPoseApplied.BindUObject(this,&ASlabActor::OnSlabPoseApplied);
	UpdateDimensions(); UpdateAppearance(); VisualizationComponent->ConfigureDisplay(AnalysisDisplaySettings); VisualizationComponent->UpdateGeometry(GetSizeCm());
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
void ASlabActor::RecordAdmission(ESlabScenarioAdmission Result,const FString& ScenarioId,const FString& Reason)
{
	LastAdmission=FSlabScenarioAdmissionStatus();LastAdmission.Result=Result;LastAdmission.ScenarioUUID=ScenarioId;LastAdmission.Reason=Reason;LastAdmission.RequestedOutputs=SensorOutputs;
	if(Result==ESlabScenarioAdmission::Started||Result==ESlabScenarioAdmission::StartedWithoutTransmission)
	{
		LastAdmission.RunUUID=Status.RunUUID;
		if(auto* Session=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>())LastAdmission.AppliedOutputs=Session->GetSlabSensorSessionStatus().Outputs;
	}
	UE_LOG(LogTemp,Display,TEXT("[SlabAdmission] receiver=%s actor=%s scenario=%s result=%d requested=%d/%d/%d applied=%d/%d/%d reason=%s"),
		*ReceiverId,*GetPathName(),*ScenarioId,int32(Result),SensorOutputs.bPointCloud,SensorOutputs.bCameraImage,SensorOutputs.bLidarTelemetry,
		LastAdmission.AppliedOutputs.bPointCloud,LastAdmission.AppliedOutputs.bCameraImage,LastAdmission.AppliedOutputs.bLidarTelemetry,*Reason);
	OnSlabStateChanged.Broadcast();
}
bool ASlabActor::ReceiveScenario(FSlabScenarioDataPtr Data)
{
	check(IsInGameThread()); if(!Data.IsValid()||bEnding||!GetGameInstance()) {RecordAdmission(ESlabScenarioAdmission::Rejected,FString(),TEXT("유효한 시나리오와 실행 중인 GameInstance가 필요합니다."));return false;}
	auto* Replay=GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>();
	const bool Duplicate=Replay&&Replay->GetValidatedScenario(Data->ScenarioUUID).IsValid();
	if(Replay&&!Replay->RegisterValidatedScenario(Data))
	{
		Status.Message=Duplicate?TEXT("이미 보관된 UUID입니다. 자동 재실행하지 않습니다. 수신 경로에서 보관 API를 중복 호출하지 마세요."):Replay->GetRegistrationMessage();
		RecordAdmission(Duplicate?ESlabScenarioAdmission::Duplicate:ESlabScenarioAdmission::Rejected,Data->ScenarioUUID,Status.Message);return false;
	}
	const auto* Session=GetWorld()?GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>():nullptr;
	const auto SessionState=Session?Session->GetSlabSensorSessionStatus().State:EVirtualSlabSessionState::Idle;
	const bool bSessionBusy=SessionState==EVirtualSlabSessionState::Ready||SessionState==EVirtualSlabSessionState::Running||SessionState==EVirtualSlabSessionState::Paused||SessionState==EVirtualSlabSessionState::Draining;
	if(IsSimulationActive()||(Replay&&Replay->IsReplayBusy())||bSessionBusy)
	{ Status.Message=TEXT("새 시나리오는 보관했습니다. 현재 재생·송신 정리 후 목록에서 실행하세요.");RecordAdmission(ESlabScenarioAdmission::StoredOnlyBusy,Data->ScenarioUUID,Status.Message);return false; }
	const bool Started=StartScenario(Data,FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower),false);
	RecordAdmission(Started?(Status.TransmissionWarning.IsEmpty()?ESlabScenarioAdmission::Started:ESlabScenarioAdmission::StartedWithoutTransmission):ESlabScenarioAdmission::Rejected,
		Data->ScenarioUUID,Started&&!Status.TransmissionWarning.IsEmpty()?Status.TransmissionWarning:Status.Message);
	return Started;
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
	if(!Session||!Replay) {ReportFailure(TEXT("Slab 실행 Subsystem이 준비되지 않았습니다."));return false;}
	const auto& First=Data->Rows[0];
	const FVector Size(FSlabScenarioCodec::ToCm(First.Length,DimensionUnit),FSlabScenarioCodec::ToCm(First.Width,DimensionUnit),FSlabScenarioCodec::ToCm(First.Thickness,DimensionUnit));
	if(Size.ContainsNaN()||Size.GetMin()<=0||Size.GetMax()>1000000) { ReportFailure(TEXT("변환된 Slab 치수는 0보다 크고 10km 이하여야 합니다.")); return false; }
	FString TransmissionWarning;
	if(!bReplay)
	{
		if(!Replay->SetLiveScenarioPlaybackActive(true,Data->ScenarioUUID)) { ReportFailure(TEXT("다른 Slab 재생이 진행 중입니다.")); return false; }
		const bool Ready=Session->ValidateScenarioOutputs(TargetSensorIds,SensorOutputs,TransmissionWarning);
		const FString SessionId=Ready?Session->BeginScenarioSensorSession(RunUUID,TargetSensorIds,Data->ScenarioUUID,SensorOutputs)
			:Session->BeginUnboundObservationSession(RunUUID,Data->ScenarioUUID);
		if(SessionId.IsEmpty())
		{ Replay->SetLiveScenarioPlaybackActive(false,FString()); ReportFailure(Session->GetSlabSensorSessionStatus().Message); return false; }
	}
	Scenario=Data; LastNotifiedIndex=INDEX_NONE; Status=FSlabSimulationStatus();
	Status.State=ESlabSimulationState::Playing; Status.ScenarioUUID=Data->ScenarioUUID; Status.RunUUID=RunUUID; Status.MtlNo=First.MtlNo;
	Status.RowCount=Data->Rows.Num(); Status.DurationSec=Data->DurationSec; Status.bReplay=bReplay;
	Status.TransmissionWarning=TransmissionWarning.IsEmpty()?FString():TEXT("선택 출력 미송신 · ")+TransmissionWarning+TEXT(" 이번 실행은 관찰만 진행합니다.");
	Status.Message=Data->bDimensionsChanged?TEXT("재생 중 · 첫 행의 치수를 고정 적용합니다."):TEXT("재생 중 · 위치와 각도를 시간 기준으로 보간합니다.");
	SlabMesh->SetRelativeScale3D(Size/100); VisualizationComponent->UpdateGeometry(Size);
	if(bReplay&&!Replay->NotifyPlaybackStarted(RunUUID)) { Status.State=ESlabSimulationState::Failed; return false; }
	if(!MotionComponent->StartPlayback(Data,GetTrackTransform(),Size,PositionUnit))
	{ if(IsSimulationActive()) FinishSimulation(true,TEXT("Slab 이동 컴포넌트를 시작하지 못했습니다.")); return false; }
	if(Status.State!=ESlabSimulationState::Playing) return false;
	OnSlabStateChanged.Broadcast(); return true;
}
FTransform ASlabActor::GetTrackTransform() const
{
	FTransform Result=TrackReference?TrackReference->GetActorTransform():InitialTrackTransform;
	Result.SetScale3D(FVector::OneVector); return Result;
}
FVector ASlabActor::GetSizeCm() const
{
	if(IsSimulationActive()&&MotionComponent->IsPlaybackRunning()) return MotionComponent->GetSlabSizeCm();
	const FVector Raw=Scenario.IsValid()&&!Scenario->Rows.IsEmpty()?FVector(Scenario->Rows[0].Length,Scenario->Rows[0].Width,Scenario->Rows[0].Thickness):InitialDimensions;
	return FVector(FSlabScenarioCodec::ToCm(Raw.X,DimensionUnit),FSlabScenarioCodec::ToCm(Raw.Y,DimensionUnit),FSlabScenarioCodec::ToCm(Raw.Z,DimensionUnit));
}
FTransform ASlabActor::MakePose(const FSlabScenarioRow& Row) const
{
	const bool bPlayback=IsSimulationActive()&&MotionComponent->IsPlaybackRunning();
	return USlabMotionComponent::BuildPose(Row,bPlayback?MotionComponent->GetPlaybackTrack():GetTrackTransform(),GetSizeCm(),
		bPlayback?MotionComponent->GetPlaybackPositionUnit():PositionUnit);
}
FSlabMetrics ASlabActor::CalculateMetricsForRow(const FSlabScenarioRow& Row) const
{
	const FTransform Track=IsSimulationActive()&&MotionComponent->IsPlaybackRunning()?MotionComponent->GetPlaybackTrack():GetTrackTransform();
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
void ASlabActor::OnSlabPoseApplied(const FSlabScenarioRow& Row,int32 Index,double Time,bool bEnd)
{
	if(!IsSimulationActive()||!Scenario.IsValid()||!Scenario->Rows.IsValidIndex(Index)) return;
	Status.ElapsedSec=FMath::Clamp(Time,0.0,Status.DurationSec); Status.Progress=Status.DurationSec>0?Status.ElapsedSec/Status.DurationSec:0;
	Status.FrameNo=Row.FrameNo; Status.RowIndex=Index;
	FSlabMetrics Metrics=CalculateMetricsForRow(Row); Metrics.ElapsedSec=Status.ElapsedSec;
	if(Index+1<Scenario->Rows.Num())
	{
		const auto& A=Scenario->Rows[Index]; const auto& B=Scenario->Rows[Index+1];
		Metrics.SpeedCmPerSec=FSlabScenarioCodec::ToCm((B.CenterX-A.CenterX)/(B.ElapsedSec-A.ElapsedSec),MotionComponent->GetPlaybackPositionUnit());
	}
	MetricsComponent->Update(Metrics);
	VisualizationComponent->UpdateAnalysis(Row,Metrics,Status,GetSizeCm(),MotionComponent->GetPlaybackTrack(),
		TrackReference?TrackReference->LeftRailYcm:0,TrackReference?TrackReference->RightRailYcm:0,TrackReference&&TrackReference->HasValidRails(),TrackReference?TrackReference->RailLengthCm:0);
	if(Index!=LastNotifiedIndex)
	{
		// Notify only after this rendered pose has been applied; never start an extra sensor scan.
		if(auto* Session=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>())
			if(!Session->NotifySlabFrameApplied(Status.RunUUID,Row.MtlNo,Row.FrameNo,Scenario->Rows[Index].ElapsedSec))
			{ FinishSimulation(true,TEXT("Slab 프레임과 센서 세션 연동에 실패했습니다.")); return; }
		LastNotifiedIndex=Index;
	}
	if(bEnd&&Status.State==ESlabSimulationState::Playing) FinishSimulation(false);
}
void ASlabActor::AdvanceSimulation(double DeltaSeconds)
{
	MotionComponent->AdvancePlayback(DeltaSeconds);
}
bool ASlabActor::SetSimulationPaused(bool bPaused)
{
	if(!IsSimulationActive()) return false;
	if((Status.State==ESlabSimulationState::Paused)==bPaused) return true;
	auto* Session=GetWorld()->GetSubsystem<UVirtualSensorSlabContextSubsystem>();
	if(Session&&!Session->SetSlabSensorSessionPaused(Status.RunUUID,bPaused)) return false;
	Status.State=bPaused?ESlabSimulationState::Paused:ESlabSimulationState::Playing;
	Status.Message=bPaused?TEXT("일시정지 · 센서 세션의 신규 송신을 보류합니다."):TEXT("재생 중");
	MotionComponent->SetPlaybackPaused(bPaused); OnSlabStateChanged.Broadcast(); return true;
}
void ASlabActor::StopSimulation() { ++ParseGeneration; bParsing=false; if(IsSimulationActive()) FinishSimulation(true); }
void ASlabActor::FinishSimulation(bool bAborted,const FString& Error)
{
	if(!IsSimulationActive()) return;
	MotionComponent->StopPlayback();
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
bool ASlabActor::GetDiagnosticHelpersVisible() const { return VisualizationComponent&&VisualizationComponent->GetHelpersVisible(); }
void ASlabActor::SetAnalysisDisplaySettings(const FSlabAnalysisDisplaySettings& Settings)
{ AnalysisDisplaySettings=Settings; VisualizationComponent->ConfigureDisplay(Settings); OnSlabStateChanged.Broadcast(); }
FSoftObjectPath ASlabActor::ResolveSurfaceMaterialPath(const FSoftObjectPath& Configured)
{
	if(Configured.IsNull()||Configured.GetLongPackageName()==TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurface"))
		return FSoftObjectPath(TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurfacePlain.M_SlabSurfacePlain"));
	return Configured;
}
UMaterialInterface* ASlabActor::GetExplicitSlotSurfaceMaterial() const
{
	const FSoftObjectPath Configured=SurfaceMaterial.ToSoftObjectPath();
	const FString Package=Configured.GetLongPackageName();
	const bool bDefaultProperty=Configured.IsNull()||Package==TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurface")||Package==TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurfacePlain");
	if(!bDefaultProperty||!SlabMesh||SlabMesh->GetNumOverrideMaterials()<1) return nullptr;
	UMaterialInterface* Slot=SlabMesh->GetMaterial(0);
	if(!Slot||Slot==SurfaceInstance) return nullptr;
	if(Slot->IsA<UMaterialInstanceDynamic>()&&Slot->IsIn(this)&&Slot->GetName().StartsWith(TEXT("SlabOwnedSurface"))) return nullptr;
	if(const auto* Legacy=Cast<UMaterialInstanceDynamic>(Slot))
	{
		// Older construction-created MIDs could be serialized in component overrides while
		// the transient SurfaceInstance pointer was not. Recognize only stock auto-named MIDs.
		const FString ParentPackage=Legacy->Parent?FSoftObjectPath(Legacy->Parent.Get()).GetLongPackageName():FString();
		if(Legacy->IsIn(this)&&Legacy->GetName().StartsWith(TEXT("MaterialInstanceDynamic"))&&
			(ParentPackage==TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurface")||ParentPackage==TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurfacePlain"))) return nullptr;
	}
	const FString SlotPackage=FSoftObjectPath(Slot).GetLongPackageName();
	if(SlotPackage==TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurface")||SlotPackage==TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurfacePlain")) return nullptr;
	return Slot;
}
FString ASlabActor::GetEffectiveSurfaceMaterialPath() const
{
	if(UMaterialInterface* Slot=GetExplicitSlotSurfaceMaterial()) return Slot->GetPathName();
	return ResolveSurfaceMaterialPath(SurfaceMaterial.ToSoftObjectPath()).ToString();
}
void ASlabActor::UpdateDimensions()
{ if(SlabMesh) SlabMesh->SetRelativeScale3D(GetSizeCm().ComponentMax(FVector(0.01))/100); }
void ASlabActor::UpdateAppearance()
{
	if(!SlabMesh) return;
	// A slot-level user assignment remains untouched, including its material-instance parameters.
	// A non-default Actor SurfaceMaterial is an explicit higher-priority override.
	if(GetExplicitSlotSurfaceMaterial()) return;
	UMaterialInterface* Material=Cast<UMaterialInterface>(ResolveSurfaceMaterialPath(SurfaceMaterial.ToSoftObjectPath()).TryLoad());
	if(!Material) Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if(!SurfaceInstance||SurfaceInstance->Parent!=Material)
		SurfaceInstance=Material?UMaterialInstanceDynamic::Create(Material,this,MakeUniqueObjectName(this,UMaterialInstanceDynamic::StaticClass(),TEXT("SlabOwnedSurface"))):nullptr;
	if(SurfaceInstance&&SlabMesh->GetMaterial(0)!=SurfaceInstance) SlabMesh->SetMaterial(0,SurfaceInstance);
	if(SurfaceInstance)
	{
		SurfaceInstance->SetScalarParameterValue(TEXT("Hotness"),bHotAppearance?1:0); SurfaceInstance->SetScalarParameterValue(TEXT("Oxidation"),Oxidation);
		SurfaceInstance->SetScalarParameterValue(TEXT("Roughness"),FMath::IsFinite(Roughness)?FMath::Clamp(Roughness,0.0f,1.0f):0.65f);
		SurfaceInstance->SetScalarParameterValue(TEXT("EmissiveStrength"),FMath::IsFinite(EmissiveStrength)?FMath::Clamp(EmissiveStrength,0.0f,20.0f):3.0f);
		SurfaceInstance->SetVectorParameterValue(TEXT("Color"),bHotAppearance?FLinearColor(1,0.08,0.01):FLinearColor(0.12,0.14,0.16));
	}
}
FSlabSetupValidation ASlabActor::ValidateSlabSetup() const
{
	FSlabSetupValidation Result;
	if(!GetWorld()) { Result.Errors.Add(TEXT("Slab의 월드가 없습니다.")); return Result; }
	if(ReceiverId.TrimStartAndEnd().IsEmpty()) Result.Errors.Add(TEXT("ReceiverId가 비어 있습니다."));
	int32 SameReceiver=0,SlabCount=0; for(TActorIterator<ASlabActor> It(GetWorld());It;++It) { ++SlabCount; if(It->ReceiverId==ReceiverId) ++SameReceiver; }
	if(SameReceiver>1) Result.Errors.Add(TEXT("동일 ReceiverId의 Slab가 여러 개 있습니다."));
	if(SlabCount>1) Result.Warnings.Add(TEXT("한 월드의 재생 adapter는 하나입니다. 사용할 Slab와 UI를 명시적으로 연결하세요."));
	if(GetGameInstance()) if(auto* Replay=GetGameInstance()->GetSubsystem<USlabScenarioReplaySubsystem>())
		if(Replay->GetPlaybackAdapter()!=this) Result.Warnings.Add(TEXT("현재 재생 adapter가 이 Slab가 아닙니다. 재생 대상 연결을 확인하세요."));
	const FVector SizeCm=GetSizeCm();
	if(SizeCm.ContainsNaN()||SizeCm.GetMin()<=0||SizeCm.GetMax()>1000000) Result.Errors.Add(TEXT("현재 단위로 변환한 Slab 치수가 유효하지 않습니다."));
	if(SizeCm.GetMax()>5000) Result.Warnings.Add(TEXT("Slab 크기가 50m를 초과합니다. 원본 치수가 cm인지 mm인지 확인하세요."));
	if(TrackReference&&TrackReference->GetWorld()!=GetWorld()) Result.Errors.Add(TEXT("TrackReference가 다른 월드에 속합니다."));
	else if(!TrackReference) Result.Warnings.Add(TEXT("TrackReference 미지정: Slab 초기 위치·회전이 원점이고 가드레일 Margin은 N/A입니다."));
	else
	{
		if(!TrackReference->HasValidRails()) Result.Warnings.Add(TEXT("좌우 가드레일 내부면이 설정되지 않아 Margin은 N/A입니다."));
		if(!TrackReference->GetActorScale3D().Equals(FVector::OneVector)) Result.Warnings.Add(TEXT("TrackReference의 Scale은 무시됩니다. 원점·회전과 cm 길이를 직접 설정하세요."));
	}
	const auto* Settings=GetDefault<UDTCoreSettings>(); bool bRoute=false;
	if(Settings)
	{
		if(UDataTable* Table=Settings->WebSocketDataTable.LoadSynchronous())
			for(const auto& Pair:Table->GetRowMap())
			{
				if(Table->GetRowStruct()!=FTransactionCodeStruct::StaticStruct()) break;
				const auto* Row=reinterpret_cast<const FTransactionCodeStruct*>(Pair.Value);
				const auto* Handler=Row->TransactionCodeMessageClass?Row->TransactionCodeMessageClass.GetDefaultObject():nullptr;
				if(Handler&&Handler->TransactionCode==TEXT("IFactory-agent"))
				{
					const auto* SlabHandler=Cast<UFactoryAgentScenarioTC>(Handler);
					bRoute=SlabHandler&&SlabHandler->ReceiverId==ReceiverId;
					if(!bRoute) Result.Warnings.Add(TEXT("IFactory-agent 담당 TC 또는 ReceiverId가 현재 Slab와 다릅니다. 다른 구현을 자동 교체하지 않습니다."));
				}
			}
		if(Settings->WebSocketTopics.IsEmpty()) Result.Warnings.Add(TEXT("DTCore WebSocket Topic이 비어 있습니다."));
	}
	if(!bRoute) Result.Warnings.Add(TEXT("DTCore IFactory-agent 수신 경로를 확인하세요. 직접 JSON 주입·보관 재생은 별도 경로입니다."));
	bool bUi=false; for(TActorIterator<ASlabSimulationUiHostActor> It(GetWorld());It;++It) if(It->SlabActor==this||(!It->SlabActor&&SlabCount==1)) bUi=true;
	if(!bUi) Result.Warnings.Add(TEXT("연결된 Slab UI Host가 없습니다. 도구 막대에서 열거나 SlabActor를 명시적으로 연결하세요."));
	Result.bCanSimulate=Result.Errors.IsEmpty();
	TArray<AVirtualSensorCoordinator*> Coordinators; for(TActorIterator<AVirtualSensorCoordinator> It(GetWorld());It;++It) Coordinators.Add(*It);
	Result.bCanSendSelectedOutputs=!SensorOutputs.HasAnyOutput();
	if(SensorOutputs.HasAnyOutput())
	{
		if(Coordinators.Num()!=1) Result.Errors.Add(TEXT("선택된 Topic 송신에는 Coordinator가 정확히 하나 필요합니다."));
		else
		{
			auto* Coordinator=Coordinators[0]; auto* Transport=Coordinator->SharedTransportComponent.Get(); bool bCamera=false,bLidar=false;
			TSet<FString> Found; for(auto* Sensor:Coordinator->GetSensorActors()) if(IsValid(Sensor)&&(TargetSensorIds.IsEmpty()||TargetSensorIds.Contains(Sensor->GetSensorId())))
			{ Found.Add(Sensor->GetSensorId()); bCamera|=Sensor->GetSensorKind()==EVirtualSensorKind::Camera; bLidar|=Sensor->GetSensorKind()==EVirtualSensorKind::Lidar; }
			bool bIds=true; for(const auto& Id:TargetSensorIds) bIds&=Found.Contains(Id);
			const bool bTransport=Transport&&Transport->TransportMode==EVirtualSensorTransportMode::StompWebSocket&&!Transport->GetTransportProfile().BrokerUrl.IsEmpty();
			Result.bCanSendSelectedOutputs=bTransport&&bIds&&(!SensorOutputs.bCameraImage||bCamera)&&(!(SensorOutputs.bPointCloud||SensorOutputs.bLidarTelemetry)||bLidar);
			if(!Result.bCanSendSelectedOutputs) Result.Errors.Add(TEXT("송신 설정 확인: STOMP profile, 대상 SensorId와 선택 출력에 맞는 Camera/LiDAR가 필요합니다."));
		}
	}
	return Result;
}

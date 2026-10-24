#include "CoilLoadingSimulation.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorIdentity.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/TextureCube.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Async/Async.h"
#include "EngineUtils.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarStaticCollisionQuery.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorFileSaveSubsystem.h"

using namespace CoilLoading;
namespace
{
bool SameScene(const FSettings& A,const FSettings& B)
{return A.LOA==B.LOA&&A.Beam==B.Beam&&A.Depth==B.Depth&&A.Draft==B.Draft&&A.KeelToTanktop==B.KeelToTanktop&&A.HoldLength==B.HoldLength&&A.HoldWidth==B.HoldWidth&&A.HoldHeight==B.HoldHeight&&A.HoldCount==B.HoldCount&&A.Coaming==B.Coaming&&A.BerthGap==B.BerthGap&&A.RailSetback==B.RailSetback&&A.RailGauge==B.RailGauge&&A.QuaySide==B.QuaySide&&A.IntermediateHoldLength==B.IntermediateHoldLength&&A.HoldGap==B.HoldGap&&A.bDeckCranes==B.bDeckCranes&&A.bFoldingCovers==B.bFoldingCovers;}
void ContactFriction(ACoilLoadingBody* B,const FPlacement& P,bool Wet)
{
    const bool UnknownPackage=P.Spec.Type==ECoilType::ColdRolled&&!P.Spec.bPackagedFrictionConfirmed;
    const double Mu=Wet||UnknownPackage?0:(P.Tier>1||!P.bDunnage?P.Spec.SteelMu:P.Spec.Mu);
    B->ContactMaterial->Friction=Mu;B->ContactMaterial->StaticFriction=Mu;B->Body->SetPhysMaterialOverride(B->ContactMaterial);
}
}
ACoilLoadingGameMode::ACoilLoadingGameMode(){DefaultPawnClass=nullptr;HUDClass=nullptr;}
ACoilLoadingSimulation::ACoilLoadingSimulation()
{
    PrimaryActorTick.bCanEverTick=true;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("CoilSceneRoot")));
    ShoreRoot=CreateDefaultSubobject<USceneComponent>(TEXT("CoilShoreRoot"));ShoreRoot->SetupAttachment(GetRootComponent());ShoreRoot->ComponentTags.Add(TEXT("CoilShoreMotionRoot"));
    PortalRoot=CreateDefaultSubobject<USceneComponent>(TEXT("CoilPortalRoot"));PortalRoot->SetupAttachment(ShoreRoot);PortalRoot->ComponentTags.Add(TEXT("CoilPortalMotionRoot"));
    RigLines=CreateDefaultSubobject<ULineBatchComponent>(TEXT("CoilRigDisplay"));RigLines->SetupAttachment(GetRootComponent());RigLines->SetCollisionEnabled(ECollisionEnabled::NoCollision);RigLines->SetCastShadow(false);
}
void ACoilLoadingSimulation::BeginPlay()
{
    Super::BeginPlay();BuildScene();
    Ambient=NewObject<USkyLightComponent>(this);Ambient->SetupAttachment(GetRootComponent());Ambient->SetMobility(EComponentMobility::Movable);Ambient->SourceType=SLS_SpecifiedCubemap;
    Ambient->Cubemap=LoadObject<UTextureCube>(nullptr,TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"));Ambient->bRealTimeCapture=false;Ambient->bLowerHemisphereIsBlack=false;Ambient->SetIntensity(1.5);Ambient->CastShadows=false;Ambient->RegisterComponent();
    Sky=NewObject<USkyAtmosphereComponent>(this);Sky->SetupAttachment(GetRootComponent());Sky->RegisterComponent();
    ConfigureLidar();RefreshPreview();GetWorldTimerManager().SetTimer(PresentationTimer,this,&ACoilLoadingSimulation::Observe,.2f,true);SetActorTickEnabled(false);
}
void ACoilLoadingSimulation::ConfigureLidar()
{
    Lidar=GetWorld()->SpawnActorDeferred<AVirtualLidarSensorActor>(AVirtualLidarSensorActor::StaticClass(),GetActorTransform(),this);
    if(!Lidar)return;Lidar->SetFlags(RF_Transient);Lidar->ScanComponent->bAutoStartScan=false;Lidar->FinishSpawning(GetActorTransform());
    FVirtualSensorEditableState S,Applied;FString E;Lidar->ReadEditableState(S);S.LidarProfile=EVirtualLidarDeviceProfile::HesaiXT32;S.SimulationQuality=EVirtualSensorSimulationQuality::FullSpec;
    if(!Lidar->ApplyProfileAndSimulationQuality(S,Applied,E)){Status=E;return;}Lidar->ReadEditableState(S);
    S.SensorId=VirtualSensorIdentity::CoilQuay;S.ActorTransform=FTransform(GetActorQuat()*FRotator(-72,112,0).Quaternion(),GetActorTransform().TransformPosition(FVector(-Settings.HoldLength*.5+4,-Settings.HoldWidth*.5+3,Settings.HoldHeight+8)*100));
    S.LidarScanInterval=.05f;S.bLidarMaterialResponse=true;S.PreviewPointStride=2;S.MaxPreviewPoints=12000;S.bExportPcdOnScan=false;S.CompatibilityOutputPolicy=EVirtualLidarCompatibilityOutputPolicy::WhenRequested;
    if(!Lidar->ApplyEditableState(S,E)){Status=E;return;}
    FVirtualLidarSemanticClassRule Rule;Rule.Label=TEXT("Coil");Rule.ActorTags={TEXT("Coil")};Rule.DisplayColor=FLinearColor(.9,.55,.1);Lidar->ScanComponent->SemanticClassRules.Insert(Rule,0);
    const TWeakObjectPtr<ACoilLoadingSimulation> Weak(this);
    Lidar->ScanComponent->SetRayQueryFactory([Weak](AActor* Ignore,ECollisionChannel Channel){auto* Self=Weak.Get();return Self?Self->CaptureQuery(Ignore,Channel):nullptr;});
    Lidar->VisualizationComponent->SetProjectionPreviewEnabled(false);Lidar->VisualizationComponent->SetWorldPointCloudEnabled(false);Lidar->SetLidarAcquisitionBackend(EVirtualLidarAcquisitionBackend::Auto,E);Lidar->StartSensor();
}
TSharedPtr<const IVirtualSensorRayQuery,ESPMode::ThreadSafe> ACoilLoadingSimulation::CaptureQuery(AActor* Ignore,ECollisionChannel Channel)
{
    if(CachedGeometryRevision==GeometryRevision&&CachedQuery)return CachedQuery;
    FString E;CachedQuery=CaptureLidarStaticCollision(GetWorld(),Ignore,Channel,E);QueryDiagnostic=E;
    if(!CachedQuery)for(TActorIterator<AActor> It(GetWorld());It;++It)
    {TInlineComponentArray<UPrimitiveComponent*> Cs(*It);for(auto* C:Cs)if(E.EndsWith(GetNameSafe(C)))QueryDiagnostic+=TEXT(" / ")+C->GetClass()->GetName();}
    CachedGeometryRevision=GeometryRevision;return CachedQuery;
}
FPlacement ACoilLoadingSimulation::PreviewPlacement() const
{
    FPlacement P;P.Spec=NextSpec;P.Tier=NextTier;P.Row=NextRow;P.Column=NextColumn;
    P.bDunnage=bNextDunnage;P.bChocks=bNextChocks;P.bLashing=bNextLashing;P.bEyeForeAft=bNextForeAft;
    P.ChockCapacityKN=NextChockKN;P.LashingCapacityKN=NextLashingKN;P.DiametralStiffnessKNPerM=NextStiffness;P.OvalityLimit=NextOvalityLimit;P.RollingResistanceM=NextRollingResistanceM;
    const double OD=P.Spec.OuterDiameter(),Width=P.Spec.OverallWidth();
    const double Direction=bRowsForeToAft?-1.:1.,HalfX=(P.bEyeForeAft?Width:OD)*.5;
    P.Center=FVector(-Direction*(Settings.HoldLength*.5-FMath::Max(HalfX,Settings.GearLength*.5)-Settings.ClearanceMargin)+Direction*(NextRow-1)*(2*HalfX+.15),-Settings.HoldWidth*.5+FMath::Max(OD*.5,Settings.GearWidth*.5)+Settings.ClearanceMargin+(NextColumn-1+(NextTier-1)*.5)*OD,(P.bDunnage?Settings.DunnageThickness:0)+OD*.5+(NextTier-1)*OD*.8660254038);
    int32 PreviousRow=0;for(const auto& Existing:Placements)if(Existing.Row<NextRow)PreviousRow=FMath::Max(PreviousRow,Existing.Row);
    if(PreviousRow>0)
    {
        double Edge=Direction<0?DBL_MAX:-DBL_MAX;
        for(const auto& Existing:Placements)if(Existing.Row==PreviousRow)
        {const double Half=(Existing.bEyeForeAft?Existing.Spec.OverallWidth():Existing.Spec.OuterDiameter())*.5;Edge=Direction<0?FMath::Min(Edge,Existing.Center.X-Half):FMath::Max(Edge,Existing.Center.X+Half);}
        P.Center.X=Edge+Direction*(HalfX+.15+(NextRow-PreviousRow-1)*(2*HalfX+.15));
    }
    for(const auto& Existing:Placements)if(Existing.Row==NextRow)
    {P.Center.X=Existing.Center.X;if(NextTier==1&&Existing.Tier==1&&Existing.Column==NextColumn-1)P.Center.Y=Existing.Center.Y+(Existing.Spec.OuterDiameter()+OD)*.5;}
    if(NextTier>1)
    {
        for(int32 I=0;I<Placements.Num();++I)if(Placements[I].Tier==NextTier-1&&Placements[I].Row==NextRow&&Placements[I].Column==NextColumn)
        for(int32 J=0;J<Placements.Num();++J)if(Placements[J].Tier==NextTier-1&&Placements[J].Row==NextRow&&Placements[J].Column==NextColumn+1)
        {
            const auto& A=Placements[I];const auto& B=Placements[J];FVector2D C;
            if(!UnequalKey({A.Center.Y,A.Center.Z},A.Spec.OuterDiameter()*.5,{B.Center.Y,B.Center.Z},B.Spec.OuterDiameter()*.5,OD*.5,C))continue;
            P.SupportA=I;P.SupportB=J;P.Center.Y=C.X;P.Center.Z=C.Y;
        }
    }
    return P;
}
void ACoilLoadingSimulation::RefreshPreview()
{
    if(bEnding||Materials.IsEmpty())return;FString E;if(!NextSpec.Validate(E)||!Settings.Validate(E)){Status=E;return;}
    if(!SameScene(Settings,BuiltSceneSettings))RestoreSceneGeometry();
    UpdateHydrostaticPresentation();
    Pending=PreviewPlacement();auto Preview=Placements;Preview.Add(Pending);PreviewAnalysis=Analyze(Settings,Preview);
    if(!Ghost){Ghost=SpawnCoil(NextSpec,Pending.Center);if(Ghost){Ghost->Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);Ghost->Tags={TEXT("CoilGhostDisplayOnly")};Ghost->Body->SetRenderCustomDepth(true);}}
    if(Ghost){Ghost->Body->SetStaticMesh(CoilMesh(NextSpec));ApplyCoilAppearance(Ghost,NextSpec);Ghost->Body->SetMaterial(0,Materials[PreviewAnalysis.Severity==ESeverity::Warning?6:5]);Ghost->SetActorLocation(GetActorTransform().TransformPosition(Pending.Center*100));Ghost->SetActorRotation(GetActorRotation()+FRotator(0,bNextForeAft?0:90,0));}
    if(CurrentPhase==ECoilLoadingPhase::Idle)
    {
        if(!WaitingCoil){WaitingCoil=SpawnCoil(NextSpec,PickupPosition());if(WaitingCoil)WaitingCoil->AttachToComponent(ShoreRoot,FAttachmentTransformRules::KeepWorldTransform);++GeometryRevision;}
        else if(WaitingCoil->Body->GetStaticMesh()!=CoilMesh(NextSpec)||!WaitingCoil->ActorHasTag(FName(*(TEXT("CoilId=")+NextSpec.CoilId))))
        {WaitingCoil->Configure(CoilMesh(NextSpec),Materials[4],NextSpec);ApplyCoilAppearance(WaitingCoil,NextSpec);WaitingCoil->SetActorLocation(GetActorTransform().TransformPosition(PickupPosition()*100));++GeometryRevision;}
    }
    ++PlanRevision;OnChanged.Broadcast();
}
void ACoilLoadingSimulation::PushUndo(){History.Add({PlanToJson(Settings,Placements),PreviewPlacement(),ManifestIndex,bRowsForeToAft});if(History.Num()>64)History.RemoveAt(0);}
int32 ACoilLoadingSimulation::NominalRowCapacity() const
{
    const double OD=NextSpec.OuterDiameter();return FMath::Max(1,FMath::FloorToInt((Settings.HoldWidth-2*FMath::Max(OD*.5,Settings.GearWidth*.5)-2*Settings.ClearanceMargin)/OD+1.e-8)+1);
}
bool ACoilLoadingSimulation::PlaceNext(bool Animate)
{
    if(NextTier<1||NextTier>MaximumTier){Status=TEXT("배치 거절: 코일은 최대 5단까지 배치할 수 있습니다. 앞/뒤 열을 선택하세요.");return false;}
    if(CurrentPhase!=ECoilLoadingPhase::Idle||bEnding){Status=TEXT("작업 중입니다. 완료 또는 취소 후 배치하세요.");return false;}
    FString E;if(!NextSpec.Validate(E)||!Settings.Validate(E)){Status=E;return false;}
    const auto Candidate=PreviewPlacement();const double HalfX=(Candidate.bEyeForeAft?Candidate.Spec.OverallWidth():Candidate.Spec.OuterDiameter())*.5;
    if(FMath::Abs(Candidate.Center.X)+HalfX>Settings.HoldLength*.5-Settings.ClearanceMargin+1.e-6)
    {Status=TEXT("배치 거절: 홀드 길이 끝입니다. 진행 방향 또는 배치 열을 변경하세요.");return false;}
    for(const auto& P:Placements)if(P.Spec.CoilId.Equals(NextSpec.CoilId,ESearchCase::IgnoreCase)){Status=TEXT("코일 ID 중복: 다른 ID를 입력하세요.");return false;}
    RefreshPreview();PushUndo();PhysicsResult=FCoilPhysicsObservation();
    if(!Animate)
    {auto* B=SpawnCoil(NextSpec,Pending.Center);if(!B)return false;CommitPlacement(B);PhysicsResult.State=TEXT("계획만 배치 · 물리 미실행");Status=TEXT("계획 배치 완료 · 다음 코일 위치를 확인하세요.");return true;}
    Suspended=WaitingCoil;WaitingCoil=nullptr;if(Suspended)Suspended->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);if(!Suspended)Suspended=SpawnCoil(NextSpec,PickupPosition());if(!Suspended)return false;
    Suspended->Configure(CoilMesh(NextSpec),Materials[4],NextSpec);ApplyCoilAppearance(Suspended,NextSpec);
    Suspended->Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);++GeometryRevision;CurrentHook=PickupPosition()+FVector(0,0,Settings.GearHeadroom);
    PhaseStart=PickupPosition();CurrentPhase=ECoilLoadingPhase::Lifting;PhaseTime=0;SimulationTime=0;SetActorTickEnabled(true);
    Status=TEXT("장면① 인양 → 해치 통과 → 하강 → Chaos 정착. 경고 상태도 배치 유지");return true;
}
void ACoilLoadingSimulation::CommitPlacement(ACoilLoadingBody* B)
{
    B->SetActorRotation(GetActorRotation()+FRotator(0,Pending.bEyeForeAft?0:90,0));ContactFriction(B,Pending,Settings.bWetSteel);Placements.Add(Pending);Bodies.Add(B);BuildSupports();
    ++NextColumn;const int32 Columns=FMath::Max(1,NominalRowCapacity()-NextTier+1);
    if(NextColumn>Columns||PreviewPlacement().Center.Y+NextSpec.OuterDiameter()*.5>Settings.HoldWidth*.5-Settings.ClearanceMargin+1.e-6){NextColumn=1;++NextRow;NextTier=1;}
    if(Manifest.IsValidIndex(++ManifestIndex))NextSpec=Manifest[ManifestIndex];else NextSpec.CoilId=FString::Printf(TEXT("COIL-%04d"),Placements.Num()+1);
    ++GeometryRevision;RebuildBatches();RefreshPreview();
}
bool ACoilLoadingSimulation::AddTier()
{
    if(CurrentPhase!=ECoilLoadingPhase::Idle)return false;
    const int32 Tier=NextTier,Row=NextRow;
    for(int32 I=0;I<10000&&NextTier==Tier&&NextRow==Row;++I)if(!PlaceNext(false))return false;
    if(Tier<MaximumTier){NextRow=Row;NextTier=Tier+1;NextColumn=1;RefreshPreview();}return true;
}
void ACoilLoadingSimulation::Tick(float Delta)
{
    Super::Tick(Delta);if(bEnding||CurrentPhase==ECoilLoadingPhase::Idle)return;
    PhaseTime+=Delta;SimulationTime+=Delta;
    const auto World=[&](FVector P){return GetActorTransform().TransformPosition(P*100);};
    if(CurrentPhase==ECoilLoadingPhase::Lifting||CurrentPhase==ECoilLoadingPhase::Traversing||CurrentPhase==ECoilLoadingPhase::Lowering)
    {
        const double Duration=CurrentPhase==ECoilLoadingPhase::Traversing?4:3,T=FMath::Clamp(PhaseTime/Duration,0.,1.),Smooth=T*T*(3-2*T);
        const double TransitZ=FMath::Max(QuayZ()+5,Settings.HoldHeight+(Settings.bFoldingCovers?Settings.OpeningLength*.25:0)+NextSpec.OuterDiameter()*.5+Settings.ClearanceMargin+1);
        const FVector Target=CurrentPhase==ECoilLoadingPhase::Lifting?FVector(PhaseStart.X,PhaseStart.Y,TransitZ):CurrentPhase==ECoilLoadingPhase::Traversing?FVector(Pending.Center.X,Pending.Center.Y,TransitZ):Pending.Center+FVector(0,0,.015);
        FVector Position=FMath::Lerp(PhaseStart,Target,Smooth);
        if(CurrentPhase==ECoilLoadingPhase::Traversing&&RailX!=Position.X){RailX=Position.X;UpdatePortalPosition();++GeometryRevision;}
        const double Rope=FMath::Max(3.,CraneTop().Z-Position.Z-Settings.GearHeadroom),Omega=FMath::Sqrt(Gravity/Rope),WindAngle=Settings.WindMps*Settings.WindMps*.00003;
        Position.Y+=Rope*(.012+WindAngle)*FMath::Sin(Omega*SimulationTime)*FMath::Sin(PI*T);CurrentHook=Position+FVector(0,0,Settings.GearHeadroom);UpdateTrolley();
        if(Suspended){Suspended->SetActorLocationAndRotation(World(Position),GetActorRotation()+FRotator(0,Pending.bEyeForeAft?0:90,0));++GeometryRevision;}
        if(T>=1)
        {
            PhaseStart=Target;PhaseTime=0;
            if(CurrentPhase==ECoilLoadingPhase::Lifting)CurrentPhase=ECoilLoadingPhase::Traversing;
            else if(CurrentPhase==ECoilLoadingPhase::Traversing)CurrentPhase=ECoilLoadingPhase::Lowering;
            else
            {
                auto* B=Suspended.Get();Suspended=nullptr;CommitPlacement(B);B->Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);B->Body->SetSimulatePhysics(true);B->Body->WakeAllRigidBodies();
                CurrentPhase=ECoilLoadingPhase::Settling;QuietTime=0;MotionStarts={B->GetActorTransform()};RebuildBatches();PhysicsResult.State=TEXT("Chaos 정착 중 (32각 annulus convex)");
            }
        }
    }
    else if(CurrentPhase==ECoilLoadingPhase::Settling)
    {
        if(Bodies.IsEmpty()){CancelOperation();return;}auto* B=Bodies.Last().Get();const double Speed=B->Body->GetPhysicsLinearVelocity().Size()*.01;
        QuietTime=Speed<.02&&B->Body->GetPhysicsAngularVelocityInDegrees().Size()<2?QuietTime+Delta:0;
        PhysicsResult.MaxSpeed=FMath::Max(PhysicsResult.MaxSpeed,Speed);PhysicsResult.SettlingSeconds=PhaseTime;
        PhysicsResult.MaxDisplacement=FMath::Max(PhysicsResult.MaxDisplacement,FVector::Distance(B->GetActorLocation(),MotionStarts[0].GetLocation())*.01);
        if(QuietTime>=1||PhaseTime>=8){PhysicsResult.bAnomaly=QuietTime<1||PhysicsResult.MaxDisplacement>.05;FinishPhysics();}
        ++GeometryRevision;
    }
    else if(CurrentPhase==ECoilLoadingPhase::MotionTest)
    {
        const double W=2*PI/MotionPeriod,Roll=FMath::DegreesToRadians(RollDegrees),Pitch=FMath::DegreesToRadians(PitchDegrees);
        FVector CSS;FString E;const bool HasCSS=Settings.bUseCSS&&CSSAcceleration(Settings,CSS,E);
        for(int32 I=0;I<Bodies.Num();++I)
        {
            auto* B=Bodies[I].Get();const double Height=Placements[I].Center.Z+Settings.KeelToTanktop;
            const double Ay=HasCSS?CSS.Y*FMath::Sin(W*PhaseTime):Gravity*FMath::Sin(Roll*FMath::Sin(W*PhaseTime))+Height*Roll*W*W*FMath::Sin(W*PhaseTime);
            const double Ax=HasCSS?CSS.X*FMath::Sin(W*PhaseTime+.8):Gravity*FMath::Sin(Pitch*FMath::Sin(W*PhaseTime+.8))+Height*Pitch*W*W*FMath::Sin(W*PhaseTime+.8);
            B->Body->AddForce(GetActorQuat().RotateVector(FVector(-Ax,-Ay,HasCSS?-CSS.Z*FMath::Sin(W*PhaseTime+1.6):0))*100,NAME_None,true);
            const double Move=FVector::Distance(B->GetActorLocation(),MotionStarts[I].GetLocation())*.01;
            const double Angle=FMath::RadiansToDegrees(B->GetActorQuat().AngularDistance(MotionStarts[I].GetRotation()));
            PhysicsResult.MaxDisplacement=FMath::Max(PhysicsResult.MaxDisplacement,Move);PhysicsResult.MaxRotationDegrees=FMath::Max(PhysicsResult.MaxRotationDegrees,Angle);
        }
        ++GeometryRevision;
        if(PhaseTime>=MotionDuration){PhysicsResult.bAnomaly=PhysicsResult.MaxDisplacement>.05||PhysicsResult.MaxRotationDegrees>5;FinishPhysics();}
    }
}
void ACoilLoadingSimulation::FinishPhysics()
{
    for(auto B:Bodies)if(B&&B->Body->IsSimulatingPhysics()){B->Body->SetSimulatePhysics(false);}
    PhysicsResult.State=PhysicsResult.bAnomaly?TEXT("이상 · 변위>0.05m / 회전>5° / 정착시간 초과"):TEXT("관측 임계 이내 · 안전 승인 아님");
    CurrentPhase=ECoilLoadingPhase::Idle;SetActorTickEnabled(false);++GeometryRevision;++PlanRevision;RebuildBatches();Status=PhysicsResult.State;RefreshPreview();OnChanged.Broadcast();
}
bool ACoilLoadingSimulation::StartMotionTest()
{
    if(CurrentPhase!=ECoilLoadingPhase::Idle||Bodies.IsEmpty()||MotionPeriod<=0||MotionDuration<=0||!FMath::IsFinite(MotionPeriod)||!FMath::IsFinite(MotionDuration))return false;
    PhysicsResult=FCoilPhysicsObservation();PhysicsResult.State=TEXT("선박 운동: 해석 등가가속도 + Chaos 정성 확인");MotionStarts.Reset();
    for(int32 I=0;I<Bodies.Num();++I){auto* B=Bodies[I].Get();ContactFriction(B,Placements[I],Settings.bWetSteel);MotionStarts.Add(B->GetActorTransform());B->Body->SetSimulatePhysics(true);B->Body->WakeAllRigidBodies();}
    CurrentPhase=ECoilLoadingPhase::MotionTest;PhaseTime=0;SetActorTickEnabled(true);RebuildBatches();return true;
}
void ACoilLoadingSimulation::CancelOperation()
{
    ++PlanLoadGeneration;bPlanLoading=false;
    if(CurrentPhase==ECoilLoadingPhase::Idle)return;
    if(Suspended){Suspended->Destroy();Suspended=nullptr;if(History.Num())History.Pop();}
    for(auto B:Bodies)if(B)B->Body->SetSimulatePhysics(false);
    CurrentPhase=ECoilLoadingPhase::Idle;SetActorTickEnabled(false);PhysicsResult.State=TEXT("취소 · 물리 수용 미완료");RebuildBatches();++GeometryRevision;++PlanRevision;if(!bEnding)RefreshPreview();
}
void ACoilLoadingSimulation::RebuildPlanBodies()
{
    for(auto B:Bodies)if(B)B->Destroy();Bodies.Reset();SupportedPlacementCount=0;
    for(const auto& P:Placements){auto* B=SpawnCoil(P.Spec,P.Center);if(B){B->SetActorRotation(GetActorRotation()+FRotator(0,P.bEyeForeAft?0:90,0));ContactFriction(B,P,Settings.bWetSteel);Bodies.Add(B);}}
    BuildSupports();RebuildBatches();++GeometryRevision;
}
bool ACoilLoadingSimulation::Undo()
{
    if(CurrentPhase!=ECoilLoadingPhase::Idle||History.IsEmpty())return false;FSettings S;TArray<FPlacement> P;FString E;
    if(!PlanFromJson(History.Last().Json,S,P,E)){Status=E;return false;}
    const auto Saved=History.Last();const auto& Removed=Saved.Next;const bool Rebuild=!SameScene(Settings,S);
    History.Pop();Settings=S;Placements=MoveTemp(P);if(Rebuild)RestoreSceneGeometry();RebuildPlanBodies();
    NextSpec=Removed.Spec;NextTier=Removed.Tier;NextRow=Removed.Row;NextColumn=Removed.Column;bNextDunnage=Removed.bDunnage;bNextChocks=Removed.bChocks;bNextLashing=Removed.bLashing;bNextForeAft=Removed.bEyeForeAft;NextChockKN=Removed.ChockCapacityKN;NextLashingKN=Removed.LashingCapacityKN;NextStiffness=Removed.DiametralStiffnessKNPerM;NextOvalityLimit=Removed.OvalityLimit;NextRollingResistanceM=Removed.RollingResistanceM;ManifestIndex=Saved.ManifestIndex;bRowsForeToAft=Saved.ForeToAft;Highlighted.Reset();
    RefreshPreview();PhysicsResult=FCoilPhysicsObservation();Status=TEXT("되돌리기 완료 · 계획과 다음 코일 위치를 복원했습니다.");return true;
}
bool ACoilLoadingSimulation::RemoveCoil(int32 Index)
{
    if(CurrentPhase!=ECoilLoadingPhase::Idle){Status=TEXT("제거 거절: 작업 완료 또는 취소 후 제거하세요.");return false;}
    if(!Placements.IsValidIndex(Index)){Status=TEXT("먼저 배치된 코일을 클릭하여 선택하세요.");return false;}
    for(const auto& P:Placements)if(P.SupportA==Index||P.SupportB==Index)
    {Status=TEXT("제거 불가: 위 코일이 지지받고 있습니다. 위 코일부터 제거하세요.");return false;}
    PushUndo();const FString Id=Placements[Index].Spec.CoilId;Placements.RemoveAt(Index);
    for(auto& P:Placements){if(P.SupportA>Index)--P.SupportA;if(P.SupportB>Index)--P.SupportB;}
    Highlighted.Reset();RebuildPlanBodies();RefreshPreview();PhysicsResult=FCoilPhysicsObservation();Status=TEXT("선택 코일 제거: ")+Id+TEXT(" · 되돌리기로 복원 가능");return true;
}
int32 ACoilLoadingSimulation::PickCoil(FVector O,FVector D) const
{
    int32 Best=INDEX_NONE;double Distance=DBL_MAX;FCollisionQueryParams Q(SCENE_QUERY_STAT(CoilSelection),true);
    for(int32 I=0;I<Bodies.Num();++I)if(Bodies[I]){FHitResult Hit;if(Bodies[I]->Body->LineTraceComponent(Hit,O,O+D*100000,Q)&&Hit.Distance<Distance){Best=I;Distance=Hit.Distance;}}
    return Best;
}
bool ACoilLoadingSimulation::ImportCoils(const FString& Text,bool Json)
{
    if(CurrentPhase!=ECoilLoadingPhase::Idle)return false;TArray<FSpec> Coils;FString E;if(!ImportManifest(Text,Json,Coils,E)){Status=E;return false;}
    for(const auto& C:Coils)for(const auto& P:Placements)if(P.Spec.CoilId.Equals(C.CoilId,ESearchCase::IgnoreCase)){Status=TEXT("기존 배치와 목록 ID 중복");return false;}
    Manifest=MoveTemp(Coils);ManifestIndex=0;NextSpec=Manifest[0];RefreshPreview();Status=TEXT("목록 불러옴 · SI m/t, 다음 코일부터 순서 사용");return true;
}
bool ACoilLoadingSimulation::RequestPlanSave()
{
    if(CurrentPhase!=ECoilLoadingPhase::Idle||IsPlanSavePending())return false;
    const FString Json=PlanToJson(Settings,Placements);const FTCHARToUTF8 Utf8(*Json);if(Utf8.Length()>1024*1024){Status=TEXT("계획 JSON 저장 한도1MiB 초과. 배치는 유지됩니다.");return false;}
    auto* Files=GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>();if(!Files)return false;
    PlanSaveRequest=Files->RequestAnalysisJson(this,Json,TEXT("CoilLoadingPlan"));const auto R=Files->GetRequestStatus(PlanSaveRequest);
    if(R.State==EVirtualSensorFileSaveState::Failed){Status=R.Message;PlanSaveRequest.Reset();return false;}
    Status=TEXT("계획 JSON 저장 중 · 완료 후 파일 경로 표시");return true;
}
bool ACoilLoadingSimulation::LoadPlan(const FString& Path)
{
    if(CurrentPhase!=ECoilLoadingPhase::Idle||bEnding)return false;
    const uint64 Generation=++PlanLoadGeneration,Revision=PlanRevision;bPlanLoading=true;Status=TEXT("계획 파일 읽기·검증 중");
    const TWeakObjectPtr<ACoilLoadingSimulation> Weak(this);
    Async(EAsyncExecution::ThreadPool,[Weak,Path,Generation,Revision]()
    {
        check(!IsInGameThread());FString Text,E;FSettings S;TArray<FPlacement> P;
        const int64 Size=IFileManager::Get().FileSize(*Path);
        if(Size<0)E=TEXT("file: 계획 파일 없음");
        else if(Size>16*1024*1024)E=TEXT("file: 계획 파일16MiB 초과");
        else if(!FFileHelper::LoadFileToString(Text,*Path))E=TEXT("file: 계획 파일 읽기 실패");
        else PlanFromJson(Text,S,P,E);
        AsyncTask(ENamedThreads::GameThread,[Weak,Generation,Revision,S,P=MoveTemp(P),E=MoveTemp(E)]() mutable
        {if(auto* Self=Weak.Get())Self->FinishPlanLoad(Generation,Revision,S,MoveTemp(P),MoveTemp(E));});
    });return true;
}
void ACoilLoadingSimulation::ParseLoadedPlan(FString Json,uint64 Generation,uint64 Revision)
{
    const TWeakObjectPtr<ACoilLoadingSimulation> Weak(this);
    Async(EAsyncExecution::ThreadPool,[Weak,Json=MoveTemp(Json),Generation,Revision]()
    {
        check(!IsInGameThread());FSettings S;TArray<FPlacement> P;FString E;PlanFromJson(Json,S,P,E);
        AsyncTask(ENamedThreads::GameThread,[Weak,Generation,Revision,S,P=MoveTemp(P),E=MoveTemp(E)]() mutable
        {if(auto* Self=Weak.Get())Self->FinishPlanLoad(Generation,Revision,S,MoveTemp(P),MoveTemp(E));});
    });
}
void ACoilLoadingSimulation::FinishPlanLoad(uint64 Generation,uint64 Revision,FSettings S,TArray<FPlacement> P,FString E)
{
    check(IsInGameThread());if(bEnding||Generation!=PlanLoadGeneration)return;bPlanLoading=false;
    if(Revision!=PlanRevision||CurrentPhase!=ECoilLoadingPhase::Idle){Status=TEXT("계획 로드 취소: 요청 뒤 배치/설정 변경");return;}
    if(!E.IsEmpty()){Status=TEXT("계획 로드 실패: ")+E;return;}
    // Only application touches UObjects or scene resources, on the game thread.
    const bool Rebuild=!SameScene(Settings,S);PushUndo();Settings=S;Placements=MoveTemp(P);if(Rebuild)RestoreSceneGeometry();RebuildPlanBodies();NextSpec.CoilId=FString::Printf(TEXT("COIL-%04d"),Placements.Num()+1);RefreshPreview();Status=TEXT("계획 불러옴 · 물리 결과는 재시험 필요");PhysicsResult=FCoilPhysicsObservation();
}
bool ACoilLoadingSimulation::SaveSlot(const FString& Slot)
{
    if(CurrentPhase!=ECoilLoadingPhase::Idle||bSlotSaving)return false;auto* Save=Cast<UCoilLoadingSaveGame>(UGameplayStatics::CreateSaveGameObject(UCoilLoadingSaveGame::StaticClass()));Save->PlanJson=PlanToJson(Settings,Placements);bSlotSaving=true;Status=TEXT("적재 계획 슬롯 저장 중");
    UGameplayStatics::AsyncSaveGameToSlot(Save,Slot,0,FAsyncSaveGameToSlotDelegate::CreateWeakLambda(this,[this](const FString&,int32,bool OK){if(bEnding)return;bSlotSaving=false;Status=OK?TEXT("적재 계획 슬롯 저장 완료"):TEXT("적재 계획 슬롯 저장 실패");}));return true;
}
bool ACoilLoadingSimulation::LoadSlot(const FString& Slot)
{
    if(CurrentPhase!=ECoilLoadingPhase::Idle||bSlotSaving||bEnding)return false;
    const uint64 Generation=++PlanLoadGeneration,Revision=PlanRevision;bPlanLoading=true;Status=TEXT("적재 계획 슬롯 읽기·검증 중");
    UGameplayStatics::AsyncLoadGameFromSlot(Slot,0,FAsyncLoadGameFromSlotDelegate::CreateWeakLambda(this,[this,Generation,Revision](const FString&,int32,USaveGame* Loaded)
    {
        if(bEnding||Generation!=PlanLoadGeneration)return;
        if(auto* Save=Cast<UCoilLoadingSaveGame>(Loaded))ParseLoadedPlan(Save->PlanJson,Generation,Revision);
        else FinishPlanLoad(Generation,Revision,FSettings(),{},TEXT("slot: 저장 데이터 없음/클래스 오류"));
    }));return true;
}
void ACoilLoadingSimulation::DrawRig()
{
    ++RigRefreshes;RigLines->Flush();if(CurrentPhase==ECoilLoadingPhase::Idle){if(RailX!=-8){RailX=-8;UpdatePortalPosition();++GeometryRevision;}CurrentHook=PickupPosition()+FVector(0,0,Settings.GearHeadroom);}UpdateTrolley();if(ActiveView==4)return;
    const FVector Load=Suspended?GetActorTransform().InverseTransformPosition(Suspended->GetActorLocation())*.01:PickupPosition();
    const FVector Hook=Suspended?CurrentHook:Load+FVector(0,0,Settings.GearHeadroom);
    const auto W=[&](FVector P){return GetActorTransform().TransformPosition(P*100);};
    const auto Line=[&](FVector A,FVector B,FLinearColor C,float Thick){RigLines->DrawLine(W(A),W(B),C,0,Thick,.22);};
    // Six cached cylinder instances connect actual top sheaves to the block.
    Line(Hook,Hook+FVector(0,0,-.35),FLinearColor::Yellow,8);
    // The selected solid C-hook / sling is instanced with the hook block.
    for(int32 I=0;I<48;++I){const double A=2*PI*I/48,B=2*PI*(I+1)/48;const FVector C(Load.X,Load.Y,CurrentPhase==ECoilLoadingPhase::Idle?QuayZ()+.05:.08);Line(C+FVector(3*FMath::Cos(A),3*FMath::Sin(A),0),C+FVector(3*FMath::Cos(B),3*FMath::Sin(B),0),FLinearColor::Red,2);}
    for(int32 Index:Highlighted)
    {
        const FPlacement* P=Placements.IsValidIndex(Index)?&Placements[Index]:Index==Placements.Num()?&Pending:nullptr;if(!P)continue;
        for(double X:{-P->Spec.Width*.5-.025,P->Spec.Width*.5+.025})for(int32 I=0;I<32;++I)
        {const double A=I*2*PI/32,B=(I+1)*2*PI/32;Line(P->Center+FVector(X,(P->Spec.OD*.5+.025)*FMath::Cos(A),(P->Spec.OD*.5+.025)*FMath::Sin(A)),P->Center+FVector(X,(P->Spec.OD*.5+.025)*FMath::Cos(B),(P->Spec.OD*.5+.025)*FMath::Sin(B)),FLinearColor(0,1,1),3);}
    }
}
void ACoilLoadingSimulation::HighlightCoils(const TArray<int32>& Indices){Highlighted=Indices;++PlanRevision;DrawRig();}
void ACoilLoadingSimulation::Observe()
{
    if(bEnding)return;DrawRig();
    if(!PlanSaveRequest.IsEmpty())if(auto* Files=GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>())
    {const auto R=Files->GetRequestStatus(PlanSaveRequest);if(R.IsTerminal()){if(R.State==EVirtualSensorFileSaveState::Succeeded&&R.FileResults.Num()){LastPlanSavePath=R.FileResults[0].AbsolutePath;Status=TEXT("적재 계획 JSON 저장 완료 · 불러오기 경로 갱신");}else Status=R.Message;PlanSaveRequest.Reset();++PlanRevision;}}
    if(!Lidar)return;const auto Snapshot=Lidar->ScanComponent->GetLastFrameSnapshot();
    if(!Snapshot||!Snapshot->Points||Snapshot->FrameId==MeasuredFrame)return;MeasuredFrame=Snapshot->FrameId;LidarCoilHits=0;UnmatchedCoilIdPoints=0;FittedCoilCount=0;
    TMap<FName,int32> IdIndices;for(int32 I=0;I<Placements.Num();++I)IdIndices.Add(FName(*(TEXT("CoilId=")+Placements[I].Spec.CoilId)),I);
    TSet<FName> SourceIds;SourceIds.Add(FName(*(TEXT("CoilId=")+NextSpec.CoilId)));for(auto A:Stock)if(A)for(const auto Tag:A->Tags)if(Tag.ToString().StartsWith(TEXT("CoilId=")))SourceIds.Add(Tag);
    TMap<int32,TArray<FVector>> Observations;
    for(const auto& P:*Snapshot->Points)if(P.bHit&&P.SemanticLabel==TEXT("Coil"))
    {
        const int32* Index=nullptr;for(const FName Tag:P.HitActorTags){Index=IdIndices.Find(Tag);if(Index)break;}
        if(!Index){bool Source=false;for(const FName Tag:P.HitActorTags)if(SourceIds.Contains(Tag)){Source=true;break;}if(!Source)++UnmatchedCoilIdPoints;continue;}
        // WorldLocation is acquisition-world centimetres. Do not reapply the
        // latest sensor pose; convert exactly once to scenario-local metres.
        Observations.FindOrAdd(*Index).Add(GetActorTransform().InverseTransformPosition(P.WorldLocation)*.01);++LidarCoilHits;
    }
    TArray<double> CenterErrors,Residuals;
    for(const auto& Pair:Observations)
    {
        const auto Fit=FitCylinder(Placements[Pair.Key],Pair.Value);if(!Fit.bValid)continue;
        ++FittedCoilCount;CenterErrors.Add(FVector::Distance(Fit.Center,Placements[Pair.Key].Center));Residuals.Add(Fit.ResidualP95);
    }
    bLidarCompared=CenterErrors.Num()>0;
    if(bLidarCompared){CenterErrors.Sort();Residuals.Sort();const int32 I=FMath::Min(CenterErrors.Num()-1,FMath::FloorToInt(CenterErrors.Num()*.95));LidarDeviationP95=CenterErrors[I];LidarFitResidualP95=Residuals[I];}
}
void ACoilLoadingSimulation::SetView(int32 V){ActiveView=FMath::Clamp(V,0,4);if(Ghost)Ghost->Body->SetVisibility(V!=4);if(Lidar)Lidar->VisualizationComponent->SetWorldPointCloudEnabled(V==4);++PlanRevision;}
void ACoilLoadingSimulation::EndPlay(const EEndPlayReason::Type Reason)
{
    bEnding=true;++PlanLoadGeneration;bPlanLoading=false;GetWorldTimerManager().ClearTimer(PresentationTimer);if(!PlanSaveRequest.IsEmpty())if(auto* Files=GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>())Files->CancelRequest(PlanSaveRequest);PlanSaveRequest.Reset();CancelOperation();if(Lidar){Lidar->StopSensor();Lidar->ScanComponent->SetRayQueryFactory({});Lidar->Destroy();}
    CachedQuery.Reset();for(auto B:Bodies)if(B)B->Destroy();for(auto B:Stock)if(B)B->Destroy();if(Ghost)Ghost->Destroy();if(Suspended)Suspended->Destroy();if(WaitingCoil)WaitingCoil->Destroy();OnChanged.Clear();Super::EndPlay(Reason);
}

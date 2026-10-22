#include "ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarPlayerDisplayComponent.h"
#include "ProceduralMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "TimerManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"

namespace
{
bool HatchPresentationSamePositions(const FProcMeshSection* Section,const TArray<FVector>& Vertices)
{if(!Section||Section->ProcVertexBuffer.Num()!=Vertices.Num())return false;for(int I=0;I<Vertices.Num();++I)if(!FVector(Section->ProcVertexBuffer[I].Position).Equals(Vertices[I],1.e-4))return false;return true;}
bool InFov(const AVirtualLidarSensorActor* A,FVector World)
{
    if(!A)return false;const auto* S=A->ScanComponent.Get();
    const FVector D=S->GetComponentTransform().InverseTransformPosition(World);const double Range=D.Size();
    if(Range<S->DeviceSpec.MinRangeCm||Range>S->MaxDistance)return false;
    const double Az=FMath::RadiansToDegrees(FMath::Atan2(D.Y,D.X));
    const double El=FMath::RadiansToDegrees(FMath::Atan2(D.Z,FMath::Sqrt(D.X*D.X+D.Y*D.Y)));
    return FMath::Abs(Az)<=S->HorizontalFov*.5+1.e-5&&El>=S->MinVerticalAngle-1.e-5&&El<=S->MaxVerticalAngle+1.e-5;
}
void HatchPresentationBox(TArray<FVector>& V,TArray<int32>& T,FVector C,FVector Size)
{
    const int32 B=V.Num();for(int32 Z=0;Z<2;++Z)for(int32 Y=0;Y<2;++Y)for(int32 X=0;X<2;++X)V.Add((C+FVector((X-.5)*Size.X,(Y-.5)*Size.Y,(Z-.5)*Size.Z))*100);
    const int32 Indices[]={0,2,3,0,3,1,4,5,7,4,7,6,0,1,5,0,5,4,2,6,7,2,7,3,0,4,6,0,6,2,1,3,7,1,7,5};
    for(int32 I=0;I<36;I+=3){T.Add(B+Indices[I]);T.Add(B+Indices[I+2]);T.Add(B+Indices[I+1]);}
}
}
AVirtualLidarSensorActor* AShipHatchSimulation::SensorAt(int32 I) const{return I==0?MLX.Get():I==1?Livox.Get():I==2?CabRight.Get():nullptr;}
bool AShipHatchSimulation::SensorEnabled(int32 I) const{return I==0?bMLX:I==1?bLivox:I==2&&bFieldLayout&&bCabRight;}
FShipHatchSight AShipHatchSimulation::DiagnoseSight(int32 I,FVector Target) const
{
    FShipHatchSight R;const auto* A=SensorAt(I);
    auto Cause=[&](const TCHAR* Key,const TCHAR* Text){R.Cause=Key;R.Description=Text;return R;};
    if(I<0||I>2||!SensorEnabled(I)||!A)return Cause(TEXT("Disabled"),TEXT("센서 꺼짐"));
    const auto* Scan=A->ScanComponent.Get();const FVector World=GetActorTransform().TransformPosition(Target*100),Origin=Scan->GetComponentLocation();
    const auto Local=Scan->GetComponentTransform().InverseTransformPosition(World);R.DistanceMeters=Local.Size()*.01;R.RangeMeters=Scan->MaxDistance*.01;
    const double Pitch=FMath::RadiansToDegrees(FMath::Atan2(Local.Z,FMath::Sqrt(Local.X*Local.X+Local.Y*Local.Y)));
    if(Scan->DeviceProfile==EVirtualLidarDeviceProfile::HesaiXT32&&(Pitch< -8.001||Pitch>7.001))R.RangeMeters=FMath::Min(R.RangeMeters,50.);
    if(R.DistanceMeters>R.RangeMeters||R.DistanceMeters<Scan->DeviceSpec.MinRangeCm*.01){R.Cause=TEXT("Range");R.Description=FString::Printf(TEXT("거리 한계: %.1f m / 최대 %.1f m"),R.DistanceMeters,R.RangeMeters);return R;}
    if(!InFov(A,World))return Cause(TEXT("Fov"),TEXT("시야각 밖"));
    FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(HatchCellDiagnostic),true);
    if(!GetWorld()->LineTraceSingleByChannel(Hit,Origin,World+(World-Origin).GetSafeNormal()*.5,ECC_Visibility,Query))return Cause(TEXT("Clear"),TEXT("가시: 샘플링·재질 검출·갱신 확인"));
    R.FirstHitMeters=GetActorTransform().InverseTransformPosition(Hit.ImpactPoint)*.01;
    if(Hit.GetComponent()==CabPresentation)return Cause(TEXT("Cab"),TEXT("차폐: 독립 운전실 캐리지"));
    if(Hit.GetComponent()==GrabPresentation)return Cause(TEXT("Grab"),TEXT("차폐: 그랩·로프"));
    if(FVector::Distance(Hit.ImpactPoint,World)<8)return Cause(TEXT("Clear"),TEXT("가시: 샘플링·재질 검출·갱신 확인"));
    if(Hit.GetActor()==UnloaderPresentation)
    {
        const auto P=R.FirstHitMeters;if(FMath::Abs(P.X-CabX)<3&&FMath::Abs(P.Y-CabY)<2&&P.Z<HoldDepth+CabHeight+3)return Cause(TEXT("Cab"),TEXT("차폐: 운전실"));
        return Cause(TEXT("Rig"),TEXT("차폐: 붐·트롤리/GTSU 구조"));
    }
    if(Hit.GetActor()==Vessel)
    {
        const auto P=R.FirstHitMeters;if(FMath::Abs(P.X)<OpeningLength*.5+.5&&FMath::Abs(P.Y)>OpeningWidth*.5&&FMath::Abs(P.Z-(HoldDepth+CoamingHeight+.56))<.6)return Cause(TEXT("Cover"),TEXT("차폐: 열린 해치 커버"));
        return Cause(TEXT("Vessel"),TEXT("차폐: 선체·갑판"));
    }
    if(Hit.GetActor()==Cargo)return Cause(TEXT("Cargo"),TEXT("차폐: 원료 사면"));
    if(Hit.GetActor()&&Hit.GetActor()->ActorHasTag(TEXT("HatchCoaming")))return Cause(TEXT("Coaming"),TEXT("차폐: 코밍"));
    return Cause(TEXT("Structure"),TEXT("차폐: 홀드 벽·갑판"));
}
FVector AShipHatchSimulation::SensorOrigin(int32 I) const
{
    if(!bFieldLayout)return SensorLocalOrigin(I==0);
    return (I==0?FVector(0,BoomTipY(),HoldDepth+BoomHeight):FVector(CabX+(I==1?-1.3:1.3),CabY,HoldDepth+CabHeight))+SensorOffsets[FMath::Clamp(I,0,2)];
}
bool AShipHatchSimulation::VisibleFromSensor(int32 I,FVector Target) const
{
    if(!SensorEnabled(I)||!ShipHatch::ThroughOpening(Current.Settings,SensorOrigin(I),Target)||!InFov(SensorAt(I),GetActorTransform().TransformPosition(Target*100)))return false;
    if(bShowGrab&&GrabPresentation){FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(HatchGrabCoverage),true);if(GrabPresentation->LineTraceComponent(Hit,GetActorTransform().TransformPosition(SensorOrigin(I)*100),GetActorTransform().TransformPosition(Target*100),Query))return false;}
    if(Vessel){FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(HatchVesselCoverage),true);if(Vessel->Mesh->LineTraceComponent(Hit,GetActorTransform().TransformPosition(SensorOrigin(I)*100),GetActorTransform().TransformPosition(Target*100),Query))return false;}
    if(CabPresentation){FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(HatchCabCoverage),true);if(CabPresentation->LineTraceComponent(Hit,GetActorTransform().TransformPosition(SensorOrigin(I)*100),GetActorTransform().TransformPosition(Target*100),Query))return false;}
    if(UnloaderPresentation)
    {
        FHitResult Hit;FCollisionQueryParams Query(SCENE_QUERY_STAT(HatchRigCoverage),true);
        if(UnloaderPresentation->Mesh->LineTraceComponent(Hit,GetActorTransform().TransformPosition(SensorOrigin(I)*100),GetActorTransform().TransformPosition(Target*100),Query))return false;
    }
    return true;
}
void AShipHatchSimulation::SetFieldSensor(int32 I,bool Enabled,int32 Profile)
{
    if(I<0||I>2||bGenerating)return;bFieldLayout=true;FieldProfiles[I]=FMath::Clamp(Profile,0,8);
    if(I==0)bMLX=Enabled;else if(I==1)bLivox=Enabled;else bCabRight=Enabled;
    ConfigureSensors();++SurfaceRevision;OnSurfaceChanged.Broadcast();
}
void AShipHatchSimulation::ShowUnloader(bool Enabled){bShowUnloader=Enabled;if(UnloaderPresentation){UnloaderPresentation->SetActorHiddenInGame(false);UnloaderPresentation->Mesh->SetVisibility(Enabled,false);if(RigFineDetails)RigFineDetails->SetVisibility(Enabled);if(CabPresentation)CabPresentation->SetVisibility(Enabled);if(UnloaderLabel)UnloaderLabel->SetVisibility(Enabled);if(GrabPresentation)GrabPresentation->SetVisibility(bShowGrab);}}
void AShipHatchSimulation::VisitUnloaderMeshes(TFunctionRef<void(UProceduralMeshComponent*)> Visitor) const
{if(UnloaderPresentation)Visitor(UnloaderPresentation->Mesh);if(RigFineDetails)Visitor(RigFineDetails);if(CabPresentation)Visitor(CabPresentation);}
void AShipHatchSimulation::ApplyUnloader()
{
    if(!bFieldLayout){if(UnloaderPresentation)UnloaderPresentation->Destroy();UnloaderPresentation=nullptr;GrabPresentation=nullptr;CabPresentation=nullptr;UnloaderLabel=nullptr;return;}
    if(!UnloaderPresentation)
    {
        FActorSpawnParameters P;P.Owner=this;P.ObjectFlags|=RF_Transient;UnloaderPresentation=GetWorld()->SpawnActor<AShipHatchPart>(AShipHatchPart::StaticClass(),GetActorTransform(),P);
        if(!UnloaderPresentation)return;auto* M=UnloaderPresentation->Mesh.Get();UnloaderPresentation->Tags.Add(TEXT("ShipHatchPreviewSensor"));UnloaderPresentation->Tags.Add(TEXT("ShipHatchUnloader"));
        M->SetCollisionEnabled(ECollisionEnabled::QueryOnly);M->SetCollisionResponseToAllChannels(ECR_Block);M->SetHiddenInSceneCapture(false);M->SetCastShadow(false);M->SetCanEverAffectNavigation(false);UnloaderPresentation->Tags.Add(TEXT("HatchOutside"));UnloaderPresentation->SetColor(FLinearColor(.012,.13,.075));
    }
    TArray<FVector> V;TArray<int32> T;
    const double Tip=BoomTipY(),Root=Tip-BoomLength,H=HoldDepth+BoomHeight+1.2;
    for(int Sign:{-1,1})HatchPresentationBox(V,T,FVector(Sign*.7,(Root+Tip)*.5,H),FVector(.22,BoomLength,.3));
    for(double Y:{Root,(Root+Tip)*.5,Tip})HatchPresentationBox(V,T,FVector(0,Y,H),FVector(1.6,.22,.3));
    // Separate side rail lets the independent cab pass the central grab trolley.
    HatchPresentationBox(V,T,FVector(CabX,(Root+Tip)*.5,H),FVector(.22,BoomLength,.3));
    for(double Y=Root;Y<=Tip;Y+=4)HatchPresentationBox(V,T,FVector((CabX-1.5)*.5,Y,H+.15),FVector(FMath::Abs(CabX)+1.5,.18,.3));
    TArray<FVector> N;N.Init(FVector::UpVector,V.Num());auto* M=UnloaderPresentation->Mesh.Get();
    if(M->GetProcMeshSection(0)&&M->GetProcMeshSection(0)->ProcVertexBuffer.Num()==V.Num()){if(!HatchPresentationSamePositions(M->GetProcMeshSection(0),V))M->UpdateMeshSection_LinearColor(0,V,N,TArray<FVector2D>(),TArray<FLinearColor>(),TArray<FProcMeshTangent>());}
    else M->CreateMeshSection_LinearColor(0,V,T,N,TArray<FVector2D>(),TArray<FLinearColor>(),TArray<FProcMeshTangent>(),true);
    BuildCabModel();BuildUnloaderDetails();
    if(!UnloaderLabel){UnloaderLabel=NewObject<UTextRenderComponent>(UnloaderPresentation);UnloaderLabel->SetupAttachment(M);UnloaderLabel->SetText(FText::FromString(TEXT("GTSU 10")));UnloaderLabel->SetHorizontalAlignment(EHTA_Center);UnloaderLabel->SetWorldSize(110);UnloaderLabel->SetTextRenderColor(FColor(245,220,110));UnloaderLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);UnloaderLabel->SetHiddenInSceneCapture(true);UnloaderLabel->SetCastShadow(false);UnloaderLabel->RegisterComponent();}
    UnloaderLabel->SetRelativeLocationAndRotation(FVector(0,Root+1.0,H+1.8)*100,FRotator(0,90,0));
    ShowUnloader(bShowUnloader);
}

double AShipHatchSimulation::EffectiveMLXHeight() const
{
    // 1m edge margin, plus coaming: narrow 23.3 degree dimension lies along X.
    return FMath::Max(SensorAboveDeck,CoamingHeight+(OpeningLength*.5+1)/FMath::Tan(FMath::DegreesToRadians(11.65)));
}
FVector AShipHatchSimulation::SensorLocalOrigin(bool IsMLX) const
{return FVector(0,IsMLX?-.6:.6,HoldDepth+(IsMLX?EffectiveMLXHeight():FMath::Clamp(SensorAboveDeck,3.,12.)));}
bool AShipHatchSimulation::VisibleFromSelected(FVector Target) const
{
    const FVector World=GetActorTransform().TransformPosition(Target*100);
    for(int32 I=0;I<3;++I)if(VisibleFromSensor(I,Target))return true;return false;
}
void AShipHatchSimulation::Coverage(double& Opening,double& VisibleFloor,double& FloorArea) const
{
    int32 Open=0,Floor=0,Potential=0;constexpr int32 N=80;
    for(int32 Y=0;Y<N;++Y)for(int32 X=0;X<N;++X)
    {
        Open+=VisibleFromSelected(FVector(((X+.5)/N-.5)*OpeningLength,((Y+.5)/N-.5)*OpeningWidth,HoldDepth+CoamingHeight));
        const FVector P(((X+.5)/N-.5)*HoldLength,((Y+.5)/N-.5)*HoldWidth,0);
        bool Through=false;for(int32 I=0;I<3;++I)Through|=SensorEnabled(I)&&ShipHatch::ThroughOpening(Current.Settings,SensorOrigin(I),P);
        if(Through){++Potential;Floor+=VisibleFromSelected(P);}
    }
    Opening=100.*Open/(N*N);VisibleFloor=Potential?100.*Floor/Potential:0;FloorArea=double(Potential)/(N*N)*HoldLength*HoldWidth;
}
void AShipHatchSimulation::UpdateSensorPresentation()
{
    if(!SensorPresentation.Num())
    {
        for(int32 I=0;I<(bFieldLayout?3:2);++I)
        {
            FActorSpawnParameters P;P.Owner=this;P.ObjectFlags|=RF_Transient;
            auto* A=GetWorld()->SpawnActor<AShipHatchPart>(AShipHatchPart::StaticClass(),GetActorTransform(),P);if(!A)return;
            A->Tags.Add(TEXT("ShipHatchPreviewSensor"));A->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);A->Mesh->SetGenerateOverlapEvents(false);
            A->Mesh->SetCanEverAffectNavigation(false);A->Mesh->SetHiddenInSceneCapture(true);A->Mesh->SetCastShadow(false);SensorPresentation.Add(A);

        }
    }
    if(SensorPresentation.Num()!=(bFieldLayout?3:2)){for(auto A:SensorPresentation)if(A)A->Destroy();SensorPresentation.Reset();UpdateSensorPresentation();return;}
    for(int32 I=0;I<(bFieldLayout?3:2);++I)
    {
        auto* A=SensorPresentation[I].Get();A->SetActorTransform(GetActorTransform());A->Mesh->SetVisibility(bSensorMarkersVisible);TArray<FVector> V;TArray<int32> T;
        if(I<3)
        {
            const FVector O=SensorOrigin(I);HatchPresentationBox(V,T,O,FVector(I==0?1.4:.9,1.,.8));
            const bool Enabled=SensorEnabled(I),Selected=SelectedSensor==I||(SelectedSensor<0&&Enabled);
            A->SetColor(!Enabled?FLinearColor(.12,.14,.16):Selected?FLinearColor(1,.62,.025):FLinearColor(.04,.45,.65));
        }
        TArray<FVector> Normals;Normals.Init(FVector::UpVector,V.Num());
        const auto* Existing=A->Mesh->GetProcMeshSection(0);
        if(Existing&&Existing->ProcVertexBuffer.Num()==V.Num())A->Mesh->UpdateMeshSection_LinearColor(0,V,Normals,TArray<FVector2D>(),TArray<FLinearColor>(),TArray<FProcMeshTangent>());
        else A->Mesh->CreateMeshSection_LinearColor(0,V,T,Normals,TArray<FVector2D>(),TArray<FLinearColor>(),TArray<FProcMeshTangent>(),false);
        if(I>0&&CabPresentation)A->AttachToComponent(CabPresentation,FAttachmentTransformRules::KeepWorldTransform);
    }
}
void AShipHatchSimulation::SetSensorMarkersVisible(bool Visible)
{bSensorMarkersVisible=Visible;for(auto A:SensorPresentation)if(A)A->Mesh->SetVisibility(Visible);}
bool AShipHatchSimulation::SelectSensorAt(FVector O,FVector D)
{
    if(!D.Normalize())return false;
    for(int32 I=0;I<(bFieldLayout?3:2);++I)
    {
        const FVector P=GetActorTransform().TransformPosition(SensorOrigin(I)*100),Delta=P-O;const double Along=FVector::DotProduct(Delta,D);
        if(Along>0&&(Delta-D*Along).SizeSquared()<FMath::Square(160.))
        {SelectedSensor=I;UpdateSensorPresentation();return true;}
    }
    return false;
}
bool AShipHatchSimulation::PresentationOccludes(FVector Origin,FVector End) const
{
    // UI picking only. Decorative hardware stays absent from sensor collision,
    // optical classification and SceneCapture; its visible triangles still
    // prevent a grab through the hardware in either player view.
    for(int32 Part=0;Part<SensorPresentation.Num()+1;++Part)
    {
        auto* A=Part<SensorPresentation.Num()?SensorPresentation[Part].Get():(bShowUnloader?UnloaderPresentation.Get():nullptr);if(!A||!A->Mesh->IsVisible())continue;
        const auto* Section=A->Mesh->GetProcMeshSection(0);if(!Section)continue;
        const auto Pose=A->Mesh->GetComponentTransform();const FVector O=Pose.InverseTransformPosition(Origin),E=Pose.InverseTransformPosition(End);
        if(!FMath::LineBoxIntersection(Section->SectionLocalBox,O,E,E-O))continue;
        for(int32 I=0;I<Section->ProcIndexBuffer.Num();I+=3)
        {
            FVector Hit,Normal;if(FMath::SegmentTriangleIntersection(O,E,Section->ProcVertexBuffer[Section->ProcIndexBuffer[I]].Position,Section->ProcVertexBuffer[Section->ProcIndexBuffer[I+1]].Position,Section->ProcVertexBuffer[Section->ProcIndexBuffer[I+2]].Position,Hit,Normal))return true;
        }
    }
    return false;
}
void AShipHatchSimulation::SetScanDisplay(bool Enabled)
{
    if(Enabled==bScanDisplay)return;bScanDisplay=Enabled;
    GetWorldTimerManager().ClearTimer(ScanDisplayTimer);
    if(Enabled){RefreshScanDisplay();GetWorldTimerManager().SetTimer(ScanDisplayTimer,this,&AShipHatchSimulation::RefreshScanDisplay,.2f,true);}
    else if(ScanDisplay){ScanDisplay->BatchedLines.Reset();ScanDisplay->BatchedPoints.Reset();ScanDisplay->MarkRenderStateDirty();ScanDisplay->SetVisibility(false);}
}
int32 AShipHatchSimulation::DisplayRayCount() const{return ScanDisplay?ScanDisplay->BatchedLines.Num():0;}
void AShipHatchSimulation::RefreshScanDisplay()
{
    if(!bScanDisplay||bEnding)return;
    if(!ScanDisplay)
    {
        // Pure presentation on the map-local actor: no collision, capture or scheduler work.
        Tags.AddUnique(TEXT("ShipHatchPreviewSensor"));ScanDisplay=NewObject<ULidarPlayerDisplayComponent>(this);ScanDisplay->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ScanDisplay->SetCanEverAffectNavigation(false);ScanDisplay->SetGenerateOverlapEvents(false);ScanDisplay->SetHiddenInSceneCapture(true);ScanDisplay->SetCastShadow(false);
        ScanDisplay->bCalculateAccurateBounds=true;ScanDisplay->RegisterComponent();ScanDisplay->SetComponentTickEnabled(false);
    }
    auto& Lines=ScanDisplay->BatchedLines;auto& Points=ScanDisplay->BatchedPoints;Lines.Reset(DisplayRayLimit);Points.Reset(DisplayRayLimit);++ScanPhase;
    for(int32 Index=0;Index<3;++Index)
    {
        auto* A=SensorEnabled(Index)?SensorAt(Index):nullptr;const FLinearColor Color=Index==0?FLinearColor(.1,.9,1):Index==1?FLinearColor(1,.6,.06):FLinearColor(.9,.2,.8);if(!A)continue;
        const auto Frame=A->ScanComponent->GetLastFrameSnapshot();if(!Frame||!Frame->Points||!Frame->Points->Num())continue;
        const auto& Source=*Frame->Points;const FVector Origin=Frame->AcquisitionTransform.GetLocation();
        const int32 Stride=FMath::Max(1,Source.Num()/32),Offset=(ScanPhase*37)%Stride;
        for(int32 I=Offset;I<Source.Num()&&Lines.Num()<DisplayRayLimit;I+=Stride)
        {
            const auto& P=Source[I];if(!P.bHit)continue;
            Lines.Emplace(Origin,P.WorldLocation,Color,0.f,1.f,SDPG_World);Points.Emplace(P.WorldLocation,Color,5,0.f,SDPG_World);
        }
    }
    ScanDisplay->SetVisibility(true);ScanDisplay->UpdateBounds();ScanDisplay->MarkRenderStateDirty();
}

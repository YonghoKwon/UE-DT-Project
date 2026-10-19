#include "ShipHatchSimulation.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarPlayerDisplayComponent.h"
#include "ProceduralMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

namespace
{
bool InFov(const AVirtualLidarSensorActor* A,FVector World)
{
    if(!A)return false;const auto* S=A->ScanComponent.Get();
    const FVector D=S->GetComponentTransform().InverseTransformPosition(World);const double Range=D.Size();
    if(Range<S->DeviceSpec.MinRangeCm||Range>S->MaxDistance)return false;
    const double Az=FMath::RadiansToDegrees(FMath::Atan2(D.Y,D.X));
    const double El=FMath::RadiansToDegrees(FMath::Atan2(D.Z,FMath::Sqrt(D.X*D.X+D.Y*D.Y)));
    return FMath::Abs(Az)<=S->HorizontalFov*.5+1.e-5&&El>=S->MinVerticalAngle-1.e-5&&El<=S->MaxVerticalAngle+1.e-5;
}
void Box(TArray<FVector>& V,TArray<int32>& T,FVector C,FVector Size)
{
    const int32 B=V.Num();for(int32 Z=0;Z<2;++Z)for(int32 Y=0;Y<2;++Y)for(int32 X=0;X<2;++X)V.Add((C+FVector((X-.5)*Size.X,(Y-.5)*Size.Y,(Z-.5)*Size.Z))*100);
    for(int32 I:{0,2,3,0,3,1,4,5,7,4,7,6,0,1,5,0,5,4,2,6,7,2,7,3,0,4,6,0,6,2,1,3,7,1,7,5})T.Add(B+I);
}
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
    return (bMLX&&ShipHatch::ThroughOpening(Current.Settings,SensorLocalOrigin(true),Target)&&InFov(MLX,World))||
        (bLivox&&ShipHatch::ThroughOpening(Current.Settings,SensorLocalOrigin(false),Target)&&InFov(Livox,World));
}
void AShipHatchSimulation::Coverage(double& Opening,double& VisibleFloor,double& FloorArea) const
{
    int32 Open=0,Floor=0,Potential=0;constexpr int32 N=80;
    for(int32 Y=0;Y<N;++Y)for(int32 X=0;X<N;++X)
    {
        Open+=VisibleFromSelected(FVector(((X+.5)/N-.5)*OpeningLength,((Y+.5)/N-.5)*OpeningWidth,HoldDepth+CoamingHeight));
        const FVector P(((X+.5)/N-.5)*HoldLength,((Y+.5)/N-.5)*HoldWidth,0);
        const bool Through=(bMLX&&ShipHatch::ThroughOpening(Current.Settings,SensorLocalOrigin(true),P))||(bLivox&&ShipHatch::ThroughOpening(Current.Settings,SensorLocalOrigin(false),P));
        if(Through){++Potential;Floor+=VisibleFromSelected(P);}
    }
    Opening=100.*Open/(N*N);VisibleFloor=Potential?100.*Floor/Potential:0;FloorArea=double(Potential)/(N*N)*HoldLength*HoldWidth;
}
void AShipHatchSimulation::UpdateSensorPresentation()
{
    if(!SensorPresentation.Num())
    {
        for(int32 I=0;I<3;++I)
        {
            FActorSpawnParameters P;P.Owner=this;P.ObjectFlags|=RF_Transient;
            auto* A=GetWorld()->SpawnActor<AShipHatchPart>(AShipHatchPart::StaticClass(),GetActorTransform(),P);if(!A)return;
            A->Tags.Add(TEXT("ShipHatchPreviewSensor"));A->Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);A->Mesh->SetGenerateOverlapEvents(false);
            A->Mesh->SetCanEverAffectNavigation(false);A->Mesh->SetHiddenInSceneCapture(true);A->Mesh->SetCastShadow(false);SensorPresentation.Add(A);

        }
    }
    if(SensorPresentation.Num()!=3)return;
    const double H=HoldDepth+EffectiveMLXHeight();
    for(int32 I=0;I<3;++I)
    {
        auto* A=SensorPresentation[I].Get();TArray<FVector> V;TArray<int32> T;
        if(I<2)
        {
            const FVector O=SensorLocalOrigin(I==0);Box(V,T,O,FVector(I==0?1.4:.9,1.,.8));Box(V,T,O+FVector(0,0,-.48),FVector(.7,.7,.18));
            const bool Enabled=I==0?bMLX:bLivox,Selected=SelectedSensor==I||(SelectedSensor<0&&Enabled);
            A->SetColor(!Enabled?FLinearColor(.12,.14,.16):Selected?FLinearColor(1,.62,.025):FLinearColor(.04,.45,.65));
        }
        else
        {
            for(int32 Side:{-1,1})Box(V,T,FVector(0,Side*(OpeningWidth*.5+1),HoldDepth+EffectiveMLXHeight()*.5),FVector(.55,.55,EffectiveMLXHeight()));
            Box(V,T,FVector(0,0,H+.8),FVector(.55,OpeningWidth+2.6,.55));
            const double Low=SensorLocalOrigin(false).Z;Box(V,T,FVector(0,.6,(Low+H)*.5+.5),FVector(.18,.18,H-Low));
            A->SetColor(FLinearColor(.22,.30,.34));
        }
        TArray<FVector> Normals;Normals.Init(FVector::UpVector,V.Num());
        const auto* Existing=A->Mesh->GetProcMeshSection(0);
        if(Existing&&Existing->ProcVertexBuffer.Num()==V.Num())A->Mesh->UpdateMeshSection_LinearColor(0,V,Normals,TArray<FVector2D>(),TArray<FLinearColor>(),TArray<FProcMeshTangent>());
        else A->Mesh->CreateMeshSection_LinearColor(0,V,T,Normals,TArray<FVector2D>(),TArray<FLinearColor>(),TArray<FProcMeshTangent>(),false);
    }
}
bool AShipHatchSimulation::SelectSensorAt(FVector O,FVector D)
{
    if(!D.Normalize())return false;
    for(int32 I=0;I<2;++I)
    {
        const FVector P=GetActorTransform().TransformPosition(SensorLocalOrigin(I==0)*100),Delta=P-O;const double Along=FVector::DotProduct(Delta,D);
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
    for(auto A:SensorPresentation)if(A)
    {
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
    int32 Index=0;for(auto* A:{bMLX?MLX.Get():nullptr,bLivox?Livox.Get():nullptr})
    {
        const FLinearColor Color=Index++==0?FLinearColor(.1,.9,1):FLinearColor(1,.6,.06);if(!A)continue;
        const auto Frame=A->ScanComponent->GetLastFrameSnapshot();if(!Frame||!Frame->Points||!Frame->Points->Num())continue;
        const auto& Source=*Frame->Points;const FVector Origin=Frame->AcquisitionTransform.GetLocation();
        const int32 Stride=FMath::Max(1,Source.Num()/48),Offset=(ScanPhase*37)%Stride;
        for(int32 I=Offset;I<Source.Num()&&Lines.Num()<DisplayRayLimit;I+=Stride)
        {
            const auto& P=Source[I];if(!P.bHit)continue;
            Lines.Emplace(Origin,P.WorldLocation,Color,0.f,1.f,SDPG_World);Points.Emplace(P.WorldLocation,Color,5,0.f,SDPG_World);
        }
    }
    ScanDisplay->SetVisibility(true);ScanDisplay->UpdateBounds();ScanDisplay->MarkRenderStateDirty();
}

#include "ShipHatchSimulation.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "TimerManager.h"
#include "Engine/World.h"

UPrimitiveComponent* AShipHatchSimulation::CabPrimitive() const { return CabPresentation; }
void AShipHatchSimulation::BuildCabModel()
{
    if(!UnloaderPresentation)return;
    if(!CabPresentation)
    {
        CabPresentation=NewObject<UProceduralMeshComponent>(UnloaderPresentation,TEXT("IndependentCabCarriage"));
        CabPresentation->SetupAttachment(UnloaderPresentation->Mesh);CabPresentation->bUseComplexAsSimpleCollision=true;
        CabPresentation->SetCollisionProfileName(TEXT("BlockAll"));CabPresentation->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        CabPresentation->SetCastShadow(false);CabPresentation->SetCanEverAffectNavigation(false);CabPresentation->ComponentTags.Add(TEXT("ShipHatchCab"));CabPresentation->RegisterComponent();
    }
    const double Rail=HoldDepth+BoomHeight+1.2-(HoldDepth+CabHeight);
    uint32 Key=GetTypeHash(Rail);
    if(CabShapeKey!=Key)
    {
        CabShapeKey=Key;TArray<FVector> V,N;TArray<int32> T;TArray<FLinearColor> C;
        auto Box=[&](FVector Center,FVector Size,FLinearColor Color)
        {
            const int32 B=V.Num();for(int32 Z=0;Z<2;++Z)for(int32 Y=0;Y<2;++Y)for(int32 X=0;X<2;++X){V.Add((Center+FVector((X-.5)*Size.X,(Y-.5)*Size.Y,(Z-.5)*Size.Z))*100);C.Add(Color);}
            const int32 Idx[]={0,2,3,0,3,1,4,5,7,4,7,6,0,1,5,0,5,4,2,6,7,2,7,3,0,4,6,0,6,2,1,3,7,1,7,5};
            for(int32 I=0;I<36;I+=3)T.Append({B+Idx[I],B+Idx[I+2],B+Idx[I+1]});
        };
        const FLinearColor Paint(.025,.22,.12),Glass(.018,.05,.065),Steel(.12,.16,.14);
        Box(FVector(0,0,1.55),FVector(2.4,2.8,2.4),Paint);
        for(int Sign:{-1,1}){Box(FVector(Sign*1.205,0,1.9),FVector(.025,2.35,1.15),Glass);Box(FVector(0,Sign*1.405,1.9),FVector(2.05,.025,1.15),Glass);}
        Box(FVector(0,0,(2.75+Rail)*.5),FVector(.3,.3,Rail-2.75),Steel);
        Box(FVector(0,0,Rail+.2),FVector(1.4,2.2,.4),Paint);
        N.Init(FVector::ZeroVector,V.Num());for(int32 I=0;I<T.Num();I+=3){const FVector Normal=FVector::CrossProduct(V[T[I+2]]-V[T[I]],V[T[I+1]]-V[T[I]]);for(int J=0;J<3;++J)N[T[I+J]]+=Normal;}for(auto& Normal:N)Normal.Normalize();
        CabPresentation->CreateMeshSection_LinearColor(0,V,T,N,TArray<FVector2D>(),C,TArray<FProcMeshTangent>(),true);
        if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Hatch/Materials/M_HatchCargo.M_HatchCargo")))
        {auto* MID=UMaterialInstanceDynamic::Create(Base,UnloaderPresentation);MID->SetScalarParameterValue(TEXT("Hardware"),1);CabPresentation->SetMaterial(0,MID);}
    }
    CabPresentation->SetRelativeLocation(FVector(CabX,CabY,HoldDepth+CabHeight)*100);CabPresentation->SetVisibility(bShowUnloader);++CabPoseRevision;
    for(int32 I=1;I<3;++I)
    {
        if(auto* A=SensorAt(I))A->AttachToComponent(CabPresentation,FAttachmentTransformRules::KeepWorldTransform);
        if(SensorPresentation.IsValidIndex(I)&&SensorPresentation[I])SensorPresentation[I]->AttachToComponent(CabPresentation,FAttachmentTransformRules::KeepWorldTransform);
    }
}
bool AShipHatchSimulation::RequestCabPosition(double TargetY)
{
    if(bEnding||bGenerating||bGrabWorkActive||!bFieldLayout||!FMath::IsFinite(TargetY)||TargetY<CabTravelMinY()||TargetY>CabTravelMaxY())return false;
    CabTargetY=TargetY;++CabCommandRevision;bCabMoving=!FMath::IsNearlyEqual(CabY,TargetY,.001);
    GetWorldTimerManager().ClearTimer(CabTravelTimer);
    if(bCabMoving){CabLastUpdate=GetWorld()->GetTimeSeconds();GetWorldTimerManager().SetTimer(CabTravelTimer,this,&AShipHatchSimulation::UpdateCabTravel,.05f,true);}
    return true;
}
void AShipHatchSimulation::StopCab()
{
    if(!bCabMoving)return;CabTargetY=CabY;bCabMoving=false;++CabCommandRevision;GetWorldTimerManager().ClearTimer(CabTravelTimer);
    ++SurfaceRevision;OnSurfaceChanged.Broadcast();
}
void AShipHatchSimulation::UpdateCabTravel()
{
    if(bEnding||!CabPresentation){StopCab();return;}
    const double Now=GetWorld()->GetTimeSeconds(),Step=FMath::Max(0.,Now-CabLastUpdate)*CabTravelMetersPerSecond;CabLastUpdate=Now;
    CabY=FMath::FInterpConstantTo(CabY,CabTargetY,1.,Step);CabPresentation->SetRelativeLocation(FVector(CabX,CabY,HoldDepth+CabHeight)*100);++CabPoseRevision;
    LastGeometryEdit=FPlatformTime::Seconds();
    if(FMath::IsNearlyEqual(CabY,CabTargetY,.001)){CabY=CabTargetY;bCabMoving=false;GetWorldTimerManager().ClearTimer(CabTravelTimer);++SurfaceRevision;OnSurfaceChanged.Broadcast();}
}
bool AShipHatchSimulation::BeginGrabWork()
{
    if(bEnding||bGenerating||bGrabWorkActive||!IsCabReady()||!FMath::IsNearlyZero(CabY,.001))return false;
    bGrabWorkActive=true;return true;
}

#include "CoilLoadingSimulation.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSurfaceResponseComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "Components/TextRenderComponent.h"

ACoilLoadingBody::ACoilLoadingBody()
{
    PrimaryActorTick.bCanEverTick=false;Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoilCollision"));SetRootComponent(Body);
    Body->SetMobility(EComponentMobility::Movable);Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Detail=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoilVisualDetails"));Detail->SetupAttachment(Body);Detail->SetCollisionEnabled(ECollisionEnabled::NoCollision);Detail->SetCastShadow(false);Detail->SetCullDistance(4500);Detail->ComponentTags.Add(TEXT("CoilVisualOnly"));
    Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("CoilIdLabel"));Label->SetupAttachment(Body);Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);Label->SetCastShadow(false);Label->SetCullDistance(1800);Label->SetWorldSize(5.5f);Label->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);Label->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);Label->SetTextRenderColor(FColor::Black);Label->ComponentTags.Add(TEXT("CoilVisualOnly"));
    auto* Optical=CreateDefaultSubobject<UVirtualLidarSurfaceResponseComponent>(TEXT("CoilOptical"));Optical->Reflectivity940Nm=.35;Optical->bTwoSided=true;Optical->bUseGpuReflectivityOverride=true;
}
void ACoilLoadingBody::Configure(UStaticMesh* Mesh,UMaterialInterface* M,const CoilLoading::FSpec& S)
{
    Body->SetStaticMesh(Mesh);Body->SetMaterial(0,M);Body->SetMassOverrideInKg(NAME_None,S.GrossMass()*1000,true);
    ContactMaterial=NewObject<UPhysicalMaterial>(this);ContactMaterial->Friction=S.Mu;ContactMaterial->StaticFriction=S.Mu;ContactMaterial->Restitution=0;
    ContactMaterial->bOverrideFrictionCombineMode=true;ContactMaterial->FrictionCombineMode=EFrictionCombineMode::Min;Body->SetPhysMaterialOverride(ContactMaterial);
    Body->SetLinearDamping(.35);Body->SetAngularDamping(.5);Body->BodyInstance.PositionSolverIterationCount=12;Body->BodyInstance.VelocitySolverIterationCount=4;
    Tags={TEXT("Coil"),FName(*(TEXT("CoilId=")+S.CoilId)),S.Type==CoilLoading::ECoilType::ColdRolled?FName(TEXT("CoilType=CR")):FName(TEXT("CoilType=HR"))};
}
UMaterialInterface* ACoilLoadingSimulation::Material(FLinearColor Color)
{
    auto* Base=StructureMaterialAsset.Get();if(!Base)Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurfacePlain.M_SlabSurfacePlain"));
    auto* M=UMaterialInstanceDynamic::Create(Base,this);M->SetVectorParameterValue(TEXT("ColdColor"),Color);M->SetScalarParameterValue(TEXT("Metallic"),.35);M->SetScalarParameterValue(TEXT("Roughness"),.62);M->SetScalarParameterValue(TEXT("Hotness"),0);Materials.Add(M);return M;
}
UStaticMesh* ACoilLoadingSimulation::CoilMesh(const CoilLoading::FSpec& S)
{
    const FString Key=FString::Printf(TEXT("%d/%.6f/%.6f/%.6f"),int32(S.Type),S.OuterDiameter(),S.ClearBore(),S.OverallWidth());
    if(auto* Found=SharedMeshes.Find(Key))return *Found;
    FMeshDescription D;FStaticMeshAttributes Attr(D);Attr.Register();auto Positions=Attr.GetVertexPositions();auto Normals=Attr.GetVertexInstanceNormals();auto Colors=Attr.GetVertexInstanceColors();auto UVs=Attr.GetVertexInstanceUVs();UVs.SetNumChannels(1);
    const auto Group=D.CreatePolygonGroup();const int32 Segments=96;TArray<TArray<FVector>> Convex;
    auto Quad=[&](FVector A,FVector B,FVector C,FVector E)
    {
        const FVector N=-FVector::CrossProduct(B-A,C-A).GetSafeNormal();TArray<FVertexInstanceID> VI;
        const bool Cap=FMath::Abs(A.X-B.X)<.001&&FMath::Abs(A.X-C.X)<.001;
        for(const FVector& P:{A,B,C,E}){auto V=D.CreateVertex();Positions[V]=FVector3f(P);auto I=D.CreateVertexInstance(V);FVector Normal=Cap?N:FVector(0,P.Y,P.Z).GetSafeNormal();if(!Cap&&FVector::DotProduct(Normal,N)<0)Normal=-Normal;Normals[I]=FVector3f(Normal);Colors[I]=FVector4f(Cap?1:0,0,0,1);UVs.Set(I,0,FVector2f(P.Y*.01,P.Z*.01));VI.Add(I);}D.CreatePolygon(Group,VI);
    };
    for(int32 I=0;I<Segments;++I)
    {
        const double A=2*PI*I/Segments,B=2*PI*(I+1)/Segments;TArray<FVector> W;
        for(double X:{-S.OverallWidth()*50,S.OverallWidth()*50})for(double R:{S.ClearBore()*50,S.OuterDiameter()*50})for(double Angle:{A,B})W.Add(FVector(X,R*FMath::Cos(Angle),R*FMath::Sin(Angle)));
        Quad(W[2],W[6],W[7],W[3]);Quad(W[0],W[1],W[5],W[4]);Quad(W[0],W[2],W[3],W[1]);Quad(W[4],W[5],W[7],W[6]);
    }
    for(int32 I=0;I<32;++I){TArray<FVector> W;for(double X:{-S.OverallWidth()*50,S.OverallWidth()*50})for(double R:{S.ClearBore()*50,S.OuterDiameter()*50})for(double A:{I*2*PI/32,(I+1)*2*PI/32})W.Add(FVector(X,R*FMath::Cos(A),R*FMath::Sin(A)));Convex.Add(MoveTemp(W));}
    auto* Mesh=NewObject<UStaticMesh>(this,NAME_None,RF_Transient);Mesh->GetStaticMaterials().Add(FStaticMaterial());
    UStaticMesh::FBuildMeshDescriptionsParams Params;Params.bBuildSimpleCollision=false;Params.bFastBuild=true;Params.bAllowCpuAccess=true;
    Mesh->BuildFromMeshDescriptions({&D},Params);Mesh->CreateBodySetup();auto* Body=Mesh->GetBodySetup();Body->CollisionTraceFlag=CTF_UseSimpleAsComplex;
    for(const auto& W:Convex){FKConvexElem C;C.VertexData=W;C.UpdateElemBox();Body->AggGeom.ConvexElems.Add(C);}Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();
    SharedMeshes.Add(Key,Mesh);return Mesh;
}
ACoilLoadingBody* ACoilLoadingSimulation::SpawnCoil(const CoilLoading::FSpec& S,FVector Center)
{
    FActorSpawnParameters P;P.Owner=this;P.ObjectFlags|=RF_Transient;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* A=GetWorld()->SpawnActor<ACoilLoadingBody>(GetActorTransform().TransformPosition(Center*100),GetActorRotation(),P);
    if(A){A->Configure(CoilMesh(S),Materials[4],S);ApplyCoilAppearance(A,S);}return A;
}
UStaticMeshComponent* ACoilLoadingSimulation::Box(FVector C,FVector Size,int32 Color,bool Collision,FRotator Rotation)
{
    auto* M=NewObject<UStaticMeshComponent>(this);M->SetupAttachment(GetRootComponent());M->SetMobility(EComponentMobility::Movable);
    M->SetStaticMesh(StructureCubeAsset?StructureCubeAsset.Get():LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));M->SetMaterial(0,Materials[Color]);
    M->SetCollisionProfileName(Collision?TEXT("BlockAll"):TEXT("NoCollision"));M->SetRelativeLocation(C*100);M->SetRelativeScale3D(Size);M->SetRelativeRotation(Rotation);M->RegisterComponent();Structure.Add(M);return M;
}
void ACoilLoadingSimulation::BuildScene()
{
    Material(FLinearColor(.035,.045,.05));Material(FLinearColor(.18,.19,.185));Material(FLinearColor(.92,.58,.025));Material(FLinearColor(.2,.1,.04));Material(FLinearColor(.29,.34,.38));Material(FLinearColor(.07,.28,.12));Material(FLinearColor(.8,.12,.025));Material(FLinearColor(.29,.48,.53));Material(FLinearColor(.53,.57,.55));
    if(auto* Water=Cast<UMaterialInstanceDynamic>(Materials[7])){Water->SetScalarParameterValue(TEXT("Metallic"),0);Water->SetScalarParameterValue(TEXT("Roughness"),.82);}
    const auto& S=Settings;const double L=S.HoldLength,W=S.HoldWidth,H=S.WeatherDeckZ();
    // Four N-class box holds; the aft working hold is centered at local X=0.
    for(int32 Hold=0;Hold<S.HoldCount;++Hold)
    {
        const double X=S.HoldCenterX(Hold),HL=S.HoldLengthAt(Hold);
        Box({X,0,-.2},{HL,W,.4},1);Box({X,-W*.5-.25,H*.5},{HL,.5,H},0);Box({X,W*.5+.25,H*.5},{HL,.5,H},0);
        for(double End:{-1.,1.})Box({X+End*(HL*.5+.25),0,H*.5},{.5,W+1,H},0);
        for(double Side:{-1.,1.})Box({X,Side*(W*.5+.25),H+S.Coaming*.5},{HL+.8,.5,S.Coaming},8);
        if(Hold>0){auto* Cover=Box({X,0,H+S.Coaming+.15},{HL,W,.3},8);Cover->ComponentTags.Add(TEXT("CoilClosedHatchCover"));}
    }
    const double VesselX=S.VesselCenterX();
    Box({VesselX,0,-1.2},{S.LOA,S.Beam,2},0);
    Box({VesselX,QuayOffset(180),QuayZ()-.6},{900,360,1.2},1);
    // Water is display only, has no measurement/collision.
    for(const FVector C:{FVector(VesselX,-S.QuaySide*(S.Beam*.5+6000),S.Draft-S.KeelToTanktop-.1),FVector(VesselX-S.LOA*.5-6000,0,S.Draft-S.KeelToTanktop-.1),FVector(VesselX+S.LOA*.5+6000,0,S.Draft-S.KeelToTanktop-.1)})
        {auto* Water=Box(C,C.Y==0?FVector(12000,24000,.05):FVector(24000,12000,.05),7,false);Water->SetCastShadow(false);Water->ComponentTags.Add(TEXT("CoilDisplayOnly"));}
    Box({VesselX,QuayEdgeY()-S.QuaySide*S.BerthGap*.5,S.Draft-S.KeelToTanktop-.1},{S.LOA,S.BerthGap,.05},7,false)->ComponentTags.Add(TEXT("CoilDisplayOnly"));
    const double RailY=QuayOffset(S.RailSetback),Q=QuayZ(),CraneX=-8,Side=S.QuaySide;
    const auto CraneBox=[&](FVector C,FVector Size,int32 Color,bool Collision=true){auto* M=Box(C,Size,Color,Collision);CraneParts.Add(M);CraneStarts.Add(C*100);return M;};
    for(double Y:{RailY,RailY+Side*S.RailGauge})
    {
        Box({VesselX,Y,Q+.08},{S.LOA,.16,.16},4);
        for(double X:{CraneX-4,CraneX+4})
        {CraneBox({X,Y,Q+14},{.85,.85,28},2);CraneBox({X,Y,Q+.6},{2,1.2,1.2},0);}
    }
    for(double X:{CraneX-4,CraneX+4}){CraneBox({X,S.RailCenterY(),Q+20},{1,S.RailGauge+1,1},2);CraneBox({X,RailY-Side*(30-S.RailGauge)*.5,Q+28},{1,31+S.RailGauge,1.2},2);}
    CraneBox({CraneX,RailY-Side*30,Q+28},{9,1,1.2},2);CraneBox({CraneX,RailY+Side*S.RailGauge,Q+28},{9,1,1.2},2);
    for(double Offset=-S.RailGauge+2;Offset<30;Offset+=4)CraneBox({CraneX,RailY-Side*Offset,Q+28},{7,.2,.2},2,false);

    Box({-8,QuayOffset(8),Q+.5},{2.5,2.3,1},2);for(double Y:{-.7,.7})Box({-8,QuayOffset(8)+Y,Q+1.05},{2,.25,.3},2,true,FRotator(0,0,Y<0?-35:35));
    // ref3: open-front coil shed, individual yellow cradles and visible coil eyes.
    const double ShedY=ShedCenterY();
    Box({2,ShedY,Q+6},{32,10,.3},5);Box({2,ShedY+Side*5,Q+3},{32,.2,6},5);
    for(double X=-12;X<=16;X+=7)for(double Y:{ShedY-4.8,ShedY+4.8})Box({X,Y,Q+3},{.25,.25,6},2);
    for(int32 I=0;I<8;++I)
    {
        const FVector C(-10+I*3.2,ShedY,Q+1.35);auto Spec=NextSpec;Spec.CoilId=FString::Printf(TEXT("STOCK-%03d"),I+1);auto* A=SpawnCoil(Spec,C);if(A){Stock.Add(A);A->AttachToActor(this,FAttachmentTransformRules::KeepWorldTransform);}
        for(double Y:{-.8,.8})Box(C+FVector(0,Y,-.85),{1.8,.35,.35},2);
    }
    BuildQuayPresentation();
    BuiltSceneSettings=Settings;CurrentHook=PickupPosition()+FVector(0,0,3);++GeometryRevision;
}
void ACoilLoadingSimulation::BuildSupports()
{
    if(SupportedPlacementCount>Placements.Num())SupportedPlacementCount=0;
    if(SupportedPlacementCount==0){for(auto M:PlanSupports){Structure.Remove(M);if(M)M->DestroyComponent();}PlanSupports.Reset();}
    for(int32 Index=SupportedPlacementCount;Index<Placements.Num();++Index)if(const auto& P=Placements[Index];P.Tier==1)
    {
        if(P.bDunnage)for(int32 J=0;J<Settings.DunnageLines;++J)
        {auto* M=Box({P.Center.X+(J-(Settings.DunnageLines-1)*.5)*.45,P.Center.Y,Settings.DunnageThickness*.5},{Settings.DunnageWidth,Settings.ContactLength,Settings.DunnageThickness},3);PlanSupports.Add(M);}
        if(P.bChocks)for(double Side:{-1.,1.})
        {auto* M=Box(P.Center+FVector(0,Side*P.Spec.OD*.44,-P.Spec.OD*.35),{P.Spec.Width*.7,.2,.2},2,true,FRotator(0,0,Side*35));PlanSupports.Add(M);}
    }
    SupportedPlacementCount=Placements.Num();
}
void ACoilLoadingSimulation::RebuildBatches()
{
    TMap<UStaticMesh*,UInstancedStaticMeshComponent*> Groups;
    for(auto B:Batches)if(B){B->ClearInstances();Groups.Add(B->GetStaticMesh(),B);}
    for(auto A:Bodies)if(IsValid(A))
    {
        if(A->Body->IsSimulatingPhysics()){A->Body->SetHiddenInGame(false);A->Detail->SetHiddenInGame(false);continue;}
        for(auto* Component:{A->Body.Get(),A->Detail.Get()})
        {
            UStaticMesh* Mesh=Component->GetStaticMesh();if(!Mesh)continue;auto*& Group=Groups.FindOrAdd(Mesh);
            if(!Group){Group=NewObject<UInstancedStaticMeshComponent>(this);Group->SetupAttachment(GetRootComponent());Group->SetStaticMesh(Mesh);for(int32 I=0;I<Component->GetNumMaterials();++I)Group->SetMaterial(I,Component->GetMaterial(I));Group->SetCollisionEnabled(ECollisionEnabled::NoCollision);if(Component==A->Detail)Group->SetCullDistances(3000,4500);Group->RegisterComponent();Group->ComponentTags.Add(TEXT("CoilSettledBatch"));Batches.Add(Group);}
            Group->AddInstance(A->GetActorTransform(),true);Component->SetHiddenInGame(true);
        }
    }
}
void ACoilLoadingSimulation::RestoreSceneGeometry()
{
    for(auto M:Structure)if(M)M->DestroyComponent();Structure.Reset();PlanSupports.Reset();CraneParts.Reset();CraneStarts.Reset();TrolleyDetails.Reset();HookDetails.Reset();PresentationBatches.Reset();PresentationMeshes.Reset();RailX=-8;SupportedPlacementCount=0;
    TInlineComponentArray<UTextRenderComponent*> Signs(this);for(auto* T:Signs)if(T->ComponentHasTag(TEXT("CoilQuayPresentation")))T->DestroyComponent();
    for(auto A:Stock)if(A)A->Destroy();Stock.Reset();Materials.Reset();BuildScene();
    BuildSupports();if(WaitingCoil)WaitingCoil->SetActorLocation(GetActorTransform().TransformPosition(PickupPosition()*100));
    if(Lidar)
    {
        FVirtualSensorEditableState S;FString E;Lidar->ReadEditableState(S);
        S.ActorTransform=FTransform(GetActorQuat()*FRotator(-72,112,0).Quaternion(),GetActorTransform().TransformPosition(FVector(-Settings.HoldLength*.5+4,-Settings.HoldWidth*.5+3,Settings.HoldHeight+8)*100));Lidar->ApplyEditableState(S,E);
    }
}

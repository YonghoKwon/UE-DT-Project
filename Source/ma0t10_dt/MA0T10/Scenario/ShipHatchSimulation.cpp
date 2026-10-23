#include "ShipHatchSimulation.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSurfaceResponseComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorEditableTypes.h"
#include "Async/Async.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/TextureCube.h"
#include "Engine/SkyLight.h"
#include "EngineUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "HAL/LowLevelMemTracker.h"

namespace
{
void Box(TArray<FVector>& V,TArray<int32>& T,FVector Center,FVector Size)
{
    const int32 B=V.Num();
    for(int32 Z=0;Z<2;++Z)for(int32 Y=0;Y<2;++Y)for(int32 X=0;X<2;++X)
        V.Add((Center+FVector((X-.5)*Size.X,(Y-.5)*Size.Y,(Z-.5)*Size.Z))*100);
    const int32 Indices[]={0,2,3,0,3,1,4,5,7,4,7,6,0,1,5,0,5,4,2,6,7,2,7,3,0,4,6,0,6,2,1,3,7,1,7,5};
    // UE procedural meshes face clockwise. The previous outward-CCW box
    // culled its tank-top at Z=0, revealing parked empty cargo at Z=-1cm.
    for(int32 I=0;I<UE_ARRAY_COUNT(Indices);I+=3){T.Add(B+Indices[I]);T.Add(B+Indices[I+2]);T.Add(B+Indices[I+1]);}
}
void Mesh(UProceduralMeshComponent* M,const TArray<FVector>& V,const TArray<int32>& T,TArray<FVector>* Scratch=nullptr,const TArray<FLinearColor>& Colors=TArray<FLinearColor>())
{
    TArray<FVector> LocalNormals;auto& N=Scratch?*Scratch:LocalNormals;N.Init(FVector::ZeroVector,V.Num());
    for(int32 I=0;I<T.Num();I+=3)
    {const FVector Normal=FVector::CrossProduct(V[T[I+2]]-V[T[I]],V[T[I+1]]-V[T[I]]);for(int32 J=0;J<3;++J)N[T[I+J]]+=Normal;}
    for(auto& Normal:N)Normal.Normalize();
    const auto* Existing=M->GetProcMeshSection(0);
    bool SameTopology=Existing&&Existing->ProcVertexBuffer.Num()==V.Num()&&Existing->ProcIndexBuffer.Num()==T.Num();
    if(SameTopology)for(int32 I=0;I<T.Num();++I)if(Existing->ProcIndexBuffer[I]!=uint32(T[I])){SameTopology=false;break;}
    if(SameTopology)
        M->UpdateMeshSection_LinearColor(0,V,N,TArray<FVector2D>(),Colors,TArray<FProcMeshTangent>());
    else M->CreateMeshSection_LinearColor(0,V,T,N,TArray<FVector2D>(),Colors,TArray<FProcMeshTangent>(),true);
}
}
AShipHatchPart::AShipHatchPart()
{
    PrimaryActorTick.bCanEverTick=false;
    Mesh=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Geometry"));SetRootComponent(Mesh);
    Mesh->bUseComplexAsSimpleCollision=true;Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Optical=CreateDefaultSubobject<UVirtualLidarSurfaceResponseComponent>(TEXT("Optical"));Optical->bTwoSided=true;
}
void AShipHatchPart::SetColor(const FLinearColor& Color)
{
    auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurfacePlain.M_SlabSurfacePlain"));
    if(!Base)return;
    auto* MID=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
    if(!MID){MID=UMaterialInstanceDynamic::Create(Base,this);Mesh->SetMaterial(0,MID);}
    MID->SetVectorParameterValue(TEXT("ColdColor"),Color);MID->SetScalarParameterValue(TEXT("Metallic"),0);
    MID->SetScalarParameterValue(TEXT("Roughness"),.95f);MID->SetScalarParameterValue(TEXT("Hotness"),0);
}
AShipHatchSimulation::AShipHatchSimulation(){PrimaryActorTick.bCanEverTick=false;SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));}
ShipHatch::FSettings AShipHatchSimulation::Settings() const
{
    ShipHatch::FSettings S;S.Length=HoldLength;S.Width=HoldWidth;S.Depth=HoldDepth;S.OpeningLength=OpeningLength;
    S.OpeningWidth=OpeningWidth;S.Coaming=CoamingHeight;S.SensorAboveDeck=SensorAboveDeck;
    S.Seed=Seed;S.Cargo=bCoal?ShipHatch::ECargo::Coal:ShipHatch::ECargo::IronOre;S.FillMin=FillMin;S.FillMax=FillMax;return S;
}
void AShipHatchSimulation::BeginPlay()
{
    Super::BeginPlay();
    // Upgrade only the old hatch fixture rig defaults in memory. No map save.
    if(bRealScale&&bFieldLayout)
    {
        if(FMath::IsNearlyEqual(BerthGap,3.5))BerthGap=2;
        // Two flat side-rolling half panels need one full aperture width on
        // each side. Keep the aperture clear and15cm inside a45m hull.
        if(FMath::IsNearlyEqual(OpeningWidth,23.))OpeningWidth=FMath::Min(OpeningWidth,VesselBeam*.5-.3);
        MigrateLegacyCabDimensions();
        if(FMath::IsNearlyZero(CabX))CabX=4.5;
        if(FMath::IsNearlyEqual(CabY,-2.))CabY=0;
        CabTargetY=CabY;
    }
    for(TActorIterator<ADirectionalLight> I(GetWorld());I;++I){KeyLight=*I;break;}
    if(KeyLight)
    {
        auto* Light=CastChecked<UDirectionalLightComponent>(KeyLight->GetLightComponent());OriginalLightRotation=KeyLight->GetActorRotation();OriginalLightIntensity=Light->Intensity;OriginalLightMobility=uint8(Light->Mobility);bOriginalLightShadows=Light->CastShadows;
        bOriginalAtmosphereSun=Light->bAtmosphereSunLight;if(bRealScale)Light->SetAtmosphereSunLight(true);
        Light->SetMobility(EComponentMobility::Movable);
        // Repeated height-field edits must not grow virtual shadow-map residency.
        // The cargo's bounded vertex shading supplies the cavity/AO cue instead.
        Light->SetCastShadows(false);
    }
    FillLight=NewObject<UPointLightComponent>(this);FillLight->SetupAttachment(GetRootComponent());FillLight->SetMobility(EComponentMobility::Movable);
    FillLight->SetCastShadows(false);FillLight->SetIntensityUnits(ELightUnits::Candelas);FillLight->SetIntensity(1000);FillLight->SetAttenuationRadius(6500);FillLight->RegisterComponent();
    FillLight->SetRelativeLocation(FVector(0,0,(HoldDepth+12)*100));SetLighting(-35,43,4.5,true);Regenerate(false);
    // Visible outdoor background, with no collision or sensor readback. The
    // player's point-only view suppresses this show flag locally in its view.
    if(bRealScale){OutdoorSky=NewObject<USkyAtmosphereComponent>(this,TEXT("HatchOutdoorSky"));OutdoorSky->SetupAttachment(GetRootComponent());OutdoorSky->RegisterComponent();}
    // The fixture has no sky geometry, so its captured skylight is black.
    // A hatch-owned, one-time engine cubemap supplies outdoor diffuse fill on
    // the portal's back faces without recapture, shadows or a new asset.
    if(bRealScale)if(auto* Cube=LoadObject<UTextureCube>(nullptr,TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap")))
    {
        // A later black scene capture from the fixture must not replace the
        // specified outdoor cube. Keep and restore the fixture's visibility.
        for(TActorIterator<ASkyLight> I(GetWorld());I;++I){FixtureSky=I->GetLightComponent();bFixtureSkyVisible=FixtureSky->IsVisible();FixtureSky->SetVisibility(false);break;}
        OutdoorAmbient=NewObject<USkyLightComponent>(this,TEXT("HatchOutdoorAmbient"));OutdoorAmbient->SetupAttachment(GetRootComponent());OutdoorAmbient->SetMobility(EComponentMobility::Movable);
        OutdoorAmbient->SourceType=SLS_SpecifiedCubemap;OutdoorAmbient->Cubemap=Cube;OutdoorAmbient->bRealTimeCapture=false;OutdoorAmbient->bLowerHemisphereIsBlack=false;OutdoorAmbient->SetIntensity(1.35f);OutdoorAmbient->CastShadows=false;OutdoorAmbient->RegisterComponent();
    }
}
void AShipHatchSimulation::SetLighting(double Azimuth,double Elevation,double Intensity,bool Fill)
{
    if(!FMath::IsFinite(Azimuth)||!FMath::IsFinite(Elevation)||!FMath::IsFinite(Intensity))return;
    LightAzimuth=FMath::Clamp(Azimuth,-180.,180.);LightElevation=FMath::Clamp(Elevation,5.,90.);LightIntensity=FMath::Clamp(Intensity,0.,12.);bFillLight=Fill;
    if(KeyLight){KeyLight->SetActorRotation(FRotator(-LightElevation,LightAzimuth,0));KeyLight->GetLightComponent()->SetIntensity(LightIntensity);}
    if(FillLight)FillLight->SetVisibility(Fill);
}
bool AShipHatchSimulation::Regenerate(bool Randomize)
{
    if(bEnding)return false;
    StopCab();bGrabWorkActive=false;
    if(Randomize){int32 Next=int32(FPlatformTime::Cycles64()&0x7fffffff);Seed=Next==Seed?Seed^0x41c64e6d:Next;}
    Pending=Settings();if(!Pending.Validate(Status))return false;
    if(bRealScale&&bFieldLayout&&!ValidateSceneScale(Status))return false;
    bPending=true;
    for(int32 I=0;I<3;++I)if(auto* A=SensorAt(I))A->StopSensor();
    if(!bGenerating)StartGeneration();
    return true;
}
void AShipHatchSimulation::StartGeneration()
{
    bGenerating=true;bPending=false;Status=TEXT("적재 상태 준비 중 · 이전 측정 무효화");
    const auto Copy=Pending;
    Generation=Async(EAsyncExecution::ThreadPool,[Copy,Heights=MoveTemp(RecycledHeights),Shape=MoveTemp(GenerationShape)]() mutable
    {FGenerationBuffers R;R.Surface.Heights=MoveTemp(Heights);R.Shape=MoveTemp(Shape);ShipHatch::GenerateInto(Copy,R.Surface,R.Shape);return R;});
    GetWorldTimerManager().SetTimer(PollTimer,this,&AShipHatchSimulation::PollGeneration,.02f,true);
}
void AShipHatchSimulation::PollGeneration()
{
    if(!Generation.IsReady())return;
    auto Result=Generation.Get();
    GenerationShape=MoveTemp(Result.Shape);
    if(bPending){RecycledHeights=MoveTemp(Result.Surface.Heights);StartGeneration();return;}
    GetWorldTimerManager().ClearTimer(PollTimer);
    ApplySurface(MoveTemp(Result.Surface));
    FinishGeneration();
}
void AShipHatchSimulation::MigrateLegacyCabDimensions()
{
    // Only the original fixture10/8 defaults are migrated.24/16 and all
    // other explicitly configured dimensions remain subject to validation.
    if(FMath::IsNearlyEqual(BoomHeight,10.))
    {BoomHeight=QuayZ()+25-HoldDepth-1.05;UE_LOG(LogTemp,Display,TEXT("[HatchFixtureMigration] BoomHeight10 -> %.3fm"),BoomHeight);}
    if(FMath::IsNearlyEqual(CabHeight,8.))
    {CabHeight=BoomHeight-2.6;UE_LOG(LogTemp,Display,TEXT("[HatchFixtureMigration] CabHeight8 -> %.3fm"),CabHeight);}
}
bool AShipHatchSimulation::FinishGeneration()
{
    // Geometry/sensor setup must finish before admitting the cab command.
    // RequestCabPosition rejects a still-generating actor by design.
    bGenerating=false;
    const bool Centered=!bFieldLayout||RequestCabPosition(TargetHatchCenterY());
    if(!Centered)
    {
        Status=TEXT("적재 준비 완료 · 운전실 중앙 이동 실패 · 붐 범위/작업 상태 확인");
        UE_LOG(LogTemp,Warning,TEXT("[HatchCabCentering] generation%llu targetY=%.3fm rejected (ending=%d work=%d)"),CompletedGenerations,TargetHatchCenterY(),bEnding,bGrabWorkActive);
    }
    else Status=IsCabMoving()?TEXT("적재 준비 완료 · 운전실 중앙 이동 중 · 그랩 대기"):TEXT("측정 중 · 가려진 영역은 측정 불가");
    OnSurfaceChanged.Broadcast();return Centered;
}
void AShipHatchSimulation::BuildStructure()
{
    for(auto A:Structure)if(IsValid(A))A->Destroy();Structure.Reset();
    const auto S=Current.Settings;
    auto Part=[&](FName Label,FLinearColor Color,TFunction<void(TArray<FVector>&,TArray<int32>&)> Build)
    {
        FActorSpawnParameters P;P.Owner=this;P.ObjectFlags|=RF_Transient;
        auto* A=GetWorld()->SpawnActor<AShipHatchPart>(AShipHatchPart::StaticClass(),GetActorTransform(),P);
        if(!A)return;A->Tags.Add(Label);A->SetColor(Color);Structure.Add(A);
        TArray<FVector> V;TArray<int32>T;Build(V,T);Mesh(A->Mesh,V,T);
        if(Label==TEXT("HatchCoaming"))
        {
            if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Hatch/Materials/M_HatchCargo.M_HatchCargo")))
            {
                auto* MID=UMaterialInstanceDynamic::Create(Base,A);MID->SetScalarParameterValue(TEXT("Hardware"),1);
                MID->SetScalarParameterValue(TEXT("WeatheredCoaming"),1);MID->SetVectorParameterValue(TEXT("PaintTint"),FLinearColor(.42,.085,.055));
                const auto Pose=GetActorTransform();MID->SetVectorParameterValue(TEXT("PaintOrigin"),FLinearColor(Pose.GetLocation()));
                MID->SetVectorParameterValue(TEXT("PaintX"),FLinearColor(Pose.GetUnitAxis(EAxis::X)));MID->SetVectorParameterValue(TEXT("PaintY"),FLinearColor(Pose.GetUnitAxis(EAxis::Y)));MID->SetVectorParameterValue(TEXT("PaintUp"),FLinearColor(Pose.GetUnitAxis(EAxis::Z)));
                MID->SetVectorParameterValue(TEXT("HoldBounds"),FLinearColor(S.OpeningLength*.5,S.OpeningWidth*.5,S.Depth));A->Mesh->SetMaterial(0,MID);
            }
        }
        if(Label==TEXT("HatchHold"))
        {
            if(auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Hatch/Materials/M_HatchCargo.M_HatchCargo")))
            {auto* MID=UMaterialInstanceDynamic::Create(Base,A);MID->SetScalarParameterValue(TEXT("TankTop"),1);A->Mesh->SetMaterial(0,MID);}
            BuildCorrugatedWalls(A);
        }
    };
    Part(TEXT("HatchHold"),FLinearColor(.20,.28,.32),[&](TArray<FVector>& V,TArray<int32>& T)
    {
        Box(V,T,FVector(0,0,-.15),FVector(S.Length+.6,S.Width+.6,.3));
        for(int32 Sign:{-1,1})
        {Box(V,T,FVector(Sign*(S.Length+.3)*.5,0,(S.Depth-.3)*.5),FVector(.3,S.Width+.6,S.Depth-.3));
         Box(V,T,FVector(0,Sign*(S.Width+.3)*.5,(S.Depth-.3)*.5),FVector(S.Length,.3,S.Depth-.3));}
    });
    Part(TEXT("HatchOutside"),FLinearColor(.16,.17,.17),[&](TArray<FVector>& V,TArray<int32>& T)
    {
        const double L=S.Length+8,W=bRealScale?VesselBeam:S.Width+8;
        for(int32 Sign:{-1,1})
        {Box(V,T,FVector(Sign*(L+S.OpeningLength)*.25,0,S.Depth-.15),FVector((L-S.OpeningLength)*.5,W,.3));
         Box(V,T,FVector(0,Sign*(W+S.OpeningWidth)*.25,S.Depth-.15),FVector(S.OpeningLength,(W-S.OpeningWidth)*.5,.3));}
    });
    Part(TEXT("HatchCoaming"),FLinearColor(.38,.025,.015),[&](TArray<FVector>& V,TArray<int32>& T)
    {
        for(int32 Sign:{-1,1})
        {Box(V,T,FVector(Sign*(S.OpeningLength+.3)*.5,0,S.Depth+S.Coaming*.5),FVector(.3,S.OpeningWidth+.6,S.Coaming));
         Box(V,T,FVector(0,Sign*(S.OpeningWidth+.3)*.5,S.Depth+S.Coaming*.5),FVector(S.OpeningLength,.3,S.Coaming));}
    });
}
void AShipHatchSimulation::ApplySurface(ShipHatch::FSurface&& Result)
{
    const double Start=FPlatformTime::Seconds();
    const bool Rebuild=!Current.Heights.Num()||Current.Settings.Length!=Result.Settings.Length||Current.Settings.Width!=Result.Settings.Width||
        Current.Settings.Depth!=Result.Settings.Depth||Current.Settings.OpeningLength!=Result.Settings.OpeningLength||
        Current.Settings.OpeningWidth!=Result.Settings.OpeningWidth||Current.Settings.Coaming!=Result.Settings.Coaming;
    uint32 ScaleKey=GetTypeHash(bRealScale);for(double V:{VesselLength,VesselBeam,MouldedDepth,DesignDraft,OperatingDraft,BerthGap,QuayAboveWater,RailSpan})ScaleKey=HashCombineFast(ScaleKey,GetTypeHash(V));
    RecycledHeights=MoveTemp(Current.Heights);Current=MoveTemp(Result);if(Rebuild||ScaleKey!=SceneScaleKey){BuildStructure();BuildVessel();SceneScaleKey=ScaleKey;}
    Original.Heights=Current.Heights;Original.Volume=Current.Volume;Original.Fill=Current.Fill;
    HistoryHead=HistoryCount=0;LastRemoved=TotalRemoved=0;
    UpdateCargoMesh();
    ++SurfaceRevision;++CompletedGenerations;ConfigureSensors();
    LastMeshApplyMs=(FPlatformTime::Seconds()-Start)*1000;
}
void AShipHatchSimulation::UpdateCargoMesh(bool UpdateAppearance)
{
    LLM_SCOPE_BYNAME(TEXT("Hatch/CargoUpdate"));
    if(!Cargo)
    {FActorSpawnParameters P;P.Owner=this;P.ObjectFlags|=RF_Transient;Cargo=GetWorld()->SpawnActor<AShipHatchPart>(AShipHatchPart::StaticClass(),GetActorTransform(),P);Cargo->Tags.Add(TEXT("HatchCargo"));}
    // Park zero-thickness faces 1cm inside the solid tank top. Keeping valid
    // triangles also preserves Chaos collision topology when an empty initial
    // hold is later refilled; the tank top always owns the first surface hit.
    auto& V=CargoVertices;auto& T=CargoIndices;auto& Colors=CargoColors;V.Reset();Colors.Reset();
    const int32 Count=Current.NX*Current.NY*6;V.Reserve(Count);Colors.Reserve(Count);
    const double MeanHeight=Current.Volume/(Current.Settings.Length*Current.Settings.Width);
    auto Vertex=[&](int32 X,int32 Y,bool Empty)
    {
        const double H=Current.At(X,Y);V.Add(FVector((X*Current.DX-Current.Settings.Length*.5)*100,(Y*Current.DY-Current.Settings.Width*.5)*100,Empty?-1:H*100));
        const int32 I=Y*(Current.NX+1)+X;
        const double Removed=Original.Heights.IsValidIndex(I)?FMath::Max(0.,double(Original.Heights[I])-H):0;
        const double Fresh=FMath::Clamp(Removed/.6,0.,1.);
        const double Surround=(Current.At(FMath::Max(0,X-1),Y)+Current.At(FMath::Min(Current.NX,X+1),Y)+Current.At(X,FMath::Max(0,Y-1))+Current.At(X,FMath::Min(Current.NY,Y+1)))*.25;
        const double Wall=FMath::Min(FMath::Min(X*Current.DX,(Current.NX-X)*Current.DX),FMath::Min(Y*Current.DY,(Current.NY-Y)*Current.DY));
        const double ReliefShade=1+.22*FMath::Clamp((H-MeanHeight)/1.5,-1.,1.);
        const double Shade=ReliefShade*(1-.22*Fresh-.18*FMath::Clamp((Surround-H)/.4,0.,1.))*(.80+.20*FMath::Clamp(Wall/.9,0.,1.));
        // Alpha is a retained per-vertex cut mask, restored from the exact
        // original field on undo/restore. No new texture or per-frame upload.
        Colors.Add(FLinearColor(Shade,Shade*(1-.035*Fresh),Shade*(1-.06*Fresh),Fresh));
    };
    for(int32 Y=0;Y<Current.NY;++Y)for(int32 X=0;X<Current.NX;++X)
    {
        const bool E1=FMath::Max3(Current.At(X,Y),Current.At(X+1,Y+1),Current.At(X+1,Y))<=1.e-4;
        const bool E2=FMath::Max3(Current.At(X,Y),Current.At(X,Y+1),Current.At(X+1,Y+1))<=1.e-4;
        Vertex(X,Y,E1);Vertex(X+1,Y+1,E1);Vertex(X+1,Y,E1);
        Vertex(X,Y,E2);Vertex(X,Y+1,E2);Vertex(X+1,Y+1,E2);
    }
    if(T.Num()!=Count){T.SetNumUninitialized(Count,false);for(int32 I=0;I<Count;++I)T[I]=I;}
    auto* Existing=Cargo->Mesh->GetProcMeshSection(0);
    const bool Refitting=Existing&&Existing->ProcVertexBuffer.Num()==V.Num()&&Existing->ProcIndexBuffer.Num()==T.Num();
    // UpdateMeshSection otherwise calls BodyInstance.UpdateTriMeshVertices,
    // whose UE5.3 Chaos implementation CopySlow()s the full triangle mesh.
    // We already cook the authoritative current triangles below (required for
    // correct refill query bounds). Suppress only that redundant intermediate
    // copy; keep the existing physical body live until its one replacement.
    if(Refitting)Existing->bEnableCollision=false;
    MeshNX=Current.NX;MeshNY=Current.NY;Mesh(Cargo->Mesh,V,T,&CargoNormals,Colors);
    if(Refitting)Existing->bEnableCollision=true;
    if(Refitting)
    {
        // UE 5.3's live triangle refit leaves stale query bounds after refill.
        // Reuse the render buffers and the one BodySetup, but cook its current
        // triangles before recreating the body. Recreating the old cooked body
        // alone would silently restore obsolete collision geometry.
        // This transient runtime mesh has no serialized collision payload. Keeping
        // bHasCookedCollisionData set would send each edit through editor DDC
        // under a fresh GUID, retaining derived blobs across repeated edits.
        LLM_SCOPE_BYNAME(TEXT("Hatch/CargoCook"));
        auto* Body=Cargo->Mesh->GetBodySetup();Body->bHasCookedCollisionData=false;
        Body->InvalidatePhysicsData();Body->CreatePhysicsMeshes();Cargo->Mesh->RecreatePhysicsState();
    }
    if(UpdateAppearance){
        auto* Base=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/MA0T10/Hatch/Materials/M_HatchCargo.M_HatchCargo"));
        auto* MID=Cast<UMaterialInstanceDynamic>(Cargo->Mesh->GetMaterial(0));
        if(Base&&(!MID||MID->Parent!=Base)){MID=UMaterialInstanceDynamic::Create(Base,Cargo);Cargo->Mesh->SetMaterial(0,MID);}
        const bool Coal=Current.Settings.Cargo==ShipHatch::ECargo::Coal;
        if(MID){MID->SetVectorParameterValue(TEXT("ColdColor"),Coal?FLinearColor(.033,.031,.029):FLinearColor(.11,.072,.051));MID->SetScalarParameterValue(TEXT("Coal"),Coal?1:0);}
        Cargo->Optical->Reflectivity940Nm=ShipHatch::Reflectivity(Current.Settings.Cargo);}
}
uint64 AShipHatchSimulation::DigBufferBytes() const
{
    uint64 Bytes=Original.Heights.GetAllocatedSize()+DigScratch.GetAllocatedSize();for(const auto& H:History)Bytes+=H.Heights.GetAllocatedSize();return Bytes;
}
bool AShipHatchSimulation::PickCargo(FVector Origin,FVector Direction,FVector& Hit) const
{
    if(bEnding||bGenerating||!Cargo||Origin.ContainsNaN()||Direction.ContainsNaN()||!Direction.Normalize())return false;
    const auto Pose=GetActorTransform();
    if(!ShipHatch::Raycast(Current,Pose.InverseTransformPosition(Origin)*.01,Pose.InverseTransformVector(Direction),Hit))return false;
    const FVector WorldHit=Pose.TransformPosition(Hit*100);
    if(PresentationOccludes(Origin,WorldHit))return false;
    // Visibility is player-local in point-only mode. Physical structures still occlude
    // the height field in BOTH render modes, including the deck above hidden cargo.
    FHitResult First;FCollisionQueryParams Query(SCENE_QUERY_STAT(HatchGrab),true);
    if(!bShowUnloader&&UnloaderPresentation)Query.AddIgnoredComponent(UnloaderPresentation->Mesh);
    // Picking chooses the cargo location, not the cab/optical sightline.
    // The independent cab still occludes normal world and LiDAR queries.
    if(CabPresentation)Query.AddIgnoredComponent(CabPresentation);
    if(!GetWorld()->LineTraceSingleByChannel(First,Origin,WorldHit+Direction*2,ECC_Visibility,Query))return false;
    if(First.GetActor()==Cargo)return true;
    // Chaos may resolve a sub-centimeter sheet as its adjacent tank top first.
    // Permit removing that analytically proven residue; empty floor still has
    // no Raycast hit. Do not bypass any deck, coaming, or other obstruction.
    return Hit.Z>1.e-4&&Hit.Z<.01&&First.GetActor()&&First.GetActor()->ActorHasTag(TEXT("HatchHold"))&&
        FVector::DistSquared(First.ImpactPoint,WorldHit)<=1.&&FMath::Abs(Pose.InverseTransformPosition(First.ImpactPoint).Z)<.01;
}
bool AShipHatchSimulation::Dig(FVector Origin,FVector Direction,double Radius,double Depth)
{
    FVector Hit;if(!PickCargo(Origin,Direction,Hit))return false;
    const double Start=FPlatformTime::Seconds();
    DigScratch=Current.Heights;const double Volume=Current.Volume,Fill=Current.Fill;
    const double Removed=ShipHatch::Excavate(Current,FVector2D(Hit.X,Hit.Y),Radius,Depth,GrabAspect);
    if(Removed<=0){Current.Heights=DigScratch;Current.Volume=Volume;Current.Fill=Fill;return false;}
    auto& Previous=History[HistoryHead];Previous.Heights=DigScratch;Previous.Volume=Volume;
    Previous.Fill=Fill;Previous.Last=LastRemoved;Previous.Total=TotalRemoved;
    HistoryHead=(HistoryHead+1)%UndoLimit;HistoryCount=FMath::Min(HistoryCount+1,UndoLimit);
    LastRemoved=Removed;TotalRemoved=FMath::Max(0.,Original.Volume-Current.Volume);
    FinishCargoEdit();LastMeshApplyMs=(FPlatformTime::Seconds()-Start)*1000;return true;
}
bool AShipHatchSimulation::UndoDig()
{
    if(bEnding||bGenerating||!HistoryCount)return false;
    const double Start=FPlatformTime::Seconds();HistoryHead=(HistoryHead+UndoLimit-1)%UndoLimit;--HistoryCount;
    const auto& Previous=History[HistoryHead];Current.Heights=Previous.Heights;Current.Volume=Previous.Volume;Current.Fill=Previous.Fill;
    LastRemoved=Previous.Last;TotalRemoved=Previous.Total;FinishCargoEdit();LastMeshApplyMs=(FPlatformTime::Seconds()-Start)*1000;return true;
}
bool AShipHatchSimulation::RestoreCargo()
{
    if(bEnding||bGenerating||TotalRemoved<=0)return false;
    const double Start=FPlatformTime::Seconds();Current.Heights=Original.Heights;Current.Volume=Original.Volume;Current.Fill=Original.Fill;
    HistoryHead=HistoryCount=0;LastRemoved=TotalRemoved=0;FinishCargoEdit();LastMeshApplyMs=(FPlatformTime::Seconds()-Start)*1000;return true;
}
void AShipHatchSimulation::FinishCargoEdit()
{
    LastGeometryEdit=FPlatformTime::Seconds();
    FrameGates[0]=MLX?MLX->ScanComponent->GetRuntimeStatus().FrameId:-1;
    FrameGates[1]=Livox?Livox->ScanComponent->GetRuntimeStatus().FrameId:-1;
    FrameGates[2]=CabRight?CabRight->ScanComponent->GetRuntimeStatus().FrameId:-1;
    UpdateCargoMesh(false);++SurfaceRevision;
    // Keep acquisition cadence and populated display buffers alive. An in-flight
    // pre-edit acquisition is excluded from analysis by its immutable start time.
    for(int32 I=0;I<3;++I)if(auto* A=SensorEnabled(I)?SensorAt(I):nullptr)
    {A->ScanComponent->InvalidateSurfaceResponseCache();A->VisualizationComponent->RefreshPointPreview();}
    Status=TEXT("측정 중 · 가려진 영역은 측정 불가");OnSurfaceChanged.Broadcast();
}
bool AShipHatchSimulation::SetSensorSelection(bool EnableMLX,bool EnableLivox,bool Native)
{
    if(!EnableMLX&&!EnableLivox){Status=TEXT("측정할 장비를 하나 이상 선택하세요.");return false;}
    bFieldLayout=false;bMLX=EnableMLX;bLivox=EnableLivox;bNativeMLX=Native;
    if(!bGenerating&&Current.Heights.Num()){ConfigureSensors();++SurfaceRevision;OnSurfaceChanged.Broadcast();}return true;
}
void AShipHatchSimulation::ConfigureSensors()
{
    FrameGates[0]=MLX?MLX->ScanComponent->GetRuntimeStatus().FrameId:-1;
    FrameGates[1]=Livox?Livox->ScanComponent->GetRuntimeStatus().FrameId:-1;
    FrameGates[2]=CabRight?CabRight->ScanComponent->GetRuntimeStatus().FrameId:-1;
    auto Configure=[&](TObjectPtr<AVirtualLidarSensorActor>& A,EVirtualLidarDeviceProfile Profile,bool Enabled,const TCHAR* Id,int32 Index)
    {
        if(!A)
        {A=GetWorld()->SpawnActorDeferred<AVirtualLidarSensorActor>(AVirtualLidarSensorActor::StaticClass(),GetActorTransform(),this);
         if(!A)return;A->SetFlags(RF_Transient);A->ScanComponent->bAutoStartScan=false;A->FinishSpawning(GetActorTransform());}
        A->StopSensor();A->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);FVirtualSensorEditableState S,Applied;FString Error;A->ReadEditableState(S);
        const bool ProfileChanged=S.LidarProfile!=Profile||S.SimulationQuality!=EVirtualSensorSimulationQuality::FullSpec;
        S.LidarProfile=Profile;S.SimulationQuality=EVirtualSensorSimulationQuality::FullSpec;
        if(ProfileChanged&&!A->ApplyProfileAndSimulationQuality(S,Applied,Error)){Status=Error;return;}
        A->ReadEditableState(S);FVirtualSensorEditableState Previous=S;S.SensorId=Id;
        const bool IsMLX=Profile!=EVirtualLidarDeviceProfile::LivoxMid360S&&Profile!=EVirtualLidarDeviceProfile::LivoxMid360;
        S.ActorTransform=FTransform(GetActorQuat()*FRotator((IsMLX?-90:-112.5)+(bFieldLayout?SensorTiltTrim[Index]:0),bFieldLayout?SensorYaw[Index]:0,0).Quaternion(),GetActorTransform().TransformPosition((bFieldLayout?SensorOrigin(Index):SensorLocalOrigin(IsMLX))*100));
        S.CompatibilityOutputPolicy=EVirtualLidarCompatibilityOutputPolicy::WhenRequested;S.bLidarMaterialResponse=true;
        S.PreviewPointStride=2;S.MaxPreviewPoints=12000;S.bExportPcdOnScan=false;
        if(!FVirtualSensorEditableState::StaticStruct()->CompareScriptStruct(&Previous,&S,PPF_None))
            if(!A->ApplyEditableState(S,Error)){Status=Error;return;}
        // GPU-LIDAR-01: the same collision snapshot feeds CPU and sensor-owned HWRT.
        // Preserve the sensor backend selection across surface/rig reconfiguration.
        auto* Scan=A->ScanComponent.Get();
        A->VisualizationComponent->SetProjectionPreviewEnabled(false);
        const TWeakObjectPtr<AShipHatchSimulation> Weak(this);
        Scan->SetRayQueryFactory([Weak](AActor* Ignore,ECollisionChannel Channel)->TSharedPtr<const IVirtualSensorRayQuery,ESPMode::ThreadSafe>{auto* S=Weak.Get();return S?S->CaptureRayQuery(Ignore,Channel):nullptr;});
        A->VisualizationComponent->SetPointPreviewTransform([Weak](FVirtualLidarPoint& P,double AcquisitionStart)
        {
            const auto* Self=Weak.Get();if(!Self||AcquisitionStart>=Self->LastGeometryEdit||P.SemanticLabel!=TEXT("HatchCargo")||!P.bHit)return;
            auto L=Self->GetActorTransform().InverseTransformPosition(P.WorldLocation)*.01;
            L.Z=Self->Current.Height(L.X,L.Y);P.WorldLocation=Self->GetActorTransform().TransformPosition(L*100);
            if(L.Z<=1.e-4)P.SemanticLabel=TEXT("HatchHold");
        });
        // Geometry and preview are GT-owned. For an unchanged surface the
        // per-point adapter is identity; avoid copying point/tag arrays at5Hz.
        A->VisualizationComponent->SetPointPreviewTransformNeeded([Weak](double AcquisitionStart)
        {const auto* Self=Weak.Get();return Self&&AcquisitionStart<Self->LastGeometryEdit;});
        if(Scan->SemanticClassRules.Num()!=4||Scan->SemanticClassRules[0].Label!=TEXT("HatchCargo"))
        {
            Scan->SemanticClassRules.Reset();
            for(const auto& Pair:TArray<TPair<FName,FLinearColor>>{{TEXT("HatchCargo"),FLinearColor(1,.6,.05)},{TEXT("HatchCoaming"),FLinearColor(.05,1,.2)},{TEXT("HatchHold"),FLinearColor(.1,.55,1)},{TEXT("HatchOutside"),FLinearColor(1,.05,.05)}})
            {FVirtualLidarSemanticClassRule R;R.Label=Pair.Key;R.ActorTags.Add(Pair.Key);R.DisplayColor=Pair.Value;Scan->SemanticClassRules.Add(R);}
        }
        Scan->InvalidateSurfaceResponseCache();if(Enabled)A->StartSensor();
    };
    auto FieldProfile=[&](int32 I){if(FieldProfiles[I]>=3)return EVirtualLidarDeviceProfile(int32(EVirtualLidarDeviceProfile::LivoxMid360)+FieldProfiles[I]-3);return FieldProfiles[I]==1?EVirtualLidarDeviceProfile::LivoxMid360S:FieldProfiles[I]==2?EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE:EVirtualLidarDeviceProfile::IYOBOT_MLX80;};
    Configure(MLX,bFieldLayout?FieldProfile(0):bNativeMLX?EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE:EVirtualLidarDeviceProfile::IYOBOT_MLX80,bMLX,bFieldLayout?TEXT("os_lidar_boom"):TEXT("Hatch.MLX80"),0);
    Configure(Livox,bFieldLayout?FieldProfile(1):EVirtualLidarDeviceProfile::LivoxMid360S,bLivox,bFieldLayout?TEXT("os_lidar_cabin_01"):TEXT("Hatch.Mid360S"),1);
    if(bFieldLayout)Configure(CabRight,FieldProfile(2),bCabRight,TEXT("os_lidar_cabin_02"),2);else if(CabRight)CabRight->StopSensor();
    UpdateSensorPresentation();ApplyUnloader();
}
void AShipHatchSimulation::CollectObservations(TArray<ShipHatch::FObservation>& Out) const
{
    Out.Reset();if(bGenerating)return;
    for(int32 Index=0;Index<3;++Index)
    {auto* A=SensorEnabled(Index)?SensorAt(Index):nullptr;
        const int64 Gate=FrameGates[Index];if(!A)continue;
        const auto Frame=A->ScanComponent->GetLastFrameSnapshot();if(!Frame||Frame->FrameId<=Gate||Frame->AcquisitionStartedMonotonic<LastGeometryEdit)continue;
        for(const auto& P:*Frame->Points)if(P.bHit)
        {
            const FVector L=GetActorTransform().InverseTransformPosition(P.WorldLocation)*.01;
            const FVector O=GetActorTransform().InverseTransformPosition(Frame->AcquisitionTransform.GetLocation())*.01;
            if(P.SemanticLabel!=TEXT("HatchCargo")||ShipHatch::ThroughOpening(Current.Settings,O,L))Out.Add({L,P.SemanticLabel});
        }
    }
}
bool AShipHatchSimulation::HasFreshMeasurement() const
{
    if(bGenerating)return false;
    for(int32 Index=0;Index<3;++Index)
    {auto* A=SensorEnabled(Index)?SensorAt(Index):nullptr;
    const int64 Gate=FrameGates[Index];if(A){const auto F=A->ScanComponent->GetLastFrameSnapshot();if(!F||F->FrameId<=Gate||F->AcquisitionStartedMonotonic<LastGeometryEdit)return false;}}
    return bMLX||bLivox||(bFieldLayout&&bCabRight);
}
void AShipHatchSimulation::VisitGeometryBounds(bool IncludeUnloader,TFunctionRef<void(const FBox&)> Visitor) const
{
    auto Visit=[&](const AShipHatchPart* Part){if(IsValid(Part)){FVector Center,Extent;Part->GetActorBounds(false,Center,Extent);Visitor(FBox(Center-Extent,Center+Extent));}};
    Visit(Cargo);for(const auto& Part:Structure)Visit(Part.Get());for(const auto& Part:SensorPresentation)Visit(Part.Get());if(IncludeUnloader)Visit(UnloaderPresentation);
}
void AShipHatchSimulation::EndPlay(const EEndPlayReason::Type Reason)
{
    bEnding=true;GetWorldTimerManager().ClearTimer(CabTravelTimer);bCabMoving=false;bGrabWorkActive=false;GetWorldTimerManager().ClearTimer(PollTimer);OnSurfaceChanged.Clear();
    if(OutdoorSky)OutdoorSky->DestroyComponent();if(OutdoorAmbient)OutdoorAmbient->DestroyComponent();if(IsValid(FixtureSky))FixtureSky->SetVisibility(bFixtureSkyVisible);
    if(KeyLight){KeyLight->SetActorRotation(OriginalLightRotation);KeyLight->GetLightComponent()->SetIntensity(OriginalLightIntensity);KeyLight->GetLightComponent()->SetCastShadows(bOriginalLightShadows);CastChecked<UDirectionalLightComponent>(KeyLight->GetLightComponent())->SetAtmosphereSunLight(bOriginalAtmosphereSun);KeyLight->GetLightComponent()->SetMobility(EComponentMobility::Type(OriginalLightMobility));}
    GetWorldTimerManager().ClearTimer(ScanDisplayTimer);for(auto A:SensorPresentation)if(IsValid(A))A->Destroy();SensorPresentation.Reset();
    if(MLX)MLX->Destroy();if(Livox)Livox->Destroy();if(CabRight)CabRight->Destroy();if(UnloaderPresentation)UnloaderPresentation->Destroy();if(Vessel)Vessel->Destroy();if(Cargo)Cargo->Destroy();
    for(auto A:Structure)if(IsValid(A))A->Destroy();Structure.Reset();
    // The bounded worker owns values only; no callback can access this Actor after EndPlay.
    Super::EndPlay(Reason);
}

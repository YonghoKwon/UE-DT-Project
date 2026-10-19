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

namespace
{
void Box(TArray<FVector>& V,TArray<int32>& T,FVector Center,FVector Size)
{
    const int32 B=V.Num();
    for(int32 Z=0;Z<2;++Z)for(int32 Y=0;Y<2;++Y)for(int32 X=0;X<2;++X)
        V.Add((Center+FVector((X-.5)*Size.X,(Y-.5)*Size.Y,(Z-.5)*Size.Z))*100);
    const int32 Indices[]={0,2,3,0,3,1,4,5,7,4,7,6,0,1,5,0,5,4,2,6,7,2,7,3,0,4,6,0,6,2,1,3,7,1,7,5};
    for(int32 I:Indices)T.Add(B+I);
}
void Mesh(UProceduralMeshComponent* M,const TArray<FVector>& V,const TArray<int32>& T,TArray<FVector>* Scratch=nullptr)
{
    TArray<FVector> LocalNormals;auto& N=Scratch?*Scratch:LocalNormals;N.Init(FVector::ZeroVector,V.Num());
    for(int32 I=0;I<T.Num();I+=3)
    {const FVector Normal=FVector::CrossProduct(V[T[I+2]]-V[T[I]],V[T[I+1]]-V[T[I]]);for(int32 J=0;J<3;++J)N[T[I+J]]+=Normal;}
    for(auto& Normal:N)Normal.Normalize();
    const auto* Existing=M->GetProcMeshSection(0);
    bool SameTopology=Existing&&Existing->ProcVertexBuffer.Num()==V.Num()&&Existing->ProcIndexBuffer.Num()==T.Num();
    if(SameTopology)for(int32 I=0;I<T.Num();++I)if(Existing->ProcIndexBuffer[I]!=uint32(T[I])){SameTopology=false;break;}
    if(SameTopology)
        M->UpdateMeshSection_LinearColor(0,V,N,TArray<FVector2D>(),TArray<FLinearColor>(),TArray<FProcMeshTangent>());
    else M->CreateMeshSection_LinearColor(0,V,T,N,TArray<FVector2D>(),TArray<FLinearColor>(),TArray<FProcMeshTangent>(),true);
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
void AShipHatchSimulation::BeginPlay(){Super::BeginPlay();Regenerate(false);}
bool AShipHatchSimulation::Regenerate(bool Randomize)
{
    if(bEnding)return false;
    if(Randomize){int32 Next=int32(FPlatformTime::Cycles64()&0x7fffffff);Seed=Next==Seed?Seed^0x41c64e6d:Next;}
    Pending=Settings();if(!Pending.Validate(Status))return false;
    bPending=true;
    if(MLX)MLX->StopSensor();if(Livox)Livox->StopSensor();
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
    GetWorldTimerManager().ClearTimer(PollTimer);bGenerating=false;
    ApplySurface(MoveTemp(Result.Surface));
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
    };
    Part(TEXT("HatchHold"),FLinearColor(.20,.28,.32),[&](TArray<FVector>& V,TArray<int32>& T)
    {
        Box(V,T,FVector(0,0,-.15),FVector(S.Length+.6,S.Width+.6,.3));
        for(int32 Sign:{-1,1})
        {Box(V,T,FVector(Sign*(S.Length+.3)*.5,0,S.Depth*.5),FVector(.3,S.Width+.6,S.Depth));
         Box(V,T,FVector(0,Sign*(S.Width+.3)*.5,S.Depth*.5),FVector(S.Length,.3,S.Depth));}
    });
    Part(TEXT("HatchOutside"),FLinearColor(.35,.12,.09),[&](TArray<FVector>& V,TArray<int32>& T)
    {
        const double L=S.Length+8,W=S.Width+8;
        for(int32 Sign:{-1,1})
        {Box(V,T,FVector(Sign*(L+S.OpeningLength)*.25,0,S.Depth+.15),FVector((L-S.OpeningLength)*.5,W,.3));
         Box(V,T,FVector(0,Sign*(W+S.OpeningWidth)*.25,S.Depth+.15),FVector(S.OpeningLength,(W-S.OpeningWidth)*.5,.3));}
    });
    Part(TEXT("HatchCoaming"),FLinearColor(.08,.40,.20),[&](TArray<FVector>& V,TArray<int32>& T)
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
    RecycledHeights=MoveTemp(Current.Heights);Current=MoveTemp(Result);if(Rebuild)BuildStructure();
    if(!Cargo)
    {FActorSpawnParameters P;P.Owner=this;P.ObjectFlags|=RF_Transient;Cargo=GetWorld()->SpawnActor<AShipHatchPart>(AShipHatchPart::StaticClass(),GetActorTransform(),P);Cargo->Tags.Add(TEXT("HatchCargo"));}
    auto& V=CargoVertices;auto& T=CargoIndices;V.Reset();V.Reserve(Current.Heights.Num());
    for(int32 Y=0;Y<=Current.NY;++Y)for(int32 X=0;X<=Current.NX;++X)
        V.Add(FVector(X*Current.DX-Current.Settings.Length*.5,Y*Current.DY-Current.Settings.Width*.5,Current.At(X,Y))*100);
    if(MeshNX!=Current.NX||MeshNY!=Current.NY)
    {T.Reset();T.Reserve(Current.NX*Current.NY*6);for(int32 Y=0;Y<Current.NY;++Y)for(int32 X=0;X<Current.NX;++X)
     {const int32 A=Y*(Current.NX+1)+X,B=A+1,C=A+Current.NX+1,D=C+1;T.Append({A,D,B,A,C,D});}MeshNX=Current.NX;MeshNY=Current.NY;}
    Mesh(Cargo->Mesh,V,T,&CargoNormals);Cargo->SetColor(Current.Settings.Cargo==ShipHatch::ECargo::Coal?FLinearColor(.012,.016,.02):FLinearColor(.32,.075,.035));
    Cargo->Optical->Reflectivity940Nm=ShipHatch::Reflectivity(Current.Settings.Cargo);
    ++SurfaceRevision;++CompletedGenerations;ConfigureSensors();
    LastMeshApplyMs=(FPlatformTime::Seconds()-Start)*1000;
    Status=TEXT("측정 중 · 가려진 영역은 측정 불가");OnSurfaceChanged.Broadcast();
}
bool AShipHatchSimulation::SetSensorSelection(bool EnableMLX,bool EnableLivox,bool Native)
{
    if(!EnableMLX&&!EnableLivox){Status=TEXT("측정할 장비를 하나 이상 선택하세요.");return false;}
    bMLX=EnableMLX;bLivox=EnableLivox;bNativeMLX=Native;
    if(!bGenerating&&Current.Heights.Num()){ConfigureSensors();++SurfaceRevision;OnSurfaceChanged.Broadcast();}return true;
}
void AShipHatchSimulation::ConfigureSensors()
{
    FrameGates[0]=MLX?MLX->ScanComponent->GetRuntimeStatus().FrameId:-1;
    FrameGates[1]=Livox?Livox->ScanComponent->GetRuntimeStatus().FrameId:-1;
    auto Configure=[&](TObjectPtr<AVirtualLidarSensorActor>& A,EVirtualLidarDeviceProfile Profile,bool Enabled,const TCHAR* Id)
    {
        if(!A)
        {A=GetWorld()->SpawnActorDeferred<AVirtualLidarSensorActor>(AVirtualLidarSensorActor::StaticClass(),GetActorTransform(),this);
         if(!A)return;A->SetFlags(RF_Transient);A->ScanComponent->bAutoStartScan=false;A->FinishSpawning(GetActorTransform());}
        A->StopSensor();FVirtualSensorEditableState S,Applied;FString Error;A->ReadEditableState(S);
        const bool ProfileChanged=S.LidarProfile!=Profile||S.SimulationQuality!=EVirtualSensorSimulationQuality::FullSpec;
        S.LidarProfile=Profile;S.SimulationQuality=EVirtualSensorSimulationQuality::FullSpec;
        if(ProfileChanged&&!A->ApplyProfileAndSimulationQuality(S,Applied,Error)){Status=Error;return;}
        A->ReadEditableState(S);FVirtualSensorEditableState Previous=S;S.SensorId=Id;
        S.ActorTransform=FTransform(GetActorQuat()*FRotator(-90,0,0).Quaternion(),GetActorTransform().TransformPosition(FVector(0,0,(Current.Settings.Depth+Current.Settings.SensorAboveDeck)*100)));
        S.CompatibilityOutputPolicy=EVirtualLidarCompatibilityOutputPolicy::WhenRequested;S.bLidarMaterialResponse=true;
        S.PreviewPointStride=2;S.MaxPreviewPoints=12000;S.bExportPcdOnScan=false;
        if(!FVirtualSensorEditableState::StaticStruct()->CompareScriptStruct(&Previous,&S,PPF_None))
            if(!A->ApplyEditableState(S,Error)){Status=Error;return;}
        // A documented map-local adapter: PMC collision is supported by CPU traces, not the GPU semantic proxy.
        auto* Scan=A->ScanComponent.Get();Scan->AcquisitionBackend=EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
        A->VisualizationComponent->SetProjectionPreviewEnabled(false);
        if(Scan->SemanticClassRules.Num()!=4||Scan->SemanticClassRules[0].Label!=TEXT("HatchCargo"))
        {
            Scan->SemanticClassRules.Reset();
            for(const auto& Pair:TArray<TPair<FName,FLinearColor>>{{TEXT("HatchCargo"),FLinearColor(1,.6,.05)},{TEXT("HatchCoaming"),FLinearColor(.05,1,.2)},{TEXT("HatchHold"),FLinearColor(.1,.55,1)},{TEXT("HatchOutside"),FLinearColor(1,.05,.05)}})
            {FVirtualLidarSemanticClassRule R;R.Label=Pair.Key;R.ActorTags.Add(Pair.Key);R.DisplayColor=Pair.Value;Scan->SemanticClassRules.Add(R);}
        }
        Scan->InvalidateSurfaceResponseCache();if(Enabled)A->StartSensor();
    };
    Configure(MLX,bNativeMLX?EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE:EVirtualLidarDeviceProfile::IYOBOT_MLX80,bMLX,TEXT("Hatch.MLX80"));
    Configure(Livox,EVirtualLidarDeviceProfile::LivoxMid360S,bLivox,TEXT("Hatch.Mid360S"));
}
void AShipHatchSimulation::CollectObservations(TArray<ShipHatch::FObservation>& Out) const
{
    Out.Reset();if(bGenerating)return;
    int32 Index=0;
    for(auto* A:{bMLX?MLX.Get():nullptr,bLivox?Livox.Get():nullptr})
    {
        const int64 Gate=FrameGates[Index++];if(!A)continue;
        const auto Frame=A->ScanComponent->GetLastFrameSnapshot();if(!Frame||Frame->FrameId<=Gate)continue;
        for(const auto& P:*Frame->Points)if(P.bHit)
            Out.Add({GetActorTransform().InverseTransformPosition(P.WorldLocation)*.01,P.SemanticLabel});
    }
}
bool AShipHatchSimulation::HasFreshMeasurement() const
{
    if(bGenerating)return false;
    int32 Index=0;
    for(auto* A:{bMLX?MLX.Get():nullptr,bLivox?Livox.Get():nullptr})
    {const int64 Gate=FrameGates[Index++];if(A){const auto F=A->ScanComponent->GetLastFrameSnapshot();if(!F||F->FrameId<=Gate)return false;}}
    return bMLX||bLivox;
}
void AShipHatchSimulation::EndPlay(const EEndPlayReason::Type Reason)
{
    bEnding=true;GetWorldTimerManager().ClearTimer(PollTimer);OnSurfaceChanged.Clear();
    if(MLX)MLX->Destroy();if(Livox)Livox->Destroy();if(Cargo)Cargo->Destroy();
    for(auto A:Structure)if(IsValid(A))A->Destroy();Structure.Reset();
    // The bounded worker owns values only; no callback can access this Actor after EndPlay.
    Super::EndPlay(Reason);
}

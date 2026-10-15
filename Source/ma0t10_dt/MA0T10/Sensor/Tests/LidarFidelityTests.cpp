#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "PreviewScene.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarFidelity.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSurfaceResponseComponent.h"
#include "ma0t10_dt/MA0T10/Slab/SlabMotionComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorAcquisitionBackend.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarFidelityTimingTest,"MA0T10.LidarFidelity.TimingAndMotionTheory",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLidarFidelityTimingTest::RunTest(const FString&)
{
    using namespace VirtualLidarFidelity;
    for (auto Profile : {EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE,EVirtualLidarDeviceProfile::IYOBOT_MLX80,EVirtualLidarDeviceProfile::Generic,EVirtualLidarDeviceProfile::LivoxMid360S})
    {
        int64 Previous=-1;
        const int32 W=Profile==EVirtualLidarDeviceProfile::IYOBOT_MLX80?200:576,H=56;
        for(int32 Order=0;Order<W*H;++Order)
        {
            const int32 Index=Profile==EVirtualLidarDeviceProfile::Generic?(Order%H)*W+Order/H:Order;
            const int64 Time=PointOffset(Profile,Index,W,H,.05);
            if(Time<=Previous || Time<0 || Time>=50000000){AddError(TEXT("firing order must be strictly monotonic in [0,period)"));return false;}
            Previous=Time;
        }
    }
    TestEqual(TEXT("single firing is at frame origin"),PointOffset(EVirtualLidarDeviceProfile::Generic,0,1,1,.05),int64(0));
    FVirtualSensorMotionSample Target,Sensor;
    const FVector Hit(1000,0,0), Normal(-1,0,0), Ray(1,0,0);
    float Range=0,Cosine=0;
    Target.LinearVelocityCmPerSecond=FVector(200,0,0);
    TestTrue(TEXT("moving plane solved"),RollingRange(Hit,Normal,Ray,FTransform::Identity,Target,Sensor,.05,Range,Cosine));
    TestTrue(TEXT("v times delta-t: 2m/s times 50ms is 10cm"),FMath::IsNearlyEqual(Range,1010.f,.0001f));
    Sensor.LinearVelocityCmPerSecond=FVector(100,0,0);
    RollingRange(Hit,Normal,Ray,FTransform::Identity,Target,Sensor,.05,Range,Cosine);
    TestTrue(TEXT("relative sensor/target velocity"),FMath::IsNearlyEqual(Range,1005.f,.0001f));
    Target=FVirtualSensorMotionSample();Sensor=FVirtualSensorMotionSample();
    RollingRange(Hit,Normal,Ray,FTransform::Identity,Target,Sensor,.05,Range,Cosine);
    TestEqual(TEXT("stationary scene has zero distortion"),Range,1000.f);
    Sensor.AngularVelocityRadiansPerSecond=FVector(0,0,1);
    RollingRange(Hit,Normal,Ray,FTransform::Identity,Target,Sensor,.05,Range,Cosine);
    TestTrue(TEXT("rotating sensor plane range agrees with secant"),FMath::IsNearlyEqual(Range,float(1000/FMath::Cos(.05)),.001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarFidelityAcquisitionTest,"MA0T10.LidarFidelity.CpuAcquisitionAndDefaultBytes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLidarFidelityAcquisitionTest::RunTest(const FString&)
{
    FPreviewScene Scene(FPreviewScene::ConstructionValues().SetCreateDefaultLighting(false));
    UWorld* World=Scene.GetWorld();
    auto* Wall=World->SpawnActor<AStaticMeshActor>();
    auto* Mesh=Wall->GetStaticMeshComponent();Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Wall->SetActorScale3D(FVector(1,200,200));Wall->Tags.Add(TEXT("Slab"));
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Mesh->SetCollisionResponseToAllChannels(ECR_Block);
    auto* Motion=NewObject<USlabMotionComponent>(Wall);Wall->AddInstanceComponent(Motion);Motion->RegisterComponent();
    auto Data=MakeShared<FSlabScenarioData,ESPMode::ThreadSafe>();
    FSlabScenarioRow A,B;A.CenterX=1000;A.ElapsedSec=0;B.CenterX=2000;B.ElapsedSec=1;Data->Rows={A,B};Data->DurationSec=1;
    Motion->OnPoseApplied.BindLambda([](const FSlabScenarioRow&,int32,double,bool){});
    TestTrue(TEXT("known-speed Slab motion starts"),Motion->StartPlayback(Data,FTransform::Identity,FVector(100,20000,20000),ESlabInputUnit::Centimeters));
    auto* Actor=World->SpawnActor<AVirtualLidarSensorActor>();Actor->SetActorLocation(FVector(0,0,10000));
    auto* Scan=Actor->ScanComponent.Get();Scan->StopScan();
    Scan->DeviceProfile=EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE;Scan->HorizontalSamples=16;Scan->VerticalChannels=4;
    Scan->HorizontalFov=2;Scan->MinVerticalAngle=-1;Scan->MaxVerticalAngle=1;Scan->ScanInterval=.05;
    Scan->MaxDistance=5000;Scan->FidelityMode=EVirtualSensorFidelityMode::IdealTruth;Scan->bUseProfileEchoCapability=false;
    Scan->bEnableRangeNoise=false;
    TestFalse(TEXT("point time defaults off"),Scan->bUseRollingPointTimestamps);
    TestFalse(TEXT("motion defaults off"),Scan->bEnableMotionDistortion);TestFalse(TEXT("material defaults off"),Scan->bEnableMaterialResponse);
    TArray<FVirtualLidarPoint> Baseline,Rolling,Stopped,Restored;TArray<uint8> Pixels;
    Scan->ExecuteScan(Baseline,Pixels);
    TestEqual(TEXT("all baseline rays hit box"),Baseline.FilterByPredicate([](const auto& P){return P.bHit;}).Num(),64);
    Scan->bUseRollingPointTimestamps=true;Scan->bEnableMotionDistortion=true;
    Scan->ExecuteScan(Rolling,Pixels);
    double MaxError=0;
    for(int32 I=0;I<Rolling.Num();++I)
    {
        TestTrue(TEXT("moving box hit"),Rolling[I].bHit);
        const double Expected=1000.0*Rolling[I].PointTimeOffsetNanoseconds*1.e-9;
        MaxError=FMath::Max(MaxError,FMath::Abs((Rolling[I].WorldLocation.X-Baseline[I].WorldLocation.X)-Expected));
    }
    TestTrue(TEXT("actual trace + Slab provider agrees with v*dt within 0.01cm"),MaxError<.01);
    Motion->SetPlaybackPaused(true);
    TestTrue(TEXT("paused provider returns zero"),Motion->CaptureSensorMotion().LinearVelocityCmPerSecond.IsZero());
    Scan->ExecuteScan(Stopped,Pixels);
    for(int32 I=0;I<Stopped.Num();++I)TestTrue(TEXT("static scene exactly matches geometry"),Stopped[I].WorldLocation.Equals(Baseline[I].WorldLocation,.001));
    Scan->ComponentVelocity=FVector(100,0,0);
    Scan->ExecuteScan(Stopped,Pixels);
    for(int32 I=0;I<Stopped.Num();++I)TestTrue(TEXT("sensor self-motion uses relative velocity"),
        FMath::Abs(Stopped[I].WorldLocation.X-Baseline[I].WorldLocation.X+100*Stopped[I].PointTimeOffsetNanoseconds*1.e-9)<.01);
    Scan->ComponentVelocity=FVector::ZeroVector;
    Scan->bUseRollingPointTimestamps=false;Scan->bEnableMotionDistortion=false;Scan->bEnableMaterialResponse=false;Scan->InvalidateSurfaceResponseCache();
    Scan->ExecuteScan(Restored,Pixels);
    auto Bytes=[&](const TArray<FVirtualLidarPoint>& Points){
        FVirtualSensorFrameEnvelope Frame;Frame.SensorId=TEXT("fidelity-golden");Frame.FrameId=7;Frame.TimestampUtc=FDateTime(2026,1,1);
        Frame.PointSnapshot=MakeShared<const TArray<FVirtualLidarPoint>,ESPMode::ThreadSafe>(Points);
        FVirtualSensorStreamConfig Config;Config.PointCloudFormat=EVirtualPointCloudStreamFormat::PCD;Config.PcdDataMode=EVirtualPcdDataMode::Binary;
        TArray<uint8> Out;FString Extension,Error;int32 Count;
        TestTrue(TEXT("production PCD serializer"),UVirtualSensorStreamPublisherComponent::SerializePointCloudFrame(Frame,Config,Extension,Out,Count,Error));return Out;
    };
    TestTrue(TEXT("toggle-on/off restores every PCD byte"),Bytes(Baseline)==Bytes(Restored));
    const auto TimedBytes=Bytes(Rolling);
    int32 Body=INDEX_NONE;
    for(int32 I=0;I+12<=TimedBytes.Num();++I)if(FMemory::Memcmp(TimedBytes.GetData()+I,"DATA binary\n",12)==0){Body=I+12;break;}
    if(TestTrue(TEXT("PCD body exists"),Body>=0))
    {
        TestEqual(TEXT("unchanged 33-byte record size"),TimedBytes.Num()-Body,Rolling.Num()*33);
        for(int32 I=0;I<Rolling.Num();++I){uint64 Offset=0;for(int32 Byte=0;Byte<8;++Byte)Offset|=uint64(TimedBytes[Body+I*33+20+Byte])<<(Byte*8);
            TestEqual(TEXT("actual acquisition offsets survive binary PCD"),int64(Offset),Rolling[I].PointTimeOffsetNanoseconds);}
    }
    auto* Response=NewObject<UVirtualLidarSurfaceResponseComponent>(Wall);Wall->AddInstanceComponent(Response);Response->RegisterComponent();
    Scan->bEnableMaterialResponse=true;Scan->bEnableRangeNoise=true;
    auto Sample=[&](float Reflectivity,float Cosine,float Range,int32& Misses,double& Signal){
        Response->Reflectivity940Nm=Reflectivity;Scan->InvalidateSurfaceResponseCache();Misses=0;Signal=0;
        FHitResult Hit(Wall,Mesh,FVector(Range,0,10000),FVector(-Cosine,FMath::Sqrt(1-Cosine*Cosine),0));Hit.Distance=Range;
        TArray<FVirtualLidarPoint> Points;
        for(int32 I=0;I<4096;++I){FVirtualLidarPoint P;Scan->ApplyPhysicalHitModel(P,Hit,FVector::ForwardVector,Scan->GetComponentTransform(),I,0);if(!P.bHit)++Misses;Signal+=P.NormalizedIntensity;Points.Add(P);}
        return Points;
    };
    int32 BrightMiss,DarkMiss,GrazingMiss,FarMiss,RepeatMiss;double BrightSignal,DarkSignal,GrazingSignal,FarSignal,RepeatSignal;
    Sample(.35,1,1000,BrightMiss,BrightSignal);
    const auto First=Sample(.08,1,1000,DarkMiss,DarkSignal);
    const auto Second=Sample(.08,1,1000,RepeatMiss,RepeatSignal);
    const auto Grazing=Sample(.08,.1,1000,GrazingMiss,GrazingSignal);Sample(.08,.1,4500,FarMiss,FarSignal);
    TestTrue(TEXT("reflectivity orders mean intensity and miss rate"),BrightSignal>DarkSignal&&BrightMiss<DarkMiss);
    TestTrue(TEXT("grazing incidence weakens intensity and increases misses"),DarkSignal>GrazingSignal&&DarkMiss<GrazingMiss);
    TestTrue(TEXT("distance weakens intensity and increases misses"),GrazingSignal>FarSignal&&GrazingMiss<FarMiss);
    for(int32 I=0;I<First.Num();++I){TestEqual(TEXT("seed repeats validity"),First[I].bHit,Second[I].bHit);TestEqual(TEXT("seed repeats noisy distance"),First[I].Distance,Second[I].Distance);}
    TestTrue(TEXT("incidence increases distance noise sigma"),VirtualLidarFidelity::NoiseScale(.5,.08,.1)>VirtualLidarFidelity::NoiseScale(.5,.08,1));
    const auto Rms=[](const TArray<FVirtualLidarPoint>& Points){double Sum=0;int32 N=0;for(const auto& P:Points)if(P.bHit){Sum+=FMath::Square((P.Distance-1000)*10.0);++N;}return FMath::Sqrt(Sum/FMath::Max(1,N));};
    TestTrue(TEXT("actual grazing noisy ranges have greater RMS error"),Rms(Grazing)>Rms(First));
    const FString Summary=FString::Printf(TEXT("CPU plane max error %.9f cm; misses/4096 bright=%d dark=%d grazing=%d far=%d\n"),MaxError,BrightMiss,DarkMiss,GrazingMiss,FarMiss);
    AddInfo(Summary);FFileHelper::SaveStringToFile(Summary,*(FPaths::ProjectSavedDir()/TEXT("Reports/LIDAR-FID-01/cpu-metrics.txt")));
    Motion->StopPlayback();Scan->StopScan();Actor->Destroy();Wall->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLidarFidelityGpuConversionTest,"MA0T10.LidarFidelity.GpuSnapshotConversion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLidarFidelityGpuConversionTest::RunTest(const FString&)
{
    auto* Scan=NewObject<UVirtualLidarScanComponent>();Scan->DeviceProfile=EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE;
    Scan->HorizontalSamples=32;Scan->VerticalChannels=8;Scan->HorizontalFov=10;Scan->MinVerticalAngle=-1;Scan->MaxVerticalAngle=1;
    Scan->MaxDistance=10000;Scan->ScanInterval=.05;Scan->bEnableRangeNoise=false;Scan->FidelityMode=EVirtualSensorFidelityMode::IdealTruth;
    Scan->bUseRollingPointTimestamps=true;Scan->bEnableMotionDistortion=true;
    FVirtualLidarDepthAcquisitionFrame Frame;Frame.Request.HorizontalSamples=32;Frame.Request.VerticalChannels=8;
    Frame.Request.HorizontalFovDegrees=10;Frame.Request.MinVerticalAngleDegrees=-1;Frame.Request.MaxVerticalAngleDegrees=1;
    Frame.CaptureWidth=32;Frame.CaptureHeight=16;Frame.ForwardDepthCentimeters.Init(1000,512);Frame.SemanticIdDepth.Init(FLinearColor(1,0,0,1000),512);
    FVirtualLidarGpuSemanticIdentity Identity;Identity.Label=TEXT("Slab");Identity.Motion.LinearVelocityCmPerSecond=FVector(1000,0,0);Identity.Surface.Reflectivity=.2;
    Frame.SemanticIdentities.Add(1,Identity);
    Scan->ConvertGpuDepthFrame(Frame);
    double MaxError=0;
    for(const auto& P:Scan->ScheduledPoints){TestTrue(TEXT("GPU synthetic plane hit"),P.bHit);MaxError=FMath::Max(MaxError,FMath::Abs(P.WorldLocation.X-(1000+1000*P.PointTimeOffsetNanoseconds*1.e-9)));}
    TestTrue(TEXT("GPU reconstructed plane and motion <=0.01cm"),MaxError<.01);
    Frame.SemanticIdentities[1].Motion=FVirtualSensorMotionSample();Scan->ConvertGpuDepthFrame(Frame);
    for(const auto& P:Scan->ScheduledPoints)TestTrue(TEXT("GPU static distortion zero"),FMath::Abs(P.WorldLocation.X-1000)<.001);
    Scan->bEnableMaterialResponse=true;Scan->ConvertGpuDepthFrame(Frame);const auto First=Scan->ScheduledPoints;
    Scan->ConvertGpuDepthFrame(Frame);
    for(int32 I=0;I<First.Num();++I){TestEqual(TEXT("GPU seed repeats hit"),First[I].bHit,Scan->ScheduledPoints[I].bHit);TestEqual(TEXT("GPU seed repeats intensity"),First[I].RawIntensity,Scan->ScheduledPoints[I].RawIntensity);}
    Frame.SemanticIdentities[1].Surface.Reflectivity=.08;Scan->ConvertGpuDepthFrame(Frame);
    double Bright=0,Dark=0;for(const auto& P:First)Bright+=P.RawIntensity;for(const auto& P:Scan->ScheduledPoints)Dark+=P.RawIntensity;
    TestTrue(TEXT("GPU uses acquisition optical snapshot"),Bright>Dark);
    AddInfo(FString::Printf(TEXT("Synthetic GPU conversion max plane error %.9f cm (not an RHI capture test)"),MaxError));
    return true;
}
#endif

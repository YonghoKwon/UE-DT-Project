#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualImuTypes.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualImuComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "Dom/JsonObject.h"

namespace
{
TArray<FVirtualImuSample> Motion(FVirtualImuConfig Config,FVector Accel=FVector::ZeroVector,FVector RotationAxis=FVector::UpVector,double Rate=0,bool Provider=false)
{
    FVirtualImuModel M;M.Reset(Config,0,FDateTime(2026,10,16));TArray<FVirtualImuSample> Out;
    const double Frames[]={.011,.027,.009,.031,.016};double T=0;int32 Frame=0;
    while(T<=3)
    {
        FVirtualImuObservation O;O.Seconds=T;O.Pose=FTransform(FQuat(RotationAxis,Rate*T),Accel*T*T*50.);
        O.HasVelocity=Provider;O.VelocityCm=Accel*T*100.;O.AngularVelocity=RotationAxis*Rate;M.Observe(O,-981,Out);T+=Frames[Frame++%5];
    }
    return Out;
}
double Variance(const TArray<double>& X)
{double A=0,B=0;for(double V:X){A+=V;B+=V*V;}return B/X.Num()-FMath::Square(A/X.Num());}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImuKinematicsTest,"MA0T10.IMU01.Kinematics",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FImuKinematicsTest::RunTest(const FString&)
{
    FVirtualImuConfig C;C.Noise=false;
    const auto Still=Motion(C);TestTrue(TEXT("many subframe samples"),Still.Num()>500);
    for(const auto& S:Still)if(!S.SpecificForce.Equals(FVector(0,0,9.81),1.e-8)||!S.AngularVelocity.IsNearlyZero(1.e-8)){AddError(TEXT("stationary specific force/gyro"));break;}
    for(bool Provider:{false,true})
    {
        const auto Moving=Motion(C,FVector(2,0,0),FVector::UpVector,PI/2,Provider);double Integral=0;
        for(const auto& S:Moving)
        {
            TestTrue(TEXT("rotation sign is right-handed FLU"),FMath::Abs(S.AngularVelocity.Z+PI/2)<1.e-7);
            const FVector World=S.Orientation.RotateVector(S.SpecificForce)+FVector(0,0,-9.81);
            if(!World.Equals(FVector(2,0,0),1.e-7)){AddError(TEXT("known acceleration in world"));break;}
            Integral+=World.X/C.RateHz;
        }
        TestTrue(TEXT("integral agrees with analytic delta-v"),FMath::Abs(Integral-2*Moving.Num()/C.RateHz)<1.e-7);
    }
    for(const auto& S:Motion(C,FVector(0,0,-9.81)))if(!S.SpecificForce.IsNearlyZero(1.e-7)){AddError(TEXT("free fall must measure zero specific force"));break;}
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImuClockTest,"MA0T10.IMU01.FixedClockAndDiscontinuities",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FImuClockTest::RunTest(const FString&)
{
    for(double Hz:{100.,200.,400.})
    {
        FVirtualImuConfig C;C.RateHz=Hz;C.Noise=false;const auto S=Motion(C);
        for(int32 I=1;I<S.Num();++I)if(S[I].Sequence!=S[I-1].Sequence+1||FMath::Abs(S[I].SimulationSeconds-S[I-1].SimulationSeconds-1/Hz)>1.e-9||FMath::Abs((S[I].TimestampUtc-S[I-1].TimestampUtc).GetTotalSeconds()-1/Hz)>1.e-7){AddError(TEXT("fixed-step drift"));break;}
        TestEqual(TEXT("no hidden sample gaps after warmup"),int64(S.Num()),S.Last().Sequence-S[0].Sequence+1);
    }
    FVirtualImuConfig C;C.Noise=false;FVirtualImuModel M;M.Reset(C,0,FDateTime(2026,10,16));TArray<FVirtualImuSample> S;
    for(double T:{0.,.02,.04,1.,1.02,1.04}){FVirtualImuObservation O;O.Seconds=T;M.Observe(O,-981,S);}
    TestEqual(TEXT("hitch counted"),M.Discontinuities,int64(1));TestTrue(TEXT("missed samples counted"),M.SkippedSamples>100);
    for(const auto& V:S)TestTrue(TEXT("no false acceleration on resume"),V.SpecificForce.Equals(FVector(0,0,9.81),1.e-8));
    FString Error;C.RateHz=401;TestFalse(TEXT("invalid rate rejected"),C.Validate(Error));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImuNoiseTest,"MA0T10.IMU01.NoiseStatisticsAndSeed",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FImuNoiseTest::RunTest(const FString&)
{
    FVirtualImuConfig C;C.AccelWalk=C.GyroWalk=0;C.ScaleError=C.MisalignmentDegrees=FVector::ZeroVector;
    FVirtualImuModel A,B;A.Reset(C,0,FDateTime(2026,10,16));B.Reset(C,0,FDateTime(2026,10,16));TArray<double> Acc,Gyro;
    for(int32 I=0;I<10000;++I)
    {
        FVirtualImuObservation O;O.Seconds=I*.01;TArray<FVirtualImuSample> X,Y;A.Observe(O,-981,X);B.Observe(O,-981,Y);
        if(X.Num()!=Y.Num()){AddError(TEXT("seed sample count"));break;}
        for(int32 J=0;J<X.Num();++J){if(X[J].SpecificForce!=Y[J].SpecificForce||X[J].AngularVelocity!=Y[J].AngularVelocity){AddError(TEXT("seed determinism"));break;}Acc.Add(X[J].SpecificForce.X);Gyro.Add(X[J].AngularVelocity.X);}
    }
    TestTrue(TEXT("accelerometer density sigma within 3%"),FMath::Abs(FMath::Sqrt(Variance(Acc))/(C.AccelDensity*FMath::Sqrt(C.RateHz))-1)<.03);
    TestTrue(TEXT("gyro density sigma within 3%"),FMath::Abs(FMath::Sqrt(Variance(Gyro))/(C.GyroDensity*FMath::Sqrt(C.RateHz))-1)<.03);
    C.AccelDensity=C.GyroDensity=0;C.AccelWalk=.01;C.GyroWalk=.001;Acc.Reset();Gyro.Reset();double Expected=0;
    for(int32 Seed=0;Seed<500;++Seed)
    {C.Seed=Seed;const auto S=Motion(C);Acc.Add(S.Last().SpecificForce.X);Gyro.Add(S.Last().AngularVelocity.X);Expected=S.Last().AccelVariance;}
    TestTrue(TEXT("bias drift ensemble variance within 20%"),FMath::Abs(Variance(Acc)/Expected-1)<.2);
    TestTrue(TEXT("gyro drift ensemble variance within 20%"),FMath::Abs(Variance(Gyro)/(Expected*.01)-1)<.2);
    C.Seed=5;const auto Other=Motion(C);C.Seed=6;TestFalse(TEXT("different seed differs"),Other.Last().SpecificForce==Motion(C).Last().SpecificForce);
    C.Noise=false;TestTrue(TEXT("noise disabled bypasses all errors"),Motion(C).Last().SpecificForce.Equals(FVector(0,0,9.81),1.e-8));
    C.Noise=true;C.AccelWalk=C.GyroWalk=0;C.ScaleError=FVector(0,0,.01);C.MisalignmentDegrees=FVector(1,0,0);
    const FVector ExpectedForce=FQuat(FVector::ForwardVector,FMath::DegreesToRadians(1.)).RotateVector(FVector(0,0,9.81*1.01));
    TestTrue(TEXT("scale and alignment are applied deterministically"),Motion(C).Last().SpecificForce.Equals(ExpectedForce,1.e-8));
    AddInfo(FString::Printf(TEXT("white sigma tolerance=3%%; bias variance tolerance=20%%; ensemble=500 seeds; observed bias variance=%.9g expected=%.9g"),Variance(Acc),Expected));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FImuOutputTest,"MA0T10.IMU01.OutputOptInAndBounds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FImuOutputTest::RunTest(const FString&)
{
    auto* P=NewObject<UVirtualSensorStreamPublisherComponent>();TestTrue(TEXT("no streams by default"),P->GetStreamStatuses().IsEmpty());
    P->SubmitImuBatch(TEXT("test.imu"),1,FDateTime::UtcNow(),TEXT("{}"));TestTrue(TEXT("disabled submission creates nothing"),P->GetStreamStatuses().IsEmpty());
    FVirtualSensorStreamConfig Config;Config.StreamKind=EVirtualSensorStreamKind::Imu;Config.SensorId=TEXT("test.imu");Config.bEnabled=true;P->ConfigureStream(Config);
    FVirtualSensorFrameEnvelope Frame;Frame.SensorId=Config.SensorId;Frame.FrameId=1;Frame.JsonPayload=MakeShared<const FString,ESPMode::ThreadSafe>(TEXT("{}"));P->SubmitFrame(Frame);
    TestEqual(TEXT("ordinary LiDAR cannot enter IMU stream"),P->GetStreamStatuses()[0].InputFrameCount,int64(0));
    P->SubmitImuBatch(Config.SensorId,1,FDateTime::UtcNow(),TEXT("{}"));TestEqual(TEXT("IMU route enabled explicitly"),P->GetStreamStatuses()[0].InputFrameCount,int64(1));
    auto* T=NewObject<UVirtualSensorTransportComponent>();TestEqual(TEXT("isolated topic"),T->ResolveDestination(TEXT("imu"),TEXT("imu-stream")),FString(TEXT("topic.virtual.sensor.imu.0")));
    FVirtualImuConfig C;C.Noise=false;const auto Samples=Motion(C);const auto Json=UVirtualImuComponent::BatchJson(Config.SensorId,C,Samples);
    TestEqual(TEXT("new schema only"),Json->GetStringField(TEXT("schemaVersion")),FString(TEXT("virtual-imu.v1")));
    auto* R=NewObject<UVirtualSensorRecorderComponent>();TArray<FVirtualImuSample> Large;Large.SetNum(24001);R->RecordImuSamples(Config.SensorId,Large);
    TestEqual(TEXT("bounded recorder"),R->GetImuSamples(Config.SensorId).Num(),24000);TestEqual(TEXT("overflow explicit"),R->GetImuOverflowCount(Config.SensorId),int64(1));
    return true;
}
#endif

#include "VirtualImuTypes.h"
#include "Dom/JsonObject.h"

namespace
{
FVector Polar(const FVector& V){return FVector(V.X,-V.Y,V.Z);}
FVector Axial(const FVector& V){return FVector(-V.X,V.Y,-V.Z);}
TArray<TSharedPtr<FJsonValue>> Values(std::initializer_list<double> V)
{TArray<TSharedPtr<FJsonValue>> R;for(double X:V)R.Add(MakeShared<FJsonValueNumber>(X));return R;}
}
bool FVirtualImuConfig::Validate(FString& Error) const
{
    if(!FMath::IsFinite(RateHz)||RateHz<100||RateHz>400||
       (Profile!=EVirtualImuProfile::Ideal&&Profile!=EVirtualImuProfile::IndustrialMems)||
       !FMath::IsFinite(AccelDensity)||!FMath::IsFinite(GyroDensity)||!FMath::IsFinite(AccelWalk)||!FMath::IsFinite(GyroWalk)||
       AccelDensity<0||GyroDensity<0||AccelWalk<0||GyroWalk<0||
       AccelDensity>10||GyroDensity>1||AccelWalk>1||GyroWalk>1||
       ScaleError.ContainsNaN()||MisalignmentDegrees.ContainsNaN()||ScaleError.GetAbsMax()>.1||MisalignmentDegrees.GetAbsMax()>10)
    {Error=TEXT("IMU: 주기 100~400 Hz, 유한한 노이즈·축 오차 값을 확인하세요.");return false;}
    Error.Reset();return true;
}
TSharedRef<FJsonObject> FVirtualImuSample::ToJson() const
{
    auto R=MakeShared<FJsonObject>();R->SetNumberField(TEXT("sequence"),Sequence);R->SetNumberField(TEXT("configurationRevision"),ConfigurationRevision);R->SetNumberField(TEXT("simulationSeconds"),SimulationSeconds);
    // FDateTime::ToIso8601 truncates to milliseconds; retain engine's 100 ns clock resolution.
    R->SetStringField(TEXT("timestampUtc"),TimestampUtc.ToString(TEXT("%Y-%m-%dT%H:%M:%S."))+FString::Printf(TEXT("%07lldZ"),TimestampUtc.GetTicks()%ETimespan::TicksPerSecond));
    R->SetArrayField(TEXT("specificForceMps2"),Values({SpecificForce.X,SpecificForce.Y,SpecificForce.Z}));
    R->SetArrayField(TEXT("angularVelocityRadps"),Values({AngularVelocity.X,AngularVelocity.Y,AngularVelocity.Z}));
    R->SetArrayField(TEXT("orientationXyzw"),Values({Orientation.X,Orientation.Y,Orientation.Z,Orientation.W}));
    R->SetArrayField(TEXT("positionWorldFluM"),Values({PositionMeters.X,PositionMeters.Y,PositionMeters.Z}));
    R->SetNumberField(TEXT("accelVariance"),AccelVariance);R->SetNumberField(TEXT("gyroVariance"),GyroVariance);return R;
}
void FVirtualImuModel::Reset(const FVirtualImuConfig& In,double E,FDateTime U)
{Config=In;Epoch=E;Utc=U;Next=1;BiasSteps=0;History.Reset();AccelBias=GyroBias=FVector::ZeroVector;Random.Initialize(Config.Seed);Discontinuities=SkippedSamples=0;}
double FVirtualImuModel::Gaussian()
{const double A=FMath::Max(1.e-7,double(Random.FRand()));return FMath::Sqrt(-2*FMath::Loge(A))*FMath::Cos(2*PI*Random.FRand());}
FVector FVirtualImuModel::Gaussian3(){const double X=Gaussian(),Y=Gaussian(),Z=Gaussian();return FVector(X,Y,Z);}
void FVirtualImuModel::Observe(const FVirtualImuObservation& O,double Gravity,TArray<FVirtualImuSample>& Out)
{
    if(!FMath::IsFinite(O.Seconds)||O.Pose.ContainsNaN()||!FMath::IsFinite(Gravity)||
       (O.HasVelocity&&(O.VelocityCm.ContainsNaN()||O.AngularVelocity.ContainsNaN())))return;
    if(History.Num()&&(O.Seconds<=History.Last().Seconds||O.Seconds-History.Last().Seconds>.25||
       FVector::Dist(O.Pose.GetLocation(),History.Last().Pose.GetLocation())>10000||
       (O.ObservationUtc.GetTicks()>0&&History.Last().ObservationUtc.GetTicks()>0&&
        ((O.ObservationUtc-History.Last().ObservationUtc).GetTotalSeconds()>.25||O.ObservationUtc<=History.Last().ObservationUtc))))
    {
        ++Discontinuities;History.Reset();
        const int64 Resume=FMath::Max<int64>(Next,FMath::FloorToInt64((O.Seconds-Epoch)*Config.RateHz)+1);
        SkippedSamples+=Resume-Next;Next=Resume;
    }
    History.Add(O);if(History.Num()<3)return;
    const auto A=History[0],B=History[1],C=History[2];const double D=C.Seconds-B.Seconds,P=B.Seconds-A.Seconds,Step=1/Config.RateHz;
    const FVector Secant=(C.Pose.GetLocation()-B.Pose.GetLocation())/D;
    const FVector Accel=B.HasVelocity&&C.HasVelocity?(C.VelocityCm-B.VelocityCm)/D:
        2*(Secant-(B.Pose.GetLocation()-A.Pose.GetLocation())/P)/(D+P);
    FQuat Delta=B.Pose.GetRotation().Inverse()*C.Pose.GetRotation();if(Delta.W<0)Delta=Delta*-1;Delta.Normalize();
    FVector Axis;double Angle;Delta.ToAxisAndAngle(Axis,Angle);const FVector LocalOmega=Axis*(Angle/D);
    Next=FMath::Max<int64>(Next,FMath::CeilToInt64((B.Seconds-Epoch)*Config.RateHz-1.e-7));
    for(;Epoch+Next*Step<=C.Seconds+1.e-9;++Next)
    {
        const double T=Epoch+Next*Step,Alpha=FMath::Clamp((T-B.Seconds)/D,0.,1.);const FQuat Q=FQuat::Slerp(B.Pose.GetRotation(),C.Pose.GetRotation(),Alpha).GetNormalized();
        FVirtualImuSample S;S.Sequence=Next;S.SimulationSeconds=T;S.TimestampUtc=Utc+FTimespan::FromSeconds(T-Epoch);
        if(B.ObservationUtc.GetTicks()>0&&C.ObservationUtc.GetTicks()>0)
            S.TimestampUtc=B.ObservationUtc+FTimespan(FMath::RoundToInt64((C.ObservationUtc-B.ObservationUtc).GetTicks()*Alpha));
        S.SpecificForce=Polar(Q.UnrotateVector((Accel-FVector(0,0,Gravity))*.01));
        S.AngularVelocity=Axial(B.HasVelocity&&C.HasVelocity?Q.UnrotateVector(FMath::Lerp(B.AngularVelocity,C.AngularVelocity,Alpha)):LocalOmega);
        S.Orientation=FQuat(-Q.X,Q.Y,-Q.Z,Q.W);
        const FVector Pos=B.HasVelocity&&C.HasVelocity?FMath::CubicInterp(B.Pose.GetLocation(),B.VelocityCm*D,C.Pose.GetLocation(),C.VelocityCm*D,Alpha):
            B.Pose.GetLocation()+(Secant-Accel*D*.5)*(T-B.Seconds)+.5*Accel*FMath::Square(T-B.Seconds);
        S.PositionMeters=Polar(Pos*.01);
        if(Config.Noise&&Config.Profile!=EVirtualImuProfile::Ideal)
        {
            AccelBias+=Gaussian3()*Config.AccelWalk*FMath::Sqrt(Step);GyroBias+=Gaussian3()*Config.GyroWalk*FMath::Sqrt(Step);
            const FQuat Mis(Config.MisalignmentDegrees.GetSafeNormal(),FMath::DegreesToRadians(Config.MisalignmentDegrees.Size()));
            S.SpecificForce=Mis.RotateVector(S.SpecificForce*(FVector::OneVector+Config.ScaleError))+AccelBias+Gaussian3()*Config.AccelDensity/FMath::Sqrt(Step);
            S.AngularVelocity=Mis.RotateVector(S.AngularVelocity*(FVector::OneVector+Config.ScaleError))+GyroBias+Gaussian3()*Config.GyroDensity/FMath::Sqrt(Step);
            const double Elapsed=++BiasSteps*Step;S.AccelVariance=FMath::Square(Config.AccelDensity)/Step+FMath::Square(Config.AccelWalk)*Elapsed;
            S.GyroVariance=FMath::Square(Config.GyroDensity)/Step+FMath::Square(Config.GyroWalk)*Elapsed;
        }
        Out.Add(S);
    }
    History.RemoveAt(0,1,false);
}

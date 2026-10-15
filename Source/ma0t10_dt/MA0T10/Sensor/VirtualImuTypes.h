#pragma once
#include "CoreMinimal.h"
#include "VirtualImuTypes.generated.h"

/** Generic simulation assumptions, not a calibrated hardware profile. */
UENUM(BlueprintType)
enum class EVirtualImuProfile : uint8 { Ideal, IndustrialMems };

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualImuConfig
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double RateHz=200;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EVirtualImuProfile Profile=EVirtualImuProfile::IndustrialMems;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool Noise=true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Seed=1337;
    // Continuous densities: sample sigma=density/sqrt(dt); bias increment sigma=walk*sqrt(dt).
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double AccelDensity=.002;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double GyroDensity=.00014;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double AccelWalk=.0002;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) double GyroWalk=.00002;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector ScaleError=FVector(.001,-.001,.0005);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector MisalignmentDegrees=FVector(.05,-.03,.02);
    bool Validate(FString& Error) const;
};

/** Sensor FLU (X forward,Y left,Z up), world FLU (UE world Y reflected).
 * Specific force R^-1(a-g) [m/s^2], right-handed angular rate [rad/s].
 * Orientation is simulator truth, not an integrated/noisy attitude estimator.
 */
USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualImuSample
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int64 Sequence=0;
    UPROPERTY(BlueprintReadOnly) int32 ConfigurationRevision=0;
    UPROPERTY(BlueprintReadOnly) double SimulationSeconds=0;
    UPROPERTY(BlueprintReadOnly) FDateTime TimestampUtc;
    UPROPERTY(BlueprintReadOnly) FVector SpecificForce=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector AngularVelocity=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FQuat Orientation=FQuat::Identity;
    UPROPERTY(BlueprintReadOnly) FVector PositionMeters=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) double AccelVariance=0;
    UPROPERTY(BlueprintReadOnly) double GyroVariance=0;
    TSharedRef<class FJsonObject> ToJson() const;
};

/** Game-thread pose observation. Velocities are optional world rigid motion at the sensor origin. */
struct FVirtualImuObservation
{
    double Seconds=0;
    FDateTime ObservationUtc;
    FTransform Pose=FTransform::Identity;
    bool HasVelocity=false;
    FVector VelocityCm=FVector::ZeroVector,AngularVelocity=FVector::ZeroVector;
};

/** Fixed simulation-time grid reconstructed from game-frame observations; never a wall-clock timer.
 * Three-pose quadratic translation / shortest-arc orientation; provider velocities take precedence.
 * Cannot reconstruct motion above the game observation bandwidth. First interval is warmup.
 */
class MA0T10_DT_API FVirtualImuModel
{
public:
    void Reset(const FVirtualImuConfig& InConfig,double EpochSeconds,FDateTime EpochUtc);
    void Observe(const FVirtualImuObservation& Observation,double GravityZCm,TArray<FVirtualImuSample>& Out);
    int64 Discontinuities=0,SkippedSamples=0;
private:
    FVirtualImuConfig Config;
    FRandomStream Random;
    TArray<FVirtualImuObservation,TInlineAllocator<3>> History;
    FVector AccelBias=FVector::ZeroVector,GyroBias=FVector::ZeroVector;
    double Epoch=0;
    FDateTime Utc;
    int64 Next=1;
    int64 BiasSteps=0;
    double Gaussian();
    FVector Gaussian3();
};

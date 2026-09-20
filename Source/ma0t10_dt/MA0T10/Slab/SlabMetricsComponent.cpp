#include "SlabMetricsComponent.h"
USlabMetricsComponent::USlabMetricsComponent() { PrimaryComponentTick.bCanEverTick=false; }
FSlabMetrics USlabMetricsComponent::Calculate(const FSlabScenarioRow& Row,const FTransform& SlabWorldTransform,const FVector& SizeCm,
	const FTransform& TrackWorldTransform,double LeftRailYcm,double RightRailYcm,bool bRailsConfigured)
{
	FSlabMetrics M; M.FrameNo=Row.FrameNo; M.ElapsedSec=Row.ElapsedSec; M.LeftAngle=Row.LeftAngle; M.RightAngle=Row.RightAngle;
	const FVector Centre=TrackWorldTransform.InverseTransformPositionNoScale(SlabWorldTransform.GetLocation()); M.CenterOffsetCm=Centre.Y;
	double MinY=DBL_MAX,MaxY=-DBL_MAX;
	for(int32 X:{-1,1}) for(int32 Y:{-1,1})
	{
		const FVector Corner=TrackWorldTransform.InverseTransformPositionNoScale(SlabWorldTransform.TransformPositionNoScale(FVector(X*SizeCm.X/2,Y*SizeCm.Y/2,0)));
		MinY=FMath::Min(MinY,Corner.Y); MaxY=FMath::Max(MaxY,Corner.Y);
	}
	M.MaxCornerOffsetCm=FMath::Max(FMath::Abs(MinY),FMath::Abs(MaxY));
	M.bMarginsValid=bRailsConfigured&&FMath::IsFinite(LeftRailYcm)&&FMath::IsFinite(RightRailYcm)&&LeftRailYcm<RightRailYcm;
	if(M.bMarginsValid) { M.MarginLeftCm=MinY-LeftRailYcm; M.MarginRightCm=RightRailYcm-MaxY; }
	return M;
}

#include "CoilLoadingModel.h"

namespace CoilLoading
{
FCylinderFit FitCylinder(const FPlacement& P,const TArray<FVector>& Points)
{
    FCylinderFit Result;Result.Center=P.Center;if(Points.Num()<20)return Result;
    const double Outer=P.Spec.OuterDiameter()*.5,Inner=P.Spec.ClearBore()*.5,Half=P.Spec.OverallWidth()*.5;
    const FQuat Rotation=FRotator(0,P.bEyeForeAft?0:90,0).Quaternion();FVector Offset=FVector::ZeroVector;
    TArray<FVector> Local;Local.Reserve(Points.Num());for(const auto& V:Points)if(!V.ContainsNaN())Local.Add(Rotation.UnrotateVector(V-P.Center));
    double LastDet=0,LastAxis=0;
    for(int32 Iteration=0;Iteration<12;++Iteration)
    {
        double XX=0,YY=0,ZZ=0,YZ=0,BX=0,BY=0,BZ=0;int32 Used=0;
        for(const FVector& V:Local)
        {
            const FVector Q=V-Offset;const double Radius=FVector2D(Q.Y,Q.Z).Size();if(Radius<1.e-6)continue;
            const double A=FMath::Abs(FMath::Abs(Q.X)-Half),B=FMath::Abs(Radius-Outer),C=FMath::Abs(Radius-Inner);
            if(FMath::Min3(A,B,C)>.25)continue;
            const bool Cap=A<FMath::Min(B,C)&&Radius>Inner-.03&&Radius<Outer+.03;
            if(!Cap&&FMath::Abs(Q.X)>Half+.05)continue;
            const double Residual=Cap?FMath::Abs(Q.X)-Half:Radius-(B<C?Outer:Inner);
            const double Weight=FMath::Min(1.,.01/FMath::Max(.000001,FMath::Abs(Residual)));
            if(Cap){const double J=Q.X>=0?-1:1;XX+=Weight;BX+=Weight*J*Residual;}
            else {const double JY=-Q.Y/Radius,JZ=-Q.Z/Radius;YY+=Weight*JY*JY;ZZ+=Weight*JZ*JZ;YZ+=Weight*JY*JZ;BY+=Weight*JY*Residual;BZ+=Weight*JZ*Residual;}
            ++Used;
        }
        LastDet=YY*ZZ-YZ*YZ;LastAxis=XX;Result.UsedPoints=Used;if(Used<20||LastDet<.01||XX<3)return Result;
        FVector Step(-BX/XX,(-BY*ZZ+BZ*YZ)/LastDet,(-BZ*YY+BY*YZ)/LastDet);Step=Step.GetClampedToMaxSize(.1);Offset+=Step;if(Step.Size()<1.e-6)break;
    }
    TArray<double> Residuals;
    for(const auto& V:Local)
    {
        const auto Q=V-Offset;const double Radius=FVector2D(Q.Y,Q.Z).Size();
        const double Side=FMath::Abs(Q.X)<=Half+.025?FMath::Min(FMath::Abs(Radius-Outer),FMath::Abs(Radius-Inner)):DBL_MAX;
        const double End=Radius>=Inner-.025&&Radius<=Outer+.025?FMath::Abs(FMath::Abs(Q.X)-Half):DBL_MAX;
        const double E=FMath::Min(Side,End);if(E<.05)Residuals.Add(E);
    }
    if(Residuals.Num()<20||LastDet<.01||LastAxis<3||Offset.Size()>.25)return Result;
    Residuals.Sort();Result.ResidualP95=Residuals[FMath::Min(Residuals.Num()-1,FMath::FloorToInt(Residuals.Num()*.95))];Result.Center=P.Center+Rotation.RotateVector(Offset);
    Result.bValid=Result.ResidualP95<=.02;return Result;
}
}

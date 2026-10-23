#pragma once
#include "CoreMinimal.h"
#include "Layout/SlateRect.h"

/** Display roles only. Indices remain the physical installation/TF order. */
namespace ShipHatchLabelPresentation
{
    inline const TCHAR* SensorRole(int32 Index)
    {
        const TCHAR* Roles[]={TEXT("붐"),TEXT("운전실 좌"),TEXT("운전실 우")};
        return Index>=0&&Index<UE_ARRAY_COUNT(Roles)?Roles[Index]:TEXT("");
    }

    inline const FString& FrameLabel(int32 Index)
    {
        static const FString Labels[]={TEXT("모델 원점"),TEXT("하역기"),TEXT("대차"),TEXT("운전실"),TEXT("붐"),TEXT("운전실 좌"),TEXT("운전실 우"),TEXT("그랩"),TEXT("")};
        return Labels[Index>=0&&Index<8?Index:8];
    }

    inline const TCHAR* FrameId(int32 Index)
    {
        const TCHAR* Ids[]={TEXT("Mesh_origin"),TEXT("Crane"),TEXT("Trolley"),TEXT("Cabin"),TEXT("os_lidar_boom"),TEXT("os_lidar_cabin_01"),TEXT("os_lidar_cabin_02"),TEXT("Grab")};
        return Index>=0&&Index<UE_ARRAY_COUNT(Ids)?Ids[Index]:TEXT("");
    }

    inline FString SensorTooltip(int32 Index)
    {
        return FString::Printf(TEXT("%s · TF %s\n좌우는 고정 설치 역할이며 화면의 좌우 순서가 아닙니다. 클릭하면 해당 센서를 강조합니다."),SensorRole(Index),FrameId(Index+4));
    }

    inline FString AcquisitionRates(const double* Goals,const double* Actual)
    {
        return FString::Printf(TEXT("● 획득 Hz · 목표/실제\n붐 %.0f/%.1f · 운전실 좌 %.0f/%.1f · 운전실 우 %.0f/%.1f"),Goals[0],Actual[0],Goals[1],Actual[1],Goals[2],Actual[2]);
    }

    inline bool Intersects(const FSlateRect& A,const FSlateRect& B)
    { return A.Left<B.Right&&A.Right>B.Left&&A.Top<B.Bottom&&A.Bottom>B.Top; }

    /** Bounded, allocation-free placement. Call selected sensors first, then
     * other sensors, then machinery. Omit a lower priority caption if full;
     * never move its TF/axis or draw overlapping text to imply a new pose. */
    inline bool PlaceLabel(const FSlateRect& Bounds,FVector2D Anchor,FVector2D Extent,float Scale,
        const FSlateRect* Occupied,int32 Count,FSlateRect& Result)
    {
        const double Gap=6*Scale;
        const FSlateRect Inner(Bounds.Left+Gap,Bounds.Top+Gap,Bounds.Right-Gap,Bounds.Bottom-Gap);
        if(!Bounds.ContainsPoint(Anchor)||Extent.X>Inner.Right-Inner.Left||Extent.Y>Inner.Bottom-Inner.Top)return false;
        const auto Try=[&](FVector2D P)
        {
            P.X=FMath::Clamp(P.X,double(Inner.Left),double(Inner.Right)-Extent.X);
            P.Y=FMath::Clamp(P.Y,double(Inner.Top),double(Inner.Bottom)-Extent.Y);
            const FSlateRect Candidate(P,P+Extent);
            const FSlateRect Padded(P-FVector2D(Gap*.5,Gap*.5),P+Extent+FVector2D(Gap*.5,Gap*.5));
            for(int32 I=0;I<Count;++I)if(Intersects(Padded,Occupied[I]))return false;
            Result=Candidate;return true;
        };
        for(int32 Row=0;Row<8;++Row)
        {
            const int32 Offset=Row==0?0:(Row%2?-(Row+1)/2:Row/2);
            for(int32 Side:{1,-1})if(Try(FVector2D(Anchor.X+(Side>0?Gap:-Extent.X-Gap),Anchor.Y-Extent.Y-Gap+Offset*(Extent.Y+Gap))))return true;
        }
        // Edge slots are a fallback for clustered TF origins, still bounded.
        for(double Y=Inner.Top;Y+Extent.Y<=Inner.Bottom;Y+=Extent.Y+Gap)
            for(double X:{double(Inner.Left),double(Inner.Right)-Extent.X})if(Try(FVector2D(X,Y)))return true;
        return false;
    }
}

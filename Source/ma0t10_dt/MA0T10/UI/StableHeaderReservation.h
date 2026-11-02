#pragma once
#include "CoreMinimal.h"

/** Grow immediately; release space only after an unchanged desired height for five seconds. */
struct FStableHeaderReservation
{
    double Height=0,Desired=0,StableSince=0;
    void Reset(double Value,double Now){Height=Desired=Value;StableSince=Now;}
    double Update(double Value,double Now)
    {
        if(!FMath::IsNearlyEqual(Value,Desired,.1)){Desired=Value;StableSince=Now;}
        Height=FMath::Max(Height,Value);
        if(Now-StableSince>=5.)Height=Desired;
        return Height;
    }
};

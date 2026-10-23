#pragma once

#include "CoreMinimal.h"
#include "ma0t10_dt/MA0T10/Core/VirtualPcdBandwidthMetrics.h"

struct FVirtualPcdBandwidthPresentation
{
 int32 PointBytes=33, SensorCount=0, ActiveCount=0, UnmeasuredCount=0;
 double SelectedMaximum=0, TotalMaximum=0, BudgetMiB=80;
 FVirtualPcdBandwidthSample Selected, Total;
 MA0T10_DT_API FText Text() const;
};

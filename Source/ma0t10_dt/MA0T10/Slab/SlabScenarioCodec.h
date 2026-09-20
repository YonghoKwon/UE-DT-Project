#pragma once
#include "SlabScenarioTypes.h"

/** Pure parser/math; safe to invoke from DTCore's background ParseToStruct. */
struct MA0T10_DT_API FSlabScenarioCodec
{
	static bool Parse(const FString& Json,FSlabScenarioDataPtr& Out,FString& Error,bool bAllowMissingUuid=false);
	static bool Sample(const FSlabScenarioData& Data,double Time,FSlabScenarioRow& Out,int32& LowerIndex);
	static double ToCm(double Value,ESlabInputUnit Unit);
	static FString MakeSyntheticJson(const FString& UUID=TEXT("6e8a08df-ecf2-4227-92b3-198120075ddb"));
};

#pragma once

#include "CoreMinimal.h"

/** One fixed second of completed socket writes, including the PCD header.
 * Payload sums are matched to those same writes, never to the latest options. */
struct FVirtualPcdBandwidthSample
{
 int32 Frames = 0;
 int64 BodyBytes = 0, OriginalPointBytes = 0, EncodedPointBytes = 0;
 bool bComplete = true;
 void Add(const FVirtualPcdBandwidthSample& Other)
 {
  Frames += Other.Frames; BodyBytes += Other.BodyBytes;
  OriginalPointBytes += Other.OriginalPointBytes; EncodedPointBytes += Other.EncodedPointBytes;
  bComplete &= Other.bComplete;
 }
 double ReductionPercent() const { return OriginalPointBytes > 0 ? (1. - double(EncodedPointBytes) / OriginalPointBytes) * 100. : 0.; }
};

/** Game-thread consumer of existing worker events; bounded, no per-frame allocation.
 * Overflow is reported until the overwritten event leaves the one-second window. */
class FVirtualPcdBandwidthWindow
{
 struct FWrite { double Seconds=0; int64 Body=0, Original=0, Encoded=0; };
 static constexpr int32 Capacity = 512;
 FWrite Writes[Capacity];
 int32 Next = 0, Count = 0;
 double LostThroughSeconds = -1;
public:
 void Record(double Seconds, int64 Body, int64 Original, int64 Encoded)
 {
  if (Count == Capacity) LostThroughSeconds = FMath::Max(LostThroughSeconds, Writes[Next].Seconds);
  Writes[Next] = {Seconds, Body, Original, Encoded};
  Next = (Next + 1) % Capacity; Count = FMath::Min(Count + 1, Capacity);
 }
 FVirtualPcdBandwidthSample Read(double Now) const
 {
  FVirtualPcdBandwidthSample Result; Result.bComplete = LostThroughSeconds <= Now - 1.;
  for (int32 I=0; I<Count; ++I)
  {
   const auto& W=Writes[I];
   if (W.Seconds > Now - 1. && W.Seconds <= Now)
   { Result.bComplete &= W.Encoded>=0; ++Result.Frames; Result.BodyBytes+=W.Body; Result.OriginalPointBytes+=W.Original; Result.EncodedPointBytes+=W.Encoded; }
  }
  return Result;
 }
};

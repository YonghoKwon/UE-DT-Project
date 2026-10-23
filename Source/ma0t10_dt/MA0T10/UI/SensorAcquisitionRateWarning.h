#pragma once

#include "CoreMinimal.h"

// UI-only observation of completed frames. No acquisition or output policy changes.
class FSensorAcquisitionRateWarning
{
public:
    void Reset() { *this = FSensorAcquisitionRateWarning(); }

    void Observe(double Now, int64 FrameId, float RequestedHz, bool bEligible)
    {
        if (!bEligible || !FMath::IsFinite(Now) || !FMath::IsFinite(RequestedHz) || RequestedHz <= 0)
        {
            Reset();
            return;
        }
        if (StartedAt < 0 || Now < LastObservedAt || Now - LastObservedAt > 1.0 ||
            FrameId < LastFrameId || !FMath::IsNearlyEqual(RequestedHz, TargetHz))
        {
            Reset();
            StartedAt = Now;
            StartFrameId = FrameId;
            TargetHz = RequestedHz;
        }
        LastObservedAt = Now;
        LastFrameId = FrameId;
        const double Elapsed = Now - StartedAt;
        if (Elapsed < 5.0) return;

        const int64 Completed = FrameId - StartFrameId;
        ObservedHz = Completed / Elapsed;
        // One completion at the observation boundary is ambiguous. This avoids
        // warning on normal 2/3-frame pacing; this is not an acceptance gate.
        bBelowRequest = Completed + 1.0 < TargetHz * Elapsed * 0.99;
        StartedAt = Now;
        StartFrameId = FrameId;
    }

    bool IsBelowRequest() const { return bBelowRequest; }
    double GetObservedHz() const { return ObservedHz; }
    float GetRequestedHz() const { return TargetHz; }

private:
    double StartedAt = -1;
    double LastObservedAt = -1;
    int64 StartFrameId = 0;
    int64 LastFrameId = 0;
    float TargetHz = 0;
    double ObservedHz = 0;
    bool bBelowRequest = false;
};

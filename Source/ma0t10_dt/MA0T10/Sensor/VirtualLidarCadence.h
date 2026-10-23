#pragma once

#include <cmath>
#include <cstdint>

/** Pure admission clock. No UObject, world, worker, or acquisition metadata ownership. */
class FVirtualLidarCadence
{
public:
    struct FDecision
    {
        bool bAdmit = false;
        std::uint64_t SkippedSlots = 0;
    };

    void Start(double Now, double Interval, double Phase = 0.0)
    {
        Period = SafePeriod(Interval);
        Origin = Now + Phase * Period;
        NextSlot = 0;
        bRunning = true;
        bHeld = false;
        bSuspended = false;
        // Keep the frame latch and cumulative skipped count across restart.
    }

    void Stop() { bRunning = false; bSuspended = false; }
    // Equal phases share the same wall-clock period grid even when Start calls
    // have different timestamps. The first due slot never predates Start.
    void StartAligned(double Now, double Interval, double Phase)
    {
        if (!std::isfinite(Now) || !std::isfinite(Interval)) { Stop(); return; }
        const double P = SafePeriod(Interval);
        Start(std::floor(Now / P) * P, Interval, Phase);
        if (GetNextDue() + P * 1.e-6 < Now) ++NextSlot;
    }
    bool IsRunning() const { return bRunning; }
    void HoldAdmissions() { bHeld = true; } // Dataset one-shot still needs completion polling.
    bool RetryHeldAdmission(double Now)
    {
        if (!bRunning || !bHeld) return false;
        bHeld = false;
        RequestImmediate(Now);
        return true;
    }
    std::uint64_t GetSkippedSlots() const { return TotalSkipped; }
    double GetNextDue() const { return Origin + static_cast<double>(NextSlot) * Period; }

    void RequestImmediate(double Now)
    {
        if (!bRunning || bHeld) return;
        Origin = bSuspended ? SuspendedAt : Now;
        NextSlot = 0;
    }

    void Suspend(double Now)
    {
        if (bRunning && !bSuspended) { SuspendedAt = Now; bSuspended = true; }
    }

    FDecision Poll(double Now, double Interval, std::uint64_t EngineFrame, bool bBusy)
    {
        FDecision Result;
        if (!bRunning || bHeld || !std::isfinite(Now) || !std::isfinite(Interval)) return Result;
        if (bSuspended)
        {
            Origin += Now - SuspendedAt; // Paused wall time produces no debt.
            bSuspended = false;
        }
        const double NewPeriod = SafePeriod(Interval);
        if (NewPeriod != Period)
        {
            Period = NewPeriod;
            Origin = Now; // A changed request starts a new grid, without old-rate debt.
            NextSlot = 0;
        }
        // Compute from a fixed epoch instead of repeatedly adding a period or
        // resetting to Now+Period: frame jitter must not accumulate into drift.
        const double SlotPosition = (Now - Origin) / Period;
        constexpr double BoundaryTolerance = 1.e-6; // <=1 ns at the minimum period.
        if (SlotPosition + BoundaryTolerance < static_cast<double>(NextSlot)) return Result;
        const auto LatestSlot = static_cast<std::uint64_t>(std::floor(SlotPosition + BoundaryTolerance));
        Result.SkippedSlots = LatestSlot - NextSlot;
        TotalSkipped += Result.SkippedSlots;
        NextSlot = LatestSlot; // Retain only the newest due slot while busy.
        if (!bBusy && (!bHasAdmitted || LastAdmissionFrame != EngineFrame))
        {
            Result.bAdmit = true;
            ++NextSlot;
            LastAdmissionFrame = EngineFrame;
            bHasAdmitted = true;
        }
        return Result;
    }

private:
    static double SafePeriod(double Interval) { return Interval > .001 ? Interval : .001; }
    double Origin = 0.0, Period = .05, SuspendedAt = 0.0;
    std::uint64_t NextSlot = 0, TotalSkipped = 0, LastAdmissionFrame = 0;
    bool bRunning = false, bHeld = false, bSuspended = false, bHasAdmitted = false;
};

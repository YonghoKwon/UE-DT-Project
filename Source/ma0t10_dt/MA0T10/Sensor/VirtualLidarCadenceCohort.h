#pragma once

#include <cstdint>
#include <vector>

/** Membership is supplied in case-sensitive SensorId order (stable identity breaks ties).
 * Only changed period groups are reassigned. Generation distinguishes a same-frame restart.
 * No world, acquisition, or metadata ownership. */
class FVirtualLidarCadenceCohort
{
public:
    struct FMember
    {
        std::uint64_t Key, Generation;
        double Interval;
        bool operator==(const FMember& R) const
        { return Key == R.Key && Generation == R.Generation && Interval == R.Interval; }
    };
    struct FAssignment { std::uint64_t Key; double Phase; bool bInitial; };

    const std::vector<FAssignment>& Update(const std::vector<FMember>& Members)
    {
        Assignments.clear();
        if (Members == Previous) return Assignments;
        for (const auto& M : Members)
        {
            std::vector<FMember> OldGroup, NewGroup;
            bool bPreviouslyKnown = false;
            std::size_t Rank = 0;
            for (const auto& P : Previous)
            {
                if (P.Interval == M.Interval) OldGroup.push_back(P);
                bPreviouslyKnown |= P.Key == M.Key;
            }
            for (const auto& N : Members)
                if (N.Interval == M.Interval)
                {
                    if (N.Key == M.Key) Rank = NewGroup.size();
                    NewGroup.push_back(N);
                    for (const auto& P : Previous) bPreviouslyKnown |= P.Key == N.Key;
                }
            if (OldGroup != NewGroup)
                Assignments.push_back({M.Key, double(Rank) / NewGroup.size(), OldGroup.empty() && !bPreviouslyKnown});
        }
        Previous = Members;
        return Assignments;
    }
private:
    std::vector<FMember> Previous;
    std::vector<FAssignment> Assignments;
};

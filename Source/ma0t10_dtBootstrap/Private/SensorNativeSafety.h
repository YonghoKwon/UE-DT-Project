#pragma once
#include "CoreMinimal.h"
#include <atomic>

namespace SensorNativeSafety
{
    class FSessionGuard
    {
        FString Path;
        TUniquePtr<FArchive> Lease;
        std::atomic<bool> Owned{false};
    public:
        explicit FSessionGuard(FString InPath):Path(MoveTemp(InPath)){}
        bool IsOwned() const {return Owned.load();}
        bool CanStartDefault() const;
        bool Begin(FString& Error);
        void End(bool Healthy);
    };
    bool IsValidatedConfiguration(uint32 VendorId,const FString& Adapter,const FString& Driver);
    bool CanAttempt(bool Explicit);
    bool BeginSession(FString& Error);
    void DisableForSession(const FString& Reason);
    void EndSession(bool Healthy);
}

#pragma once
#include "CoreMinimal.h"
#include <atomic>

namespace SensorNativeSafety
{
    class FSessionGuard
    {
        FString Path;
        FString LegacyPath;
        TUniquePtr<FArchive> Lease;
        std::atomic<bool> Owned{false};
        bool Recovery=false,Validated=false;
        FString LastDecision;
        int32 Failures=0;
        void SaveState();
    public:
        explicit FSessionGuard(FString InPath,FString InLegacyPath={}):Path(MoveTemp(InPath)),LegacyPath(MoveTemp(InLegacyPath)){}
        bool IsOwned() const {return Owned.load();}
        bool CanStartDefault() const;
        bool Begin(FString& Error);
        void End(bool Healthy);
        bool IsRecovery() const {return Recovery;}
        bool IsBanned() const {return Failures>=2;}
        int32 FailureCount() const {return Failures;}
        bool HasValidation() const {return Validated&&!IsBanned();}
        void Validate();
        void Invalidate();
        void RecordDecision(const FString& Reason);
    };
    bool IsValidatedConfiguration(uint32 VendorId,const FString& Adapter,const FString& Driver);
    bool IsProvenEngineDepthBarrier(uint32 MessageId,const FString& Description);
    bool CanAttempt(bool Explicit);
    bool BeginSession(FString& Error);
    bool NeedsValidation();
    void CompleteValidation(bool Passed,const FString& Reason);
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    void RequestValidationForTests();
#endif
    void DisableForSession(const FString& Reason);
    void EndSession(bool Healthy);
}

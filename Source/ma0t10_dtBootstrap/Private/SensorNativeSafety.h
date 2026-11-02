#pragma once
#include "CoreMinimal.h"
#include "SensorNativeSafetyStatus.h"
#include <atomic>

namespace SensorNativeSafety
{
    class FSessionGuard
    {
        FString Path;
        FString LegacyPath;
        TUniquePtr<FArchive> Lease;
        std::atomic<bool> Owned{false},Unhealthy{false};
        bool Recovery=false,Validated=false,RevalidationRequired=false;
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
        EStatus GetStatus() const {return IsBanned()?EStatus::Banned:(Recovery?EStatus::Recovery:EStatus::None);}
        int32 FailureCount() const {return Failures;}
        bool HasDeviceFault() const {return Unhealthy.load();}
        bool HasValidation() const {return Validated&&!IsBanned();}
        bool RequiresValidation() const {return RevalidationRequired;}
        void Validate();
        void Invalidate();
        void DisableForSession(bool DeviceHealthy,const FString& Reason);
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

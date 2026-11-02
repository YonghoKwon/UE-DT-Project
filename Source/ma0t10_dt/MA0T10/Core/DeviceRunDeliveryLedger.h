#pragma once
#include "CoreMinimal.h"
/** Per-run output failures; no scenario, slab, UI, UObject or transport ownership. */
class FDeviceRunDeliveryLedger
{
public:
    int32 Num() const {return Failures.Num();}
    void Reset() { Failures.Reset(); }
    void Record(const FString& RunKey,const FString& SensorKey,uint8 OutputKind,int64 FrameId)
    {
        if(RunKey.IsEmpty())return;
        if(!Failures.Contains(RunKey)&&Failures.Num()>=32)
            for(auto It=Failures.CreateIterator();It;++It)if(It.Key()!=RunKey){It.RemoveCurrent();break;}
        Failures.FindOrAdd(RunKey).Add(SensorKey+TEXT("|")+LexToString(OutputKind)+TEXT("|")+LexToString(FrameId));
    }
    int64 Count(const FString& RunKey) const
    {const auto* Found=Failures.Find(RunKey);return Found?Found->Num():0;}
private:
    TMap<FString,TSet<FString>> Failures;
};

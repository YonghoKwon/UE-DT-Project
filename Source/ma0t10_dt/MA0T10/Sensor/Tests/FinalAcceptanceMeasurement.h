#pragma once
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorExternalSourceHostActor.h"
#include "Json.h"
#include "Misc/FileHelper.h"

class FFinalAcceptanceMeasurement
{
    struct FRow {FVirtualSensorTransportObservation Accepted;double Submit=0,Receipt=0,Consumed=0;float SocketMs=0,ReceiptMs=0,E2eMs=0;bool ClockValid=true,UiValidated=false;};
    TMap<FString,FRow> Rows;
    TWeakObjectPtr<UVirtualSensorHighThroughputTransportSubsystem> Raw;
    FDelegateHandle Handle;
    TWeakObjectPtr<AVirtualSensorExternalSourceHostActor> UiHost;FDelegateHandle UiHandle;bool RequireUi=false;
    double Begin=0,End=0;
    bool LedgerOverflow=false;
    static FString Key(const FVirtualSensorTransportObservation& O){return FString::Printf(TEXT("%d|%s|%lld"),int32(O.Kind),*O.SensorId,O.FrameId);}
public:
    ~FFinalAcceptanceMeasurement(){if(Raw.IsValid())Raw->OnTransportObservation.Remove(Handle);if(UiHost.IsValid())UiHost->OnUiReceiveCompletedNative.Remove(UiHandle);}
    bool IsAcceptedSetComplete() const
    {
        if(LedgerOverflow)return false;
        for(const auto& P:Rows)
        {
            const auto& R=P.Value;if(R.Submit<=0||R.Receipt<=0||R.Consumed<=0||!R.ClockValid||(RequireUi&&!R.UiValidated))return false;
        }
        return !Rows.IsEmpty();
    }
    bool IsContiguousAndComplete() const
    {
        if(!IsAcceptedSetComplete())return false;
        TMap<FString,TArray<int64>> Ids;
        for(const auto& P:Rows)
        {
            const auto& R=P.Value;
            Ids.FindOrAdd(FString::Printf(TEXT("%d|%s"),int32(R.Accepted.Kind),*R.Accepted.SensorId)).Add(R.Accepted.FrameId);
        }
        if(Rows.IsEmpty())return false;
        for(auto& P:Ids){P.Value.Sort();for(int32 I=1;I<P.Value.Num();++I)if(P.Value[I]!=P.Value[I-1]+1)return false;}
        return true;
    }
    double GetRate(EVirtualSensorStreamKind Kind,const FString& Sensor=FString()) const
    {int32 Count=0;for(const auto& P:Rows)Count+=P.Value.Accepted.Kind==Kind&&(Sensor.IsEmpty()||P.Value.Accepted.SensorId==Sensor)?1:0;return Count/FMath::Max(.001,End-Begin);}
    int32 GetUiCount(EVirtualSensorStreamKind Kind) const {int32 N=0;for(const auto& P:Rows)N+=P.Value.Accepted.Kind==Kind&&P.Value.UiValidated;return N;}
    void Start(UVirtualSensorHighThroughputTransportSubsystem* In,double Start,double Duration,AVirtualSensorExternalSourceHostActor* Ui=nullptr)
    {
        Raw=In;Begin=Start;End=Start+Duration;
        UiHost=Ui;RequireUi=Ui!=nullptr;
        if(Ui)UiHandle=Ui->OnUiReceiveCompletedNative.AddLambda([this](const FVirtualSensorTopicReceivedDataBase& Data)
        {
            const FString K=FString::Printf(TEXT("%d|%s|%lld"),int32(Data.Kind),*Data.SensorId,Data.FrameId);
            if(auto* R=Rows.Find(K))R->UiValidated=Data.bValid&&Data.RequestId==R->Accepted.RequestId;
        });
        Handle=In->OnTransportObservation.AddLambda([this](const FVirtualSensorTransportObservation& O)
        {
            const auto K=Key(O);
            if(O.Phase==EVirtualSensorTransportObservationPhase::Accepted)
            {
                if(O.MonotonicSeconds>=Begin&&O.MonotonicSeconds<End)
                {if(Rows.Num()>=300000&&!Rows.Contains(K))LedgerOverflow=true;else Rows.FindOrAdd(K).Accepted=O;}
                return;
            }
            auto* R=Rows.Find(K);if(!R)return;
            if(O.Phase==EVirtualSensorTransportObservationPhase::Submitted){R->Submit=O.MonotonicSeconds;R->SocketMs=O.LatencyMs;R->Accepted.RequestId=O.RequestId;}
            if(O.Phase==EVirtualSensorTransportObservationPhase::Receipt){R->Receipt=O.MonotonicSeconds;R->ReceiptMs=O.LatencyMs;}
            if(O.Phase==EVirtualSensorTransportObservationPhase::Consumed){R->Consumed=O.MonotonicSeconds;R->ClockValid=O.bClockValid;R->E2eMs=float((O.ObservedUtc-O.AcquisitionUtc).GetTotalMilliseconds());}
        });
    }
    bool Save(const FString& Path,FIntPoint Viewport)
    {
        if(Path.IsEmpty())return true;
        auto Root=MakeShared<FJsonObject>();Root->SetNumberField(TEXT("measurement_start_monotonic"),Begin);Root->SetNumberField(TEXT("duration_seconds"),End-Begin);
        Root->SetBoolField(TEXT("record_capacity_exceeded"),LedgerOverflow);
        Root->SetNumberField(TEXT("actual_viewport_x"),Viewport.X);Root->SetNumberField(TEXT("actual_viewport_y"),Viewport.Y);
        TArray<TSharedPtr<FJsonValue>> Entries;
        for(const auto& Pair:Rows)
        {
            const auto& R=Pair.Value;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("request_id"),R.Accepted.RequestId);
            J->SetStringField(TEXT("sensor_id"),R.Accepted.SensorId);J->SetNumberField(TEXT("kind"),int32(R.Accepted.Kind));J->SetNumberField(TEXT("frame_id"),R.Accepted.FrameId);
            J->SetStringField(TEXT("acquisition_utc"),R.Accepted.AcquisitionUtc.ToIso8601());J->SetStringField(TEXT("accepted_utc"),R.Accepted.ObservedUtc.ToIso8601());J->SetNumberField(TEXT("accepted_monotonic"),R.Accepted.MonotonicSeconds);
            J->SetNumberField(TEXT("submit_monotonic"),R.Submit);J->SetNumberField(TEXT("receipt_monotonic"),R.Receipt);J->SetNumberField(TEXT("consumer_monotonic"),R.Consumed);
            J->SetNumberField(TEXT("socket_ms"),R.SocketMs);J->SetNumberField(TEXT("receipt_ms"),R.ReceiptMs);J->SetNumberField(TEXT("e2e_ms"),R.E2eMs);J->SetBoolField(TEXT("clock_valid"),R.ClockValid);
            J->SetBoolField(TEXT("ui_validated"),R.UiValidated);
            Entries.Add(MakeShared<FJsonValueObject>(J));
        }
        Root->SetArrayField(TEXT("frames"),Entries);FString Text;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text));
        return FFileHelper::SaveStringToFile(Text,*Path);
    }
};

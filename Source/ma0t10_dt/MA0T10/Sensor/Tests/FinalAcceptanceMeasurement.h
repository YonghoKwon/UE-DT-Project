#pragma once
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "Json.h"
#include "Misc/FileHelper.h"

class FFinalAcceptanceMeasurement
{
    struct FRow {FVirtualSensorTransportObservation Accepted;double Submit=0,Receipt=0,Consumed=0;float SocketMs=0,ReceiptMs=0,E2eMs=0;bool ClockValid=true;};
    TMap<FString,FRow> Rows;
    TWeakObjectPtr<UVirtualSensorHighThroughputTransportSubsystem> Raw;
    FDelegateHandle Handle;
    double Begin=0,End=0;
    static FString Key(const FVirtualSensorTransportObservation& O){return FString::Printf(TEXT("%d|%s|%lld"),int32(O.Kind),*O.SensorId,O.FrameId);}
public:
    ~FFinalAcceptanceMeasurement(){if(Raw.IsValid())Raw->OnTransportObservation.Remove(Handle);}
    void Start(UVirtualSensorHighThroughputTransportSubsystem* In,double Start,double Duration)
    {
        Raw=In;Begin=Start;End=Start+Duration;
        Handle=In->OnTransportObservation.AddLambda([this](const FVirtualSensorTransportObservation& O)
        {
            const auto K=Key(O);
            if(O.Phase==EVirtualSensorTransportObservationPhase::Accepted)
            {
                if(O.MonotonicSeconds>=Begin&&O.MonotonicSeconds<End&&Rows.Num()<300000)Rows.FindOrAdd(K).Accepted=O;
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
        Root->SetNumberField(TEXT("actual_viewport_x"),Viewport.X);Root->SetNumberField(TEXT("actual_viewport_y"),Viewport.Y);
        TArray<TSharedPtr<FJsonValue>> Entries;
        for(const auto& Pair:Rows)
        {
            const auto& R=Pair.Value;auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("request_id"),R.Accepted.RequestId);
            J->SetStringField(TEXT("sensor_id"),R.Accepted.SensorId);J->SetNumberField(TEXT("kind"),int32(R.Accepted.Kind));J->SetNumberField(TEXT("frame_id"),R.Accepted.FrameId);
            J->SetStringField(TEXT("acquisition_utc"),R.Accepted.AcquisitionUtc.ToIso8601());J->SetStringField(TEXT("accepted_utc"),R.Accepted.ObservedUtc.ToIso8601());J->SetNumberField(TEXT("accepted_monotonic"),R.Accepted.MonotonicSeconds);
            J->SetNumberField(TEXT("submit_monotonic"),R.Submit);J->SetNumberField(TEXT("receipt_monotonic"),R.Receipt);J->SetNumberField(TEXT("consumer_monotonic"),R.Consumed);
            J->SetNumberField(TEXT("socket_ms"),R.SocketMs);J->SetNumberField(TEXT("receipt_ms"),R.ReceiptMs);J->SetNumberField(TEXT("e2e_ms"),R.E2eMs);J->SetBoolField(TEXT("clock_valid"),R.ClockValid);
            Entries.Add(MakeShared<FJsonValueObject>(J));
        }
        Root->SetArrayField(TEXT("frames"),Entries);FString Text;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text));
        return FFileHelper::SaveStringToFile(Text,*Path);
    }
};

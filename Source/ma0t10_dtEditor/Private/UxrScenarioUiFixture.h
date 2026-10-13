#pragma once
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "Serialization/JsonSerializer.h"

/** Same memory-only input on both revisions. Uses the existing Actor admission path. */
inline bool StartUxrScenarioUiFixture(ASlabActor* Slab)
{
    if(!Slab)return false;
    TSharedPtr<FJsonObject> Root;
    const FString Source=FSlabScenarioCodec::MakeSyntheticJson(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source),Root))return false;
    auto Meta=Root->GetObjectField(TEXT("_meta"));Meta->SetNumberField(TEXT("duration_sec"),90);Meta->SetNumberField(TEXT("sample_period_sec"),.15);
    for(const auto& Value:Root->GetArrayField(TEXT("DATA_MAP"))){auto Row=Value->AsObject();Row->SetNumberField(TEXT("elapsed_sec"),Row->GetNumberField(TEXT("elapsed_sec"))*3);}
    FString Json;FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));
    Slab->SetSensorOutputs(FVirtualSlabSensorOutputSelection::ObservationOnly());
    return Slab->SubmitScenarioJson(Json);
}

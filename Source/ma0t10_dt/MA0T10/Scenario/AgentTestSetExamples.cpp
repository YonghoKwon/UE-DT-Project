#include "AgentTestSetExamples.h"
#include "VirtualDatasetTypes.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "Json.h"

int32 AgentTestSetExamples::SeedFor(const FString& ContentSha1,int32 Seed)
{
    // Stable content-derived seed, independent of process hash salts and generated archive IDs.
    uint32 Value=uint32(Seed);for(TCHAR C:ContentSha1)Value=Value*16777619u^uint32(C);return int32(Value&0x7fffffffu);
}
FString AgentTestSetExamples::Variation(int32 Seed,int32 Example,ESlabInputUnit Dimension,ESlabInputUnit Position)
{
    FVirtualDatasetSettings Settings;Settings.Seed=Seed;
    Settings.X.Enabled=Settings.Y.Enabled=Settings.Pitch.Enabled=Settings.Roll.Enabled=Settings.Size.Enabled=true;
    const auto A=FVirtualDatasetSample::Make(Settings,Example*4);
    const auto B=FVirtualDatasetSample::Make(Settings,Example*4+1);
    const auto C=FVirtualDatasetSample::Make(Settings,Example*4+2);
    const auto D=FVirtualDatasetSample::Make(Settings,Example*4+3);
    auto O=MakeShared<FJsonObject>();
    O->SetNumberField(TEXT("seed"),Seed);O->SetNumberField(TEXT("example_index"),Example);
    O->SetNumberField(TEXT("start_offset_cm"),A.SlabOffset.X);
    O->SetNumberField(TEXT("distance_multiplier"),D.Size);
    O->SetNumberField(TEXT("left_skew_offset_deg"),A.SlabRotation.Pitch);
    O->SetNumberField(TEXT("right_skew_offset_deg"),A.SlabRotation.Roll);
    O->SetNumberField(TEXT("length_multiplier"),A.Size);O->SetNumberField(TEXT("width_multiplier"),B.Size);O->SetNumberField(TEXT("thickness_multiplier"),C.Size);
    O->SetBoolField(TEXT("meandering"),Example%2!=0);
    O->SetNumberField(TEXT("meander_amplitude_deg"),Example%2?B.SlabRotation.Roll:0);
    O->SetNumberField(TEXT("dimension_unit"),int32(Dimension));O->SetNumberField(TEXT("position_unit"),int32(Position));
    // BuildPose uses center_x and left skew. center_y/mov_pos remain diagnostic source fields.
    O->SetStringField(TEXT("pose_rule"),TEXT("existing TrackReference + BuildPose(center_x,left_skew_angle,first-row size)"));
    FString Json;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Json));return Json;
}
bool AgentTestSetExamples::Generate(const FSlabScenarioData& Source,const FString& Id,const FString& VariationJson,FSlabScenarioDataPtr& Out,FString& Error)
{
    TSharedPtr<FJsonObject> Root,V;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source.OriginalJson),Root)||!Root||
       !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(VariationJson),V)||!V){Error=TEXT("예시 원본과 변형 값을 읽을 수 없습니다");return false;}
    auto Meta=Root->HasTypedField<EJson::Object>(TEXT("_meta"))?Root->GetObjectField(TEXT("_meta")):MakeShared<FJsonObject>();Root->SetObjectField(TEXT("_meta"),Meta);
    Meta->SetStringField(TEXT("UUID"),Id);Meta->SetStringField(TEXT("scenario"),TEXT("Example · ")+Source.Name);
    Meta->SetStringField(TEXT("source"),TEXT("example"));Meta->SetStringField(TEXT("source_uuid"),Source.ScenarioUUID);Meta->SetStringField(TEXT("source_sha1"),Source.OriginalContentSha1);Meta->SetObjectField(TEXT("example_variation"),V);
    const auto Dimension=ESlabInputUnit(int32(V->GetNumberField(TEXT("dimension_unit"))));
    const auto Position=ESlabInputUnit(int32(V->GetNumberField(TEXT("position_unit"))));
    const double Offset=V->GetNumberField(TEXT("start_offset_cm"))/FSlabScenarioCodec::ToCm(1,Position);
    const auto& First=Source.Rows[0];const double Scale=V->GetNumberField(TEXT("distance_multiplier"));
    // Perturb within DATASET-01's 0.9–1.1 range, capped to industrial slab dimensions in cm.
    auto Size=[&](double Value,const TCHAR* Key,double Min,double Max){return FMath::Clamp(FSlabScenarioCodec::ToCm(Value,Dimension)*V->GetNumberField(Key),Min,Max)/FSlabScenarioCodec::ToCm(1,Dimension);};
    const double Length=Size(First.Length,TEXT("length_multiplier"),100,3000),Width=Size(First.Width,TEXT("width_multiplier"),30,400),Thickness=Size(First.Thickness,TEXT("thickness_multiplier"),3,60);
    const double WeightScale=(Length/First.Length)*(Width/First.Width)*(Thickness/First.Thickness);
    auto& Rows=Root->GetArrayField(TEXT("DATA_MAP"));
    for(int32 I=0;I<Rows.Num();++I)
    {
        const auto& R=Source.Rows[I];auto O=Rows[I]->AsObject();
        const double Wave=FMath::Sin(Rows.Num()>1?double(I)/(Rows.Num()-1)*2*PI:0)*V->GetNumberField(TEXT("meander_amplitude_deg"));
        O->SetNumberField(TEXT("slab_len"),Length);O->SetNumberField(TEXT("slab_wth"),Width);O->SetNumberField(TEXT("slab_thk"),Thickness);O->SetNumberField(TEXT("slab_wt"),R.Weight*WeightScale);
        O->SetNumberField(TEXT("center_x"),First.CenterX+Offset+(R.CenterX-First.CenterX)*Scale);
        if(O->HasField(TEXT("mov_pos")))O->SetNumberField(TEXT("mov_pos"),First.MovePosition+Offset+(R.MovePosition-First.MovePosition)*Scale);
        O->SetNumberField(TEXT("left_skew_angle"),FMath::Clamp(R.LeftAngle+V->GetNumberField(TEXT("left_skew_offset_deg"))+Wave,-10.,10.));
        O->SetNumberField(TEXT("right_skew_angle"),FMath::Clamp(R.RightAngle+V->GetNumberField(TEXT("right_skew_offset_deg"))-Wave,-10.,10.));
        if(O->HasField(TEXT("is_meandering")))O->SetBoolField(TEXT("is_meandering"),V->GetBoolField(TEXT("meandering")));
    }
    FString Json;FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));return FSlabScenarioCodec::Parse(Json,Out,Error);
}
TArray<int32> AgentTestSetExamples::SampleRows(int32 Rows,int32 Stride,int32 Cap)
{
    TArray<int32> Result;const int32 Count=FMath::DivideAndRoundUp(Rows,FMath::Max(1,Stride));
    const int32 Samples=Cap>0?FMath::Min(Count,Cap):Count;Result.Reserve(Samples);
    for(int32 I=0;I<Samples;++I)Result.Add((Samples>1?int32(int64(I)*(Count-1)/(Samples-1)):Count/2)*FMath::Max(1,Stride));return Result;
}

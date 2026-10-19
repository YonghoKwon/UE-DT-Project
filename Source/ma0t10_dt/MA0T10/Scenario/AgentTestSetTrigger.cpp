#include "AgentTestSetTrigger.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

namespace
{
FString ReadMaterial(const TSharedPtr<FJsonObject>& O)
{
    if(!O)return FString();const auto V=O->TryGetField(TEXT("mtl_no"));if(!V)return FString();
    if(V->Type==EJson::String)return V->AsString().TrimStartAndEnd();
    return FString();
}
bool Unit(const TSharedPtr<FJsonObject>& Meta,const TCHAR* Key,ESlabInputUnit& Out)
{
    FString S;if(!Meta||!Meta->TryGetStringField(Key,S))return false;
    if(S==TEXT("cm"))Out=ESlabInputUnit::Centimeters;
    else if(S==TEXT("mm"))Out=ESlabInputUnit::Millimeters;
    else if(S==TEXT("m"))Out=ESlabInputUnit::Meters;
    else return false;return true;
}
}
FString AgentTestSetTrigger::Material(const FString& Json)
{
    // Bound work before deserializing. This lightweight envelope never validates scenario fields.
    if(Json.Len()>8*1024*1024||FTCHARToUTF8(*Json).Length()>8*1024*1024)return FString();
    TSharedPtr<FJsonObject> Root;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root)||!Root)return FString();
    FString Id;if(!Root->TryGetStringField(TEXT("MESSAGE_ID"),Id)||Id!=TEXT("IFactory-agent"))return FString();
    const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
    if(Root->TryGetArrayField(TEXT("DATA_MAP"),Rows)&&Rows&&!Rows->IsEmpty())
    {
        const TSharedPtr<FJsonObject>* First=nullptr;
        if((*Rows)[0]&&(*Rows)[0]->TryGetObject(First)&&First){const auto Key=ReadMaterial(*First);if(!Key.IsEmpty())return Key;}
    }
    const auto Key=ReadMaterial(Root);if(!Key.IsEmpty())return Key;
    const TSharedPtr<FJsonObject>* Meta=nullptr;return Root->TryGetObjectField(TEXT("_meta"),Meta)&&Meta?ReadMaterial(*Meta):FString();
}
bool AgentTestSetTrigger::LoadTemplate(const FString& Path,const FString& Trigger,ESlabInputUnit Dimension,ESlabInputUnit Position,FSlabScenarioDataPtr& Out,FString& Error)
{
    Out.Reset();const int64 Size=IFileManager::Get().FileSize(*Path);
    if(Size<=0||Size>8*1024*1024){Error=TEXT("기본 시나리오 템플릿 경로와 8MiB 한도를 확인하세요");return false;}
    FString Json;if(!FFileHelper::LoadFileToString(Json,*Path)){Error=TEXT("기본 시나리오 템플릿을 읽을 수 없습니다");return false;}
    FSlabScenarioDataPtr Source;if(!FSlabScenarioCodec::Parse(Json,Source,Error))return false;
    TSharedPtr<FJsonObject> Root;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root);
    const auto Meta=Root->GetObjectField(TEXT("_meta"));ESlabInputUnit D,P;
    if(!Unit(Meta,TEXT("dimension_unit"),D)||!Unit(Meta,TEXT("position_unit"),P)){Error=TEXT("템플릿의 dimension_unit/position_unit은 cm, mm, m 중 하나여야 합니다");return false;}
    const double DS=FSlabScenarioCodec::ToCm(1,D)/FSlabScenarioCodec::ToCm(1,Dimension),PS=FSlabScenarioCodec::ToCm(1,P)/FSlabScenarioCodec::ToCm(1,Position);
    for(const auto& Value:Root->GetArrayField(TEXT("DATA_MAP")))
    {
        const auto Row=Value->AsObject();Row->SetStringField(TEXT("mtl_no"),Trigger);
        for(const auto* K:{TEXT("slab_len"),TEXT("slab_wth"),TEXT("slab_thk")})Row->SetNumberField(K,Row->GetNumberField(K)*DS);
        for(const auto* K:{TEXT("center_x"),TEXT("center_y"),TEXT("mov_pos")})if(Row->HasField(K))Row->SetNumberField(K,Row->GetNumberField(K)*PS);
    }
    Meta->SetStringField(TEXT("template_file_sha1"),Source->OriginalContentSha1);
    Meta->SetNumberField(TEXT("applied_dimension_unit"),int32(Dimension));Meta->SetNumberField(TEXT("applied_position_unit"),int32(Position));
    FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Json));return FSlabScenarioCodec::Parse(Json,Out,Error);
}

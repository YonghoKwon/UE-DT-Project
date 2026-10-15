#include "AutomationProtocol.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/Archive.h"

namespace MA0T10Automation
{
FString Json(const TSharedRef<FJsonObject>& O)
{ FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));return Text; }
bool ParseObject(const FString& Text,TSharedPtr<FJsonObject>& O)
{return Text.Len()<=MaxDocumentBytes&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O)&&O.IsValid();}
bool ReadDocument(const FString& Path,FString& Text,int64 Limit)
{
    TUniquePtr<FArchive> File(IFileManager::Get().CreateFileReader(*Path,FILEREAD_Silent));
    if(!File)return false;const int64 Size=File->TotalSize();
    if(Size<=0||Size>Limit||Size>MAX_int32-2)return false;
    TArray<uint8> Bytes;Bytes.SetNumUninitialized(int32(Size)+2);Bytes[int32(Size)]=Bytes[int32(Size)+1]=0;
    File->Serialize(Bytes.GetData(),Size);if(File->IsError())return false;
    // Read the admitted snapshot only, even if another process grows the file.
    FFileHelper::BufferToString(Text,Bytes.GetData(),int32(Size));return true;
}
static bool Fields(const FJsonObject& O,const FString& Allowed)
{TArray<FString> Keys;Allowed.ParseIntoArray(Keys,TEXT("|"));for(const auto& F:O.Values)if(!Keys.Contains(F.Key))return false;return true;}
static bool String(const FJsonObject& O,const TCHAR* Key,bool Required=false)
{FString V;return !O.HasField(Key)?!Required:O.TryGetField(Key)->Type==EJson::String&&O.TryGetStringField(Key,V)&&!V.IsEmpty()&&V.Len()<=1024;}
static bool Number(const FJsonObject& O,const TCHAR* Key,double Min,double Max,bool Integer=false)
{if(!O.HasField(Key))return true;double V;return O.TryGetField(Key)->Type==EJson::Number&&O.TryGetNumberField(Key,V)&&FMath::IsFinite(V)&&V>=Min&&V<=Max&&(!Integer||FMath::FloorToDouble(V)==V);}
static bool Boolean(const FJsonObject& O,const TCHAR* Key)
{bool V;return !O.HasField(Key)||(O.TryGetField(Key)->Type==EJson::Boolean&&O.TryGetBoolField(Key,V));}
bool ValidateCommand(const FJsonObject& C,FString& Error)
{
    Error=TEXT("Invalid command fields or values");FString Op;if(!String(C,TEXT("op"),true)||!C.TryGetStringField(TEXT("op"),Op))return false;
    FString Allowed=TEXT("op|timeout_sec");bool Valid=Number(C,TEXT("timeout_sec"),1,86400);
    if(Op==TEXT("sensor.set"))
    {
        Allowed+=TEXT("|sensor_id|interval_sec|profile|quality");
        Valid&=String(C,TEXT("sensor_id"),true)&&String(C,TEXT("profile"))&&String(C,TEXT("quality"))&&Number(C,TEXT("interval_sec"),.033,60);
        // Profile/quality is one transaction; combining a preset with an override is ambiguous.
        Valid&=!(C.HasField(TEXT("interval_sec"))&&(C.HasField(TEXT("profile"))||C.HasField(TEXT("quality"))));
        Valid&=C.HasField(TEXT("interval_sec"))||C.HasField(TEXT("profile"))||C.HasField(TEXT("quality"));
    }
    else if(Op==TEXT("sensor.start")||Op==TEXT("sensor.stop")||Op==TEXT("capture")||Op==TEXT("pcd.start")||Op==TEXT("pcd.run"))
    {Allowed+=TEXT("|sensor_id");Valid&=String(C,TEXT("sensor_id"),true);if(Op==TEXT("pcd.run")){Allowed+=TEXT("|seconds");Valid&=C.HasField(TEXT("seconds"))&&Number(C,TEXT("seconds"),.1,86400);}}
    else if(Op==TEXT("scenario.start"))
    {Allowed+=TEXT("|json_path|uuid|send_pcd|sensor_id");Valid&=String(C,TEXT("json_path"))&&String(C,TEXT("uuid"))&&Boolean(C,TEXT("send_pcd"))&&String(C,TEXT("sensor_id"));Valid&=C.HasField(TEXT("json_path"))!=C.HasField(TEXT("uuid"));bool Send=false;C.TryGetBoolField(TEXT("send_pcd"),Send);Valid&=!Send||String(C,TEXT("sensor_id"),true);}
    else if(Op==TEXT("dataset.start"))
    {
        Allowed+=TEXT("|name|scenes|seed|settle_frames|camera_truth|lidar_fidelity|slab|lidar_id|camera_id|light|ranges|export_format|export_upright_envelope");
        Valid&=String(C,TEXT("export_format"))&&Boolean(C,TEXT("export_upright_envelope"));
        FString Export;C.TryGetStringField(TEXT("export_format"),Export);
        Valid&=!C.HasField(TEXT("export_format"))||Export==TEXT("none")||Export==TEXT("coco")||Export==TEXT("kitti")||Export==TEXT("coco,kitti");
        Valid&=String(C,TEXT("slab"),true)&&String(C,TEXT("lidar_id"),true)&&String(C,TEXT("camera_id"),true)&&String(C,TEXT("light"))&&String(C,TEXT("name"),true);
        FString Name;C.TryGetStringField(TEXT("name"),Name);for(TCHAR Ch:Name)Valid&=FChar::IsAlnum(Ch)||Ch==TEXT('_')||Ch==TEXT('-');Valid&=Name.Len()<=64;
        Valid&=C.HasField(TEXT("scenes"))&&Number(C,TEXT("scenes"),1,10000,true)&&Number(C,TEXT("seed"),MIN_int32,MAX_int32,true)&&Number(C,TEXT("settle_frames"),1,600,true)&&Boolean(C,TEXT("camera_truth"))&&Boolean(C,TEXT("lidar_fidelity"));
        if(C.HasField(TEXT("ranges")))
        {
            const TSharedPtr<FJsonObject>* Ranges=nullptr;Valid&=C.TryGetObjectField(TEXT("ranges"),Ranges);
            if(Ranges&&!Fields(**Ranges,TEXT("yaw|pitch|roll|x|y|z|size|surface|roughness|oxidation|light_intensity|light_pitch|light_yaw|sensor_position|sensor_angle")))return false;
            if(Ranges)for(const auto& R:(*Ranges)->Values)
            {
                const auto* A=R.Value->Type==EJson::Array?&R.Value->AsArray():nullptr;Valid&=A&&A->Num()==2;
                if(A&&A->Num()==2)
                {
                    double Min=0,Max=0;Valid&=(*A)[0]->Type==EJson::Number&&(*A)[1]->Type==EJson::Number&&(*A)[0]->TryGetNumber(Min)&&(*A)[1]->TryGetNumber(Max)&&FMath::IsFinite(Min)&&FMath::IsFinite(Max)&&Min<=Max;
                    double Low=-180,High=180;
                    if(R.Key==TEXT("x")||R.Key==TEXT("y")||R.Key==TEXT("z")){Low=-1000000;High=1000000;}
                    else if(R.Key==TEXT("size")){Low=.1;High=10;}
                    else if(R.Key==TEXT("surface")){Low=0;High=4;Valid&=FMath::FloorToDouble(Min)==Min&&FMath::FloorToDouble(Max)==Max;}
                    else if(R.Key==TEXT("roughness")||R.Key==TEXT("oxidation")){Low=0;High=1;}
                    else if(R.Key==TEXT("light_intensity")){Low=0;High=1000000;}
                    else if(R.Key==TEXT("light_pitch")){Low=-90;High=90;}
                    else if(R.Key==TEXT("sensor_position")){Low=-100;High=100;}
                    else if(R.Key==TEXT("sensor_angle")){Low=-10;High=10;}
                    Valid&=Min>=Low&&Max<=High;
                }
            }
        }
    }
    else if(Op==TEXT("wait")){Allowed+=TEXT("|seconds");Valid&=C.HasField(TEXT("seconds"))&&Number(C,TEXT("seconds"),.01,86400);}
    else if(Op!=TEXT("pcd.stop")&&Op!=TEXT("scenario.stop")&&Op!=TEXT("dataset.stop"))return false;
    if(!Valid||!Fields(C,Allowed))return false;Error.Reset();return true;
}
bool ParseJob(const FString& Text,const FString& Base,FJob& Job,FString& Error)
{
    Job=FJob();TSharedPtr<FJsonObject> Root;Error=TEXT("Invalid job document");
    if(!ParseObject(Text,Root)||!Fields(*Root,TEXT("schema|result_path|timeout_sec|steps")))return false;
    FString Schema;const TArray<TSharedPtr<FJsonValue>>* Steps=nullptr;
    if(!Root->TryGetStringField(TEXT("schema"),Schema)||Schema!=TEXT("ma0t10.job.v1")||!String(*Root,TEXT("result_path"),true)||!Number(*Root,TEXT("timeout_sec"),1,86400)||!Root->TryGetArrayField(TEXT("steps"),Steps)||Steps->Num()<1||Steps->Num()>1000)return false;
    Root->TryGetNumberField(TEXT("timeout_sec"),Job.TimeoutSeconds);Root->TryGetStringField(TEXT("result_path"),Job.ResultPath);
    Job.ResultPath=FPaths::ConvertRelativePathToFull(Base,Job.ResultPath);
    if(FPaths::GetExtension(Job.ResultPath)!=TEXT("json"))return false;
    for(const auto& Value:*Steps)
    {
        if(Value->Type!=EJson::Object||!ValidateCommand(*Value->AsObject(),Error))return false;
        auto C=MakeShared<FJsonObject>(*Value->AsObject());FString Path;
        if(C->TryGetStringField(TEXT("json_path"),Path))C->SetStringField(TEXT("json_path"),FPaths::ConvertRelativePathToFull(Base,Path));
        if(C->GetStringField(TEXT("op"))==TEXT("pcd.start")){Error=TEXT("Jobs require bounded pcd.run");return false;}
        Job.Steps.Add(C);
    }
    Error.Reset();return true;
}
}

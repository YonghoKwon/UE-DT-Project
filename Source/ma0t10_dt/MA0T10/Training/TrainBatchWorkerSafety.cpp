#include "ma0t10_dt/MA0T10/Training/TrainBatchWorkerSafety.h"
#include "ma0t10_dt/MA0T10/Training/TrainBatchProcessProtocol.h"
#include "Device/Contracts/LocalCapturePath.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"

namespace
{
bool ReadGuardText(const FString& Path,FString& Text)
{const int64 Size=IFileManager::Get().FileSize(*Path);return Size>0&&Size<=8192&&FFileHelper::LoadFileToString(Text,*Path);}
}
bool TrainBatchWorkerSafety::CountsNativeFailure(const FExitEvidence& E)
{return E.NativeFault&&!E.CoordinatorTerminated&&!E.CancelRequested;}
FString TrainBatchWorkerSafety::GuardStatePath(const FString& Root,int32 Slot)
{
    if(Slot<0||Slot>3)return FString();
    const FString Path=Root/FString::Printf(TEXT("slot-%02d/UserDir/Saved/NativeLidarSafety"),Slot)/SensorNativeSafety::StateFileName();
    return LocalCapturePath::SafeDescendant(Root,Path)?Path:FString();
}
bool TrainBatchWorkerSafety::GuardBanned(const FString& Root,int32 Slot)
{
    const FString Path=GuardStatePath(Root,Slot);FString Text;if(Path.IsEmpty()||!ReadGuardText(Path,Text))return false;
    TArray<FString> Lines;Text.ParseIntoArrayLines(Lines);
    for(const auto& Line:Lines)if(Line.StartsWith(TEXT("failures=")))
    {const FString Value=Line.Mid(9);int32 Count=0;return Value.IsNumeric()&&LexTryParseString(Count,*Value)&&Count>=2;}
    return false;
}
bool TrainBatchWorkerSafety::GuardFaulted(const FString& Root,int32 Slot)
{
    const FString Path=GuardStatePath(Root,Slot);FString Text;if(Path.IsEmpty()||!ReadGuardText(Path,Text))return false;
    TArray<FString> Lines;Text.ParseIntoArrayLines(Lines);return Lines.Contains(TEXT("faulted=1"));
}
bool TrainBatchWorkerSafety::CleanupControlledMarker(const FString& Root,int32 Slot,uint32 Pid,uint64 Stamp,const FExitEvidence& E)
{
    if((!E.CoordinatorTerminated&&!E.CancelRequested)||E.NativeFault||GuardFaulted(Root,Slot)||!Pid||!Stamp)return false;
    bool Alive=false;const uint64 Current=TrainBatchProcess::ProcessStamp(Pid,Alive);
    if(Alive&&(!Current||Current==Stamp))return false;
    const FString State=GuardStatePath(Root,Slot);if(State.IsEmpty())return false;
    const FString Marker=State.LeftChop(6);FString Text;if(!IFileManager::Get().FileExists(*Marker))return true;
    uint32 Owner=0;uint64 Created=0;
    if(!ReadGuardText(Marker,Text)||!FParse::Value(*Text,TEXT("pid="),Owner)||!FParse::Value(*Text,TEXT("start="),Created)||Owner!=Pid||Created!=Stamp)return false;
    return IFileManager::Get().Delete(*Marker,false,true); // State/counters are never reset here.
}

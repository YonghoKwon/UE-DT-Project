#pragma once
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace CoilTestReportPaths
{
inline constexpr TCHAR DefaultRelativeRoot[]=TEXT("Reports/COIL");
inline FString Resolve(const TCHAR* Arguments)
{
    FString Override;
    if(!FParse::Value(Arguments,TEXT("CoilReport="),Override))FParse::Value(Arguments,TEXT("CoilPhotoReport="),Override);
    return Override.IsEmpty()?FPaths::ProjectSavedDir()/DefaultRelativeRoot:Override;
}
inline FString Root(){return Resolve(FCommandLine::Get());}
inline FString Progress(){return Root()/TEXT("progress");}
}
#endif

#pragma once
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace CoilTestReportPaths
{
inline constexpr TCHAR DefaultRelativeRoot[]=TEXT("Reports/COIL");
inline FString Root()
{
    FString Override;FParse::Value(FCommandLine::Get(),TEXT("CoilReport="),Override);
    return Override.IsEmpty()?FPaths::ProjectSavedDir()/DefaultRelativeRoot:Override;
}
inline FString Progress(){return Root()/TEXT("progress");}
}
#endif

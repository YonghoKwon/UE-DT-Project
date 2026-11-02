#pragma once
#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/Paths.h"

/** Neutral local-file boundary. Callers own their storage root; device code knows no job types. */
namespace LocalCapturePath
{
    inline FString Full(FString Path)
    {
        Path=FPaths::ConvertRelativePathToFull(Path);
        FPaths::NormalizeFilename(Path);FPaths::CollapseRelativeDirectories(Path);
        FPaths::RemoveDuplicateSlashes(Path);
        while(Path.EndsWith(TEXT("/")))Path.LeftChopInline(1,false);
        return Path;
    }
    inline bool SafeDescendant(const FString& Root,const FString& Path)
    {
        if(FPaths::IsRelative(Root)||FPaths::IsRelative(Path))return false;
        for(const FString* Value:{&Root,&Path})for(TCHAR C:*Value)if(C<32||C==127)return false;
        FString Raw=Path;Raw.ReplaceInline(TEXT("\\"),TEXT("/"));TArray<FString> Components;Raw.ParseIntoArray(Components,TEXT("/"));
        if(Components.Contains(TEXT("..")))return false;
        for(int32 I=1;I<Components.Num();++I)if(Components[I].Contains(TEXT(":")))return false;
        const FString R=Full(Root),P=Full(Path);
        if(!P.StartsWith(R+TEXT("/"),ESearchCase::IgnoreCase))return false;
        // Check existing ancestors too: on Windows IsSymlink detects junction/reparse points.
        auto& Physical=IPlatformFile::GetPlatformPhysical();
        for(FString Current=P;!Current.IsEmpty();)
        {
            const auto Link=Physical.IsSymlink(*Current);
            if(Link!=ESymlinkResult::NonSymlink)return false;
            const FString Parent=FPaths::GetPath(Current);
            if(Parent==Current)break;Current=Parent;
        }
        return true;
    }
}

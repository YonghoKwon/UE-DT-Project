#include "RepairTestMapMonitorReferenceCommandlet.h"

#include "Blueprint/UserWidget.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"

URepairTestMapMonitorReferenceCommandlet::URepairTestMapMonitorReferenceCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 URepairTestMapMonitorReferenceCommandlet::Main(const FString& Params)
{
    // Never accept an arbitrary map, and never load/save SensorTestMap here.
    const FString PackageName(TEXT("/Game/MA0T10/Maps/TestMap"));
    UPackage* Package = LoadPackage(nullptr, *PackageName, LOAD_DisableCompileOnLoad);
    UWorld* World = Package ? FindObject<UWorld>(Package, TEXT("TestMap")) : nullptr;
    ULevelScriptBlueprint* Blueprint = World && World->PersistentLevel
        ? World->PersistentLevel->GetLevelScriptBlueprint(true) : nullptr;
    UClass* Replacement = LoadClass<UUserWidget>(nullptr,
        TEXT("/Game/MA0T10/UI/WBP_VirtualSensorMonitorPanel.WBP_VirtualSensorMonitorPanel_C"));
    if (!Blueprint || !Replacement)
    {
        UE_LOG(LogTemp, Error, TEXT("Expected TestMap level script and current monitor class are required."));
        return 1;
    }

    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    TArray<UEdGraphNode*> Missing;
    int32 AlreadyCurrent = 0;
    for (UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node || Node->GetClass()->GetFName() != FName(TEXT("K2Node_CreateWidget"))) continue;
            UEdGraphPin* ClassPin = Node->FindPin(TEXT("Class"));
            if (!ClassPin || ClassPin->LinkedTo.Num() != 0) continue;
            UE_LOG(LogTemp, Display, TEXT("CreateWidget node=%s class=%s"),
                *Node->GetName(), *GetPathNameSafe(ClassPin->DefaultObject));
            if (!ClassPin->DefaultObject) Missing.Add(Node);
            else if (ClassPin->DefaultObject == Replacement) ++AlreadyCurrent;
        }
    }
    if (Missing.IsEmpty() && AlreadyCurrent == 1)
    {
        UE_LOG(LogTemp, Display, TEXT("Reference already repaired; no package was written."));
        return 0;
    }
    if (Missing.Num() != 1 || AlreadyCurrent != 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Expected exactly one missing literal widget class; refusing ambiguous repair."));
        return 1;
    }
    if (!FParse::Param(*Params, TEXT("Apply")))
    {
        UE_LOG(LogTemp, Display, TEXT("DRY RUN: one missing class would use %s. Pass -Apply to save TestMap only."), *Replacement->GetPathName());
        return 0;
    }

    const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetMapPackageExtension());
    const FString Backup = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("LauncherPackagingBackup"),
        FDateTime::UtcNow().ToString(TEXT("%Y%m%d%H%M%S")), TEXT("TestMap.umap"));
    if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true)
        || IFileManager::Get().Copy(*Backup, *Filename, false) != COPY_OK)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not preserve the original TestMap; no repair saved."));
        return 1;
    }
    UEdGraphNode* Node = Missing[0];
    Node->Modify();
    Node->FindPin(TEXT("Class"))->DefaultObject = Replacement;
    Node->ReconstructNode();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
    if (Blueprint->Status == BS_Error)
    {
        UE_LOG(LogTemp, Error, TEXT("Repaired blueprint still has errors; package not saved."));
        return 1;
    }
    Package->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Package, World, *Filename, Args)) return 1;
    UE_LOG(LogTemp, Display, TEXT("Saved TestMap monitor reference. Original backup: %s"), *Backup);
    return 0;
}

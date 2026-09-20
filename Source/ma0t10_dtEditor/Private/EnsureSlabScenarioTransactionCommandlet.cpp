#include "EnsureSlabScenarioTransactionCommandlet.h"
#include "Engine/DataTable.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "WebSocket/TransactionCodeStruct.h"
#include "ma0t10_dt/MA0T10/WebSocket/TC/FactoryAgentScenarioTC.h"
UEnsureSlabScenarioTransactionCommandlet::UEnsureSlabScenarioTransactionCommandlet()
{IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;}
int32 UEnsureSlabScenarioTransactionCommandlet::Main(const FString& Params)
{
    auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/MA0T10/Common/DataTables/DT_TransactionCode.DT_TransactionCode"));
    if(!Table||Table->GetRowStruct()!=FTransactionCodeStruct::StaticStruct())return 1;
    const FName Name(TEXT("IFactory-agent"));
    for(const auto& Pair:Table->GetRowMap())
    {
        const auto* Row=reinterpret_cast<const FTransactionCodeStruct*>(Pair.Value);
        if(Row->TransactionCodeMessageClass&&Row->TransactionCodeMessageClass.GetDefaultObject()->TransactionCode==TEXT("IFactory-agent"))
        {
            if(Row->TransactionCodeMessageClass!=UFactoryAgentScenarioTC::StaticClass())
            {UE_LOG(LogTemp,Error,TEXT("Existing IFactory-agent handler belongs to another implementation; unchanged."));return 1;}
            UE_LOG(LogTemp,Display,TEXT("SLAB_TRANSACTION_READY: existing correct handler preserved"));return 0;
        }
    }
    if(Table->GetRowNames().Contains(Name))
    {UE_LOG(LogTemp,Error,TEXT("IFactory-agent row name is already used; unchanged."));return 1;}
    FTransactionCodeStruct Row;Row.TransactionCodeName=TEXT("IFactory-agent");Row.TransactionCodeInfo=TEXT("Slab bulk scenario");
    Row.TransactionCodeMessageClass=UFactoryAgentScenarioTC::StaticClass();Row.Comment=TEXT("Project-owned Slab component route; DTCore unchanged");
    if(Params.Contains(TEXT("-NoSave"))) {UE_LOG(LogTemp,Display,TEXT("SLAB_TRANSACTION_DRY_RUN: safe to append"));return 0;}
    Table->AddRow(Name,Row);Table->MarkPackageDirty();
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    auto* Package=Table->GetOutermost();
    const FString File=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());
    if(!UPackage::SavePackage(Package,Table,*File,Args))return 1;
    UE_LOG(LogTemp,Display,TEXT("SLAB_TRANSACTION_READY: appended one handler; other rows preserved"));return 0;
}

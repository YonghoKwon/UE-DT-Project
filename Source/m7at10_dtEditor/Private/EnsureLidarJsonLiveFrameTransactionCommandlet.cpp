#include "EnsureLidarJsonLiveFrameTransactionCommandlet.h"

#include "Core/DTCoreSettings.h"
#include "Engine/DataTable.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "WebSocket/TransactionCodeStruct.h"
#include "m7at10_dt/M7AT10/WebSocket/TC/LidarJsonLiveFrameTC.h"

UEnsureLidarJsonLiveFrameTransactionCommandlet::UEnsureLidarJsonLiveFrameTransactionCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UEnsureLidarJsonLiveFrameTransactionCommandlet::Main(const FString& Params)
{
    const bool bNoSave = Params.Contains(TEXT("-NoSave"));
    const UDTCoreSettings* Settings = GetDefault<UDTCoreSettings>();
    if (!Settings)
    {
        UE_LOG(LogTemp, Error, TEXT("DTCore settings are not available."));
        return 1;
    }

    const FSoftObjectPath ConfiguredPath = Settings->WebSocketDataTable.ToSoftObjectPath();
    if (!ConfiguredPath.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("DTCore WebSocketDataTable is not configured."));
        return 1;
    }

    UDataTable* DataTable = Settings->WebSocketDataTable.LoadSynchronous();
    if (!DataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load WebSocketDataTable: %s"), *ConfiguredPath.ToString());
        return 1;
    }

    const UScriptStruct* ExpectedRowStruct = FTransactionCodeStruct::StaticStruct();
    if (DataTable->GetRowStruct() != ExpectedRowStruct)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("WebSocketDataTable row struct is %s, expected %s."),
            *GetNameSafe(DataTable->GetRowStruct()),
            *GetNameSafe(ExpectedRowStruct));
        return 1;
    }

    const FName RowName(TEXT("LIDAR_JSON_LIVE_FRAME"));
    FTransactionCodeStruct Row;
    if (const FTransactionCodeStruct* ExistingRow = DataTable->FindRow<FTransactionCodeStruct>(
        RowName,
        TEXT("EnsureLidarJsonLiveFrameTransaction"),
        false))
    {
        Row = *ExistingRow;
    }

    Row.TransactionCodeName = TEXT("LIDAR_JSON_LIVE_FRAME");
    Row.TransactionCodeInfo = TEXT("JSON live LiDAR frame bridge");
    Row.TransactionCodeMessageClass = ULidarJsonLiveFrameTC::StaticClass();
    Row.Comment = TEXT("Routes WebSocket JSON LiDAR frames into ULidarJsonLiveSourceComp.");

    if (!Row.TransactionCodeMessageClass)
    {
        UE_LOG(LogTemp, Error, TEXT("ULidarJsonLiveFrameTC class failed to resolve."));
        return 1;
    }

    DataTable->AddRow(RowName, Row);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("Ensured WebSocket transaction row %s -> %s in %s."),
        *RowName.ToString(),
        *GetNameSafe(Row.TransactionCodeMessageClass.Get()),
        *ConfiguredPath.ToString());

    if (bNoSave)
    {
        UE_LOG(LogTemp, Display, TEXT("-NoSave was specified; data table was not saved."));
        return 0;
    }

    DataTable->MarkPackageDirty();
    UPackage* Package = DataTable->GetOutermost();
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("WebSocketDataTable package is not available."));
        return 1;
    }

    const FString PackageFileName = FPackageName::LongPackageNameToFilename(
        Package->GetName(),
        FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;

    if (!UPackage::SavePackage(Package, DataTable, *PackageFileName, SaveArgs))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to save WebSocketDataTable package: %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("Saved WebSocketDataTable package: %s"), *PackageFileName);
    return 0;
}

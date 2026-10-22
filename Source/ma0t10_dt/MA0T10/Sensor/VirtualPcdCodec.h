#pragma once

#include "CoreMinimal.h"
#include "VirtualLidarSensorTypes.h"
#include "VirtualPcdCodec.generated.h"

UENUM(BlueprintType)
enum class EVirtualPcdFieldProfile : uint8 { Full, Compact, Minimal, Custom };

/** Transmission only. XYZ are mandatory; custom bits follow the canonical FIELDS order. */
USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualPcdSendOptions
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) EVirtualPcdFieldProfile Profile = EVirtualPcdFieldProfile::Full;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 CustomFieldMask = 2047;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bCompressed = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="1", ClampMax="4")) int32 SendDivisor = 1;
	int32 FieldMask() const;
	int32 Divisor() const { return SendDivisor == 2 || SendDivisor == 4 ? SendDivisor : 1; }
	bool operator==(const FVirtualPcdSendOptions& R) const { return Profile==R.Profile && CustomFieldMask==R.CustomFieldMask && bCompressed==R.bCompressed && SendDivisor==R.SendDivisor; }
};

struct MA0T10_DT_API FVirtualPcdDocument
{
	FString Header;
	FString Encoding;
	int32 PayloadOffset = 0;
	int32 PointCount = 0;
	int32 PointBytes = 0;
	TArray<FString> Names;
	TArray<int32> Sizes, Offsets;
	/** Header-ordered little-endian AoS, including unknown fields. */
	TArray<uint8> Records;
	bool ReadPoints(TArray<FVirtualLidarPoint>& OutPoints) const;
};

/** Stateless bounded PCD binary/LZF codec. No UObject or acquisition access on worker threads. */
class MA0T10_DT_API FVirtualPcdCodec
{
public:
	static const TCHAR* FieldName(int32 Index);
	static int32 BytesPerPoint(const FVirtualPcdSendOptions& Options);
	static double EstimateBytesPerSecond(const FVirtualPcdSendOptions& Options, int32 Points, double ScanHz);
	static bool Encode(const TArray<const FVirtualLidarPoint*>& Points, const FString& MetadataJson, const FVirtualPcdSendOptions& Options, TArray<uint8>& Out);
	static bool Decode(const TArray<uint8>& Bytes, FVirtualPcdDocument& Out, FString& Error);
	static void CompressLzf(const TArray<uint8>& Input, TArray<uint8>& Out);
	static bool DecompressLzf(const uint8* Input, int32 InputSize, int32 OutputSize, TArray<uint8>& Out);
};

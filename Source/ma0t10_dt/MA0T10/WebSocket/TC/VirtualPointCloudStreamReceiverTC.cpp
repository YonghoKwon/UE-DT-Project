#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualPointCloudStreamReceiverTC.h"

#include "Json.h"
#include "Misc/Base64.h"
#include "Misc/SecureHash.h"
#include "ma0t10_dt/ma0t10_dt.h"

namespace
{
bool StartsWithBytes(const TArray<uint8>& Bytes, const ANSICHAR* Prefix)
{
	const int32 PrefixLength = FCStringAnsi::Strlen(Prefix);
	return Bytes.Num() >= PrefixLength && FMemory::Memcmp(Bytes.GetData(), Prefix, PrefixLength) == 0;
}

bool HasPointCloudSignature(const FString& Format, const TArray<uint8>& Bytes)
{
	if (Format == TEXT("CSV")) return StartsWithBytes(Bytes, "x,y,z,distance,hit");
	if (Format == TEXT("JSONL"))
	{
		for (const uint8 Byte : Bytes)
		{
			if (Byte == ' ' || Byte == '\t' || Byte == '\r' || Byte == '\n') continue;
			return Byte == '{';
		}
		return false;
	}
	if (Format == TEXT("PCD")) return StartsWithBytes(Bytes, "# .PCD");
	if (Format == TEXT("LAS") || Format == TEXT("LAZ")) return StartsWithBytes(Bytes, "LASF");
	if (Format == TEXT("VLB2")) return StartsWithBytes(Bytes, "VLDR2");
	return false;
}

const FString* FindHeader(const TMap<FName, FString>& Headers, const TCHAR* Name)
{
	return Headers.Find(FName(Name));
}

bool ReadRequiredHeader(const TMap<FName, FString>& Headers, const TCHAR* Name, FString& OutValue)
{
	if (const FString* Value = FindHeader(Headers, Name))
	{
		OutValue = *Value;
		return !OutValue.IsEmpty();
	}
	return false;
}

bool ParseInt64Header(const TMap<FName, FString>& Headers, const TCHAR* Name, int64& OutValue)
{
	FString Text;
	return ReadRequiredHeader(Headers, Name, Text) && LexTryParseString(OutValue, *Text);
}

bool ParseInt32Header(const TMap<FName, FString>& Headers, const TCHAR* Name, int32& OutValue)
{
	FString Text;
	return ReadRequiredHeader(Headers, Name, Text) && LexTryParseString(OutValue, *Text);
}

int32 FindBinaryPayloadOffset(const TArray<uint8>& Bytes)
{
	static const ANSICHAR Marker[] = "DATA binary\n";
	constexpr int32 MarkerLength = static_cast<int32>(sizeof(Marker) - 1);
	for (int32 Index = 0; Index + MarkerLength <= Bytes.Num(); ++Index)
	{
		if (FMemory::Memcmp(Bytes.GetData() + Index, Marker, MarkerLength) == 0) return Index + MarkerLength;
	}
	return INDEX_NONE;
}
}

UVirtualPointCloudStreamReceiverTC::UVirtualPointCloudStreamReceiverTC()
{
	TransactionCode = TEXT("VIRTUAL_POINTCLOUD_STREAM");
}

TSharedPtr<FTransactionCodeDataBase> UVirtualPointCloudStreamReceiverTC::ParseToStruct(const FString& JsonString) const
{
	TSharedPtr<FVirtualPointCloudStreamReceiverData> Data = MakeShared<FVirtualPointCloudStreamReceiverData>();
	Data->Kind = EVirtualSensorTopicReceiveKind::PointCloud;
	Data->MessageBytes = FTCHARToUTF8(*JsonString).Length();
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		Data->Message = TEXT("Point Cloud JSON 파싱 실패");
		return Data;
	}
	double Frame = -1.0, PointCount = -1.0, ByteCount = -1.0;
	FString EncodedData;
	if (!Root->TryGetStringField(TEXT("schema"), Data->SchemaVersion) ||
		!Root->TryGetStringField(TEXT("sensorId"), Data->SensorId) || Data->SensorId.IsEmpty() ||
		!Root->TryGetNumberField(TEXT("frameId"), Frame) ||
		!Root->TryGetStringField(TEXT("format"), Data->Format) ||
		!Root->TryGetStringField(TEXT("encoding"), Data->Encoding) ||
		!Root->TryGetNumberField(TEXT("pointCount"), PointCount) ||
		!Root->TryGetNumberField(TEXT("byteCount"), ByteCount) ||
		!Root->TryGetStringField(TEXT("data"), EncodedData) || EncodedData.IsEmpty())
	{
		Data->Message = TEXT("Point Cloud 필수 필드 또는 data가 없습니다.");
		return Data;
	}
	Data->FrameId = static_cast<int64>(Frame);
	Data->PointCount = static_cast<int32>(PointCount);
	Data->DeclaredByteCount = static_cast<int32>(ByteCount);
	Data->Format = Data->Format.ToUpper();
	const bool bKnownFormat = Data->Format == TEXT("CSV") || Data->Format == TEXT("JSONL") || Data->Format == TEXT("PCD") ||
		Data->Format == TEXT("LAS") || Data->Format == TEXT("LAZ") || Data->Format == TEXT("VLB2");
	if (Data->SchemaVersion != TEXT("virtual-pointcloud.v1") || Data->Encoding != TEXT("base64") ||
		!bKnownFormat || Data->PointCount <= 0 || Data->DeclaredByteCount <= 0)
	{
		Data->Message = FString::Printf(TEXT("Point Cloud 계약 불일치: schema=%s format=%s encoding=%s points=%d bytes=%d"),
			*Data->SchemaVersion, *Data->Format, *Data->Encoding, Data->PointCount, Data->DeclaredByteCount);
		return Data;
	}
	Data->bDeepValidated = Data->FrameId >= 0 && (Data->FrameId % 10) == 0;
	if (Data->bDeepValidated)
	{
		TArray<uint8> Decoded;
		if (!FBase64::Decode(EncodedData, Decoded))
		{
			Data->Message = TEXT("Point Cloud Base64 디코딩 실패");
			return Data;
		}
		Data->DecodedByteCount = Decoded.Num();
		const bool bSignatureValid = HasPointCloudSignature(Data->Format, Decoded);
		if (Data->DecodedByteCount != Data->DeclaredByteCount || !bSignatureValid)
		{
			Data->Message = FString::Printf(TEXT("Point Cloud 상세 검증 실패: declared=%d decoded=%d signature=%s"),
				Data->DeclaredByteCount, Data->DecodedByteCount, bSignatureValid ? TEXT("true") : TEXT("false"));
			return Data;
		}
	}
	Data->bValid = true;
	Data->Message = FString::Printf(TEXT("Point Cloud 수신 정상 · %s · %d points · %d bytes%s"), *Data->Format,
		Data->PointCount, Data->DeclaredByteCount, Data->bDeepValidated ? TEXT(" · 상세 검증") : TEXT(""));
	return Data;
}

TSharedPtr<FTransactionCodeDataBase> UVirtualPointCloudStreamReceiverTC::ParseBinaryPcdToStruct(
	const TArray<uint8>& Body,
	const TMap<FName, FString>& Headers) const
{
	TSharedPtr<FVirtualPointCloudStreamReceiverData> Data = MakeShared<FVirtualPointCloudStreamReceiverData>();
	Data->Kind = EVirtualSensorTopicReceiveKind::PointCloud;
	Data->MessageBytes = Body.Num();
	Data->Format = TEXT("PCD");
	Data->Encoding = TEXT("binary");
	Data->DecodedByteCount = Body.Num();

	FString ContentType;
	FString TimestampText;
	if (!ReadRequiredHeader(Headers, TEXT("schema"), Data->SchemaVersion) ||
		!ReadRequiredHeader(Headers, TEXT("x-sensor-id"), Data->SensorId) ||
		!ParseInt64Header(Headers, TEXT("x-frame-id"), Data->FrameId) ||
		!ParseInt32Header(Headers, TEXT("x-point-count"), Data->PointCount) ||
		!ParseInt32Header(Headers, TEXT("x-source-point-count"), Data->SourcePointCount) ||
		!ParseInt32Header(Headers, TEXT("x-filter-revision"), Data->FilterRevision) ||
		!ReadRequiredHeader(Headers, TEXT("x-checksum-sha1"), Data->ChecksumSha1) ||
		!ReadRequiredHeader(Headers, TEXT("x-acquisition-profile"), Data->ProfileKey) ||
		!ReadRequiredHeader(Headers, TEXT("x-utc"), TimestampText) ||
		!ReadRequiredHeader(Headers, TEXT("content-type"), ContentType))
	{
		Data->Message = TEXT("Binary PCD required STOMP headers are missing.");
		return Data;
	}
	if (!FDateTime::ParseIso8601(*TimestampText, Data->SourceTimestampUtc))
	{
		Data->Message = FString::Printf(TEXT("Binary PCD x-utc header is invalid: %s"), *TimestampText);
		return Data;
	}
	Data->DeclaredByteCount = Body.Num();
	if (Data->SchemaVersion != TEXT("virtual-pointcloud.pcd.v1") ||
		!ContentType.Equals(TEXT("application/vnd.pcd"), ESearchCase::IgnoreCase) ||
		Data->FrameId < 0 || Data->PointCount < 0 || Data->SourcePointCount < Data->PointCount)
	{
		Data->Message = FString::Printf(TEXT("Binary PCD contract mismatch: schema=%s contentType=%s frame=%lld points=%d/%d"),
			*Data->SchemaVersion, *ContentType, Data->FrameId, Data->PointCount, Data->SourcePointCount);
		return Data;
	}

	const int32 PayloadOffset = FindBinaryPayloadOffset(Body);
	if (PayloadOffset == INDEX_NONE)
	{
		Data->Message = TEXT("PCD DATA binary marker is missing.");
		return Data;
	}
	FUTF8ToTCHAR HeaderUtf8(reinterpret_cast<const ANSICHAR*>(Body.GetData()), PayloadOffset);
	const FString Header(HeaderUtf8.Length(), HeaderUtf8.Get());
	const FString RequiredFields = TEXT("FIELDS x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence");
	const bool bHeaderContractValid = Header.Contains(TEXT("# .PCD v0.7")) && Header.Contains(TEXT("VERSION 0.7")) &&
		Header.Contains(RequiredFields) && Header.Contains(TEXT("SIZE 4 4 4 2 2 2 1 1 8 1 4")) &&
		Header.Contains(TEXT("TYPE F F F U U U U U I U F")) && Header.Contains(TEXT("COUNT 1 1 1 1 1 1 1 1 1 1 1")) &&
		Header.Contains(FString::Printf(TEXT("POINTS %d"), Data->PointCount));
	constexpr int32 PointRecordBytes = 33;
	const int64 ExpectedBytes = static_cast<int64>(PayloadOffset) + static_cast<int64>(Data->PointCount) * PointRecordBytes;
	if (!bHeaderContractValid || ExpectedBytes != Body.Num())
	{
		Data->Message = FString::Printf(TEXT("Binary PCD header/body mismatch: points=%d expectedBytes=%lld actualBytes=%d"),
			Data->PointCount, ExpectedBytes, Body.Num());
		return Data;
	}

	uint8 Hash[FSHA1::DigestSize];
	FSHA1::HashBuffer(Body.GetData(), Body.Num(), Hash);
	const FString ActualChecksum = BytesToHex(Hash, FSHA1::DigestSize).ToLower();
	if (!ActualChecksum.Equals(Data->ChecksumSha1, ESearchCase::IgnoreCase))
	{
		Data->Message = FString::Printf(TEXT("Binary PCD checksum mismatch: expected=%s actual=%s"), *Data->ChecksumSha1, *ActualChecksum);
		return Data;
	}

	Data->bDeepValidated = true;
	Data->bValid = true;
	Data->Message = FString::Printf(TEXT("Binary PCD valid · %d/%d points · %d bytes · checksum ok"),
		Data->PointCount, Data->SourcePointCount, Body.Num());
	return Data;
}

void UVirtualPointCloudStreamReceiverTC::ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data)
{
	const TSharedPtr<FVirtualPointCloudStreamReceiverData> PointCloud = StaticCastSharedPtr<FVirtualPointCloudStreamReceiverData>(Data);
	if (!PointCloud.IsValid()) return;
	const double Now = FPlatformTime::Seconds();
	if (!PointCloud->bValid || LastOutputLogSeconds < 0.0 || Now - LastOutputLogSeconds >= 1.0)
	{
		LastOutputLogSeconds = Now;
		UE_LOG(LogMA0T10, Log, TEXT("[SensorTopicReceiver][PointCloud] sensor=%s frame=%lld bytes=%d valid=%s %s"),
			*PointCloud->SensorId, PointCloud->FrameId, PointCloud->MessageBytes,
			PointCloud->bValid ? TEXT("true") : TEXT("false"), *PointCloud->Message);
	}
}

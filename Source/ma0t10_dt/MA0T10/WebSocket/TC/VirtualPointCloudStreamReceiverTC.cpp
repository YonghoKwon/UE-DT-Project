#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualPointCloudStreamReceiverTC.h"

#include "Json.h"
#include "Misc/Base64.h"
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
	return false;
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
		Data->Format == TEXT("LAS") || Data->Format == TEXT("LAZ");
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

#include "DeviceSensorStreamCodec.h"
#include "ma0t10_dt/MA0T10/Contracts/DeviceIdentityPolicy.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorPcdReceiveContract.h"

#include "Json.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualPcdCodec.h"
#include "Misc/Base64.h"
#include "Misc/SecureHash.h"

namespace
{
bool DevicePointCloudStartsWithBytes(const TArray<uint8>& Bytes, const ANSICHAR* Prefix)
{
	const int32 PrefixLength = FCStringAnsi::Strlen(Prefix);
	return Bytes.Num() >= PrefixLength && FMemory::Memcmp(Bytes.GetData(), Prefix, PrefixLength) == 0;
}

bool DevicePointCloudHasPointCloudSignature(const FString& Format, const TArray<uint8>& Bytes)
{
	if (Format == TEXT("CSV")) return DevicePointCloudStartsWithBytes(Bytes, "x,y,z,distance,hit");
	if (Format == TEXT("JSONL"))
	{
		for (const uint8 Byte : Bytes)
		{
			if (Byte == ' ' || Byte == '\t' || Byte == '\r' || Byte == '\n') continue;
			return Byte == '{';
		}
		return false;
	}
	if (Format == TEXT("PCD")) return DevicePointCloudStartsWithBytes(Bytes, "# .PCD");
	if (Format == TEXT("LAS") || Format == TEXT("LAZ")) return DevicePointCloudStartsWithBytes(Bytes, "LASF");
	if (Format == TEXT("VLB2")) return DevicePointCloudStartsWithBytes(Bytes, "VLDR2");
	return false;
}

const FString* FindHeader(const TMap<FName, FString>& Headers, const TCHAR* Name)
{
	return Headers.Find(FName(Name));
}

bool DevicePointCloudReadRequiredHeader(const TMap<FName, FString>& Headers, const TCHAR* Name, FString& OutValue)
{
	if (const FString* Value = FindHeader(Headers, Name))
	{
		OutValue = *Value;
		return !OutValue.IsEmpty();
	}
	return false;
}

bool DevicePointCloudParseInt64Header(const TMap<FName, FString>& Headers, const TCHAR* Name, int64& OutValue)
{
	FString Text;
	return DevicePointCloudReadRequiredHeader(Headers, Name, Text) && LexTryParseString(OutValue, *Text);
}

bool DevicePointCloudParseInt32Header(const TMap<FName, FString>& Headers, const TCHAR* Name, int32& OutValue)
{
	FString Text;
	return DevicePointCloudReadRequiredHeader(Headers, Name, Text) && LexTryParseString(OutValue, *Text);
}


}


TSharedPtr<FTransactionCodeDataBase> DeviceSensorStreamCodec::ParsePointCloudJson(const FString& JsonString)
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
		const bool bSignatureValid = DevicePointCloudHasPointCloudSignature(Data->Format, Decoded);
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
TSharedPtr<FTransactionCodeDataBase> DeviceSensorStreamCodec::ParseBinaryPcd(
	const TArray<uint8>& Body,
	const TMap<FName, FString>& IncomingHeaders)
{
	TSharedPtr<FVirtualPointCloudStreamReceiverData> Data = MakeShared<FVirtualPointCloudStreamReceiverData>();
	Data->Kind = EVirtualSensorTopicReceiveKind::PointCloud;
	Data->MessageBytes = Body.Num();
	Data->Format = TEXT("PCD");
	Data->Encoding = TEXT("binary");
	Data->DecodedByteCount = Body.Num();
	FVirtualPcdDocument Pcd; FString PcdError;
	if(!FVirtualPcdCodec::Decode(Body,Pcd,PcdError)){Data->Message=PcdError;return Data;}
	Data->Encoding=Pcd.Encoding;
	const FString& Header=Pcd.Header;
	TMap<FName,FString> CombinedHeaders = IncomingHeaders;
	TSharedPtr<FJsonObject> Embedded;
	TArray<FString> Lines; Header.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		if (!Line.StartsWith(TEXT("# MA0T10_META "))) continue;
		if (Embedded.IsValid() || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line.Mid(14)), Embedded) || !Embedded.IsValid())
		{ Data->Message=TEXT("PCD 내부 메타데이터 JSON 오류 또는 중복"); return Data; }
		FString Schema;
		if (!Embedded->TryGetStringField(TEXT("schema"),Schema) || Schema!=TEXT("virtual-pointcloud.context.v1"))
		{ Data->Message=TEXT("PCD 내부 메타데이터 schema 오류"); return Data; }
		const TCHAR* Fields[][2]={{TEXT("scenario_uuid"),TEXT("x-scenario-uuid")},{TEXT("run_uuid"),TEXT("x-run-uuid")},{TEXT("mtl_no"),TEXT("x-mtl-no")},
			{TEXT("frame_no"),TEXT("x-slab-frame-no")},{TEXT("elapsed_sec"),TEXT("x-slab-elapsed-sec")},{TEXT("session_segment"),TEXT("x-session-segment")}};
		for (const auto& Field : Fields)
		{
			FString Value;
			if (!Embedded->TryGetStringField(Field[0], Value)) continue;
			if (const FString* Existing=CombinedHeaders.Find(Field[1]))
				if (*Existing!=Value) { Data->Message=TEXT("PCD 내부 Slab 정보와 STOMP 헤더가 다릅니다."); return Data; }
			CombinedHeaders.Add(Field[1],Value);
		}
	}
	TMap<FName,FString> Headers;
	if(!FVirtualSensorPcdReceiveContract::Normalize(CombinedHeaders,Headers,*Data)) return Data;

	FString ContentType;
	FString TimestampText;
	if (!DevicePointCloudReadRequiredHeader(Headers, TEXT("schema"), Data->SchemaVersion) ||
		!DevicePointCloudReadRequiredHeader(Headers, TEXT("x-sensor-id"), Data->SensorId) ||
		!DevicePointCloudParseInt64Header(Headers, TEXT("x-frame-id"), Data->FrameId) ||
		!DevicePointCloudParseInt32Header(Headers, TEXT("x-point-count"), Data->PointCount) ||
		!DevicePointCloudParseInt32Header(Headers, TEXT("x-source-point-count"), Data->SourcePointCount) ||
		!DevicePointCloudParseInt32Header(Headers, TEXT("x-filter-revision"), Data->FilterRevision) ||
		!DevicePointCloudReadRequiredHeader(Headers, TEXT("x-checksum-sha1"), Data->ChecksumSha1) ||
		!DevicePointCloudReadRequiredHeader(Headers, TEXT("x-acquisition-profile"), Data->ProfileKey) ||
		!DevicePointCloudReadRequiredHeader(Headers, TEXT("x-utc"), TimestampText) ||
		!DevicePointCloudReadRequiredHeader(Headers, TEXT("content-type"), ContentType))
	{
		Data->Message = TEXT("Binary PCD required STOMP headers are missing.");
		return Data;
	}
	// Normalize has already converted both wire timestamp formats to ISO8601.
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

	if (Embedded.IsValid())
	{
		FString Sensor, Frame, Utc;
		if (!Embedded->TryGetStringField(TEXT("sensor_id"),Sensor) || DeviceIdentityPolicy::Canonical(Sensor)!=Data->SensorId ||
			!Embedded->TryGetStringField(TEXT("sensor_frame_id"),Frame) || Frame!=LexToString(Data->FrameId) ||
			!Embedded->TryGetStringField(TEXT("timestamp_utc"),Utc) || Utc!=TimestampText)
		{ Data->Message=TEXT("PCD 내부 센서 식별 정보와 STOMP 헤더가 다릅니다."); return Data; }
	}
	if(Pcd.PointCount!=Data->PointCount){Data->Message=TEXT("PCD POINTS and transport header disagree");return Data;}

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

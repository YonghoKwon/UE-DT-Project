#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualCameraStreamReceiverTC.h"

#include "Json.h"
#include "Misc/Base64.h"
#include "ma0t10_dt/ma0t10_dt.h"

UVirtualCameraStreamReceiverTC::UVirtualCameraStreamReceiverTC()
{
	TransactionCode = TEXT("VIRTUAL_CAMERA_STREAM");
}

TSharedPtr<FTransactionCodeDataBase> UVirtualCameraStreamReceiverTC::ParseToStruct(const FString& JsonString) const
{
	TSharedPtr<FVirtualCameraStreamReceiverData> Data = MakeShared<FVirtualCameraStreamReceiverData>();
	Data->Kind = EVirtualSensorTopicReceiveKind::Camera;
	Data->MessageBytes = FTCHARToUTF8(*JsonString).Length();
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		Data->Message = TEXT("Camera JSON 파싱 실패");
		return Data;
	}
	double Frame = -1.0, Width = 0.0, Height = 0.0, ByteSize = -1.0;
	FString Image;
	if (!Root->TryGetStringField(TEXT("schemaVersion"), Data->SchemaVersion) ||
		!Root->TryGetStringField(TEXT("sensorId"), Data->SensorId) || Data->SensorId.IsEmpty() ||
		!Root->TryGetNumberField(TEXT("frameId"), Frame) ||
		!Root->TryGetNumberField(TEXT("width"), Width) || !Root->TryGetNumberField(TEXT("height"), Height) ||
		!Root->TryGetNumberField(TEXT("byteSize"), ByteSize) ||
		!Root->TryGetStringField(TEXT("encoding"), Data->Encoding) ||
		!Root->TryGetStringField(TEXT("image"), Image) || Image.IsEmpty())
	{
		Data->Message = TEXT("Camera 필수 필드 또는 image가 없습니다.");
		return Data;
	}
	Data->FrameId = static_cast<int64>(Frame);
	Data->Width = static_cast<int32>(Width);
	Data->Height = static_cast<int32>(Height);
	Data->DeclaredByteSize = static_cast<int32>(ByteSize);
	if (Data->SchemaVersion != TEXT("virtual-camera.v1") || Data->Encoding != TEXT("jpeg/base64") ||
		Data->Width <= 0 || Data->Height <= 0 || Data->DeclaredByteSize <= 0)
	{
		Data->Message = FString::Printf(TEXT("Camera 계약 불일치: schema=%s encoding=%s size=%dx%d bytes=%d"),
			*Data->SchemaVersion, *Data->Encoding, Data->Width, Data->Height, Data->DeclaredByteSize);
		return Data;
	}
	Data->bDeepValidated = Data->FrameId >= 0 && (Data->FrameId % 10) == 0;
	if (Data->bDeepValidated)
	{
		TArray<uint8> Decoded;
		if (!FBase64::Decode(Image, Decoded))
		{
			Data->Message = TEXT("Camera Base64 디코딩 실패");
			return Data;
		}
		Data->DecodedByteSize = Decoded.Num();
		const bool bJpegSignature = Decoded.Num() >= 4 && Decoded[0] == 0xff && Decoded[1] == 0xd8 &&
			Decoded[Decoded.Num() - 2] == 0xff && Decoded[Decoded.Num() - 1] == 0xd9;
		if (!bJpegSignature || Data->DecodedByteSize != Data->DeclaredByteSize)
		{
			Data->Message = FString::Printf(TEXT("Camera JPEG 검증 실패: declared=%d decoded=%d signature=%s"),
				Data->DeclaredByteSize, Data->DecodedByteSize, bJpegSignature ? TEXT("true") : TEXT("false"));
			return Data;
		}
	}
	Data->bValid = true;
	Data->Message = FString::Printf(TEXT("Camera 수신 정상 · %dx%d · JPEG %d bytes%s"), Data->Width, Data->Height,
		Data->DeclaredByteSize, Data->bDeepValidated ? TEXT(" · 상세 검증") : TEXT(""));
	return Data;
}

void UVirtualCameraStreamReceiverTC::ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data)
{
	const TSharedPtr<FVirtualCameraStreamReceiverData> Camera = StaticCastSharedPtr<FVirtualCameraStreamReceiverData>(Data);
	if (!Camera.IsValid()) return;
	const double Now = FPlatformTime::Seconds();
	if (!Camera->bValid || LastOutputLogSeconds < 0.0 || Now - LastOutputLogSeconds >= 1.0)
	{
		LastOutputLogSeconds = Now;
		UE_LOG(LogMA0T10, Log, TEXT("[SensorTopicReceiver][Camera] sensor=%s frame=%lld bytes=%d valid=%s %s"),
			*Camera->SensorId, Camera->FrameId, Camera->MessageBytes, Camera->bValid ? TEXT("true") : TEXT("false"), *Camera->Message);
	}
}

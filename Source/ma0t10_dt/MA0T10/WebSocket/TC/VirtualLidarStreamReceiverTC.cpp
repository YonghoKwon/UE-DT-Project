#include "ma0t10_dt/MA0T10/WebSocket/TC/VirtualLidarStreamReceiverTC.h"

#include "Json.h"
#include "ma0t10_dt/ma0t10_dt.h"

namespace
{
bool ReadRequiredString(const TSharedPtr<FJsonObject>& Root, const TCHAR* Name, FString& Out, FString& Error)
{
	if (!Root->TryGetStringField(Name, Out) || Out.IsEmpty())
	{
		Error = FString::Printf(TEXT("필수 문자열 필드가 없습니다: %s"), Name);
		return false;
	}
	return true;
}

bool ReadRequiredInt64(const TSharedPtr<FJsonObject>& Root, const TCHAR* Name, int64& Out, FString& Error)
{
	double Number = 0.0;
	if (!Root->TryGetNumberField(Name, Number) || !FMath::IsFinite(Number))
	{
		Error = FString::Printf(TEXT("필수 숫자 필드가 없습니다: %s"), Name);
		return false;
	}
	Out = static_cast<int64>(Number);
	return true;
}
}

UVirtualLidarStreamReceiverTC::UVirtualLidarStreamReceiverTC()
{
	TransactionCode = TEXT("VIRTUAL_LIDAR_STREAM");
}

TSharedPtr<FTransactionCodeDataBase> UVirtualLidarStreamReceiverTC::ParseToStruct(const FString& JsonString) const
{
	TSharedPtr<FVirtualLidarStreamReceiverData> Data = MakeShared<FVirtualLidarStreamReceiverData>();
	Data->Kind = EVirtualSensorTopicReceiveKind::Lidar;
	Data->MessageBytes = FTCHARToUTF8(*JsonString).Length();
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		Data->Message = TEXT("LiDAR JSON 파싱 실패");
		return Data;
	}
	if (!ReadRequiredString(Root, TEXT("schemaVersion"), Data->SchemaVersion, Data->Message) ||
		!ReadRequiredString(Root, TEXT("sensorId"), Data->SensorId, Data->Message) ||
		!ReadRequiredInt64(Root, TEXT("frameId"), Data->FrameId, Data->Message))
	{
		return Data;
	}
	if (Data->SchemaVersion != TEXT("virtual-lidar.v1"))
	{
		Data->Message = FString::Printf(TEXT("지원하지 않는 LiDAR schema: %s"), *Data->SchemaVersion);
		return Data;
	}
	double Horizontal = 0.0, Vertical = 0.0, Ray = 0.0, Total = 0.0, Hit = 0.0, Payload = 0.0;
	const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
	if (!Root->TryGetNumberField(TEXT("horizontalSamples"), Horizontal) ||
		!Root->TryGetNumberField(TEXT("verticalChannels"), Vertical) ||
		!Root->TryGetNumberField(TEXT("rayCount"), Ray) ||
		!Root->TryGetNumberField(TEXT("totalPointCount"), Total) ||
		!Root->TryGetNumberField(TEXT("hitPointCount"), Hit) ||
		!Root->TryGetNumberField(TEXT("payloadPointCount"), Payload) ||
		!Root->TryGetArrayField(TEXT("points"), Points) || !Points)
	{
		Data->Message = TEXT("LiDAR 측정 카운터 또는 points 배열이 없습니다.");
		return Data;
	}
	Data->HorizontalSamples = static_cast<int32>(Horizontal);
	Data->VerticalChannels = static_cast<int32>(Vertical);
	Data->RayCount = static_cast<int32>(Ray);
	Data->TotalPointCount = static_cast<int32>(Total);
	Data->HitPointCount = static_cast<int32>(Hit);
	Data->PayloadPointCount = static_cast<int32>(Payload);
	if (Data->HorizontalSamples <= 0 || Data->VerticalChannels <= 0 ||
		Data->RayCount != Data->HorizontalSamples * Data->VerticalChannels ||
		Data->TotalPointCount < 0 || Data->HitPointCount < 0 || Data->HitPointCount > Data->TotalPointCount ||
		Data->PayloadPointCount != Points->Num() || Data->PayloadPointCount > Data->TotalPointCount)
	{
		Data->Message = FString::Printf(TEXT("LiDAR 카운터 불일치: rays=%d total=%d hit=%d payload=%d array=%d"),
			Data->RayCount, Data->TotalPointCount, Data->HitPointCount, Data->PayloadPointCount, Points->Num());
		return Data;
	}
	Data->bDeepValidated = Data->FrameId >= 0 && (Data->FrameId % 10) == 0;
	if (Data->bDeepValidated)
	{
		for (const TSharedPtr<FJsonValue>& PointValue : *Points)
		{
			const TSharedPtr<FJsonObject> Point = PointValue.IsValid() ? PointValue->AsObject() : nullptr;
			bool bHit = false;
			double Distance = 0.0, Row = 0.0, Col = 0.0;
			if (!Point.IsValid() || !Point->TryGetBoolField(TEXT("hit"), bHit) ||
				!Point->TryGetNumberField(TEXT("distance"), Distance) ||
				!Point->TryGetNumberField(TEXT("row"), Row) || !Point->TryGetNumberField(TEXT("col"), Col) ||
				!FMath::IsFinite(Distance) || Distance < 0.0)
			{
				Data->Message = TEXT("LiDAR points 상세 검증 실패");
				return Data;
			}
		}
	}
	Data->bValid = true;
	Data->Message = FString::Printf(TEXT("LiDAR 수신 정상 · %dx%d · 측정 %d · 검출 %d · Payload %d%s"),
		Data->HorizontalSamples, Data->VerticalChannels, Data->TotalPointCount, Data->HitPointCount,
		Data->PayloadPointCount, Data->bDeepValidated ? TEXT(" · 상세 검증") : TEXT(""));
	return Data;
}

void UVirtualLidarStreamReceiverTC::ProcessStructData(const TSharedPtr<FTransactionCodeDataBase>& Data)
{
	const TSharedPtr<FVirtualLidarStreamReceiverData> Lidar = StaticCastSharedPtr<FVirtualLidarStreamReceiverData>(Data);
	if (!Lidar.IsValid()) return;
	const double Now = FPlatformTime::Seconds();
	if (!Lidar->bValid || LastOutputLogSeconds < 0.0 || Now - LastOutputLogSeconds >= 1.0)
	{
		LastOutputLogSeconds = Now;
		UE_LOG(LogMA0T10, Log, TEXT("[SensorTopicReceiver][LiDAR] sensor=%s frame=%lld bytes=%d valid=%s %s"),
			*Lidar->SensorId, Lidar->FrameId, Lidar->MessageBytes, Lidar->bValid ? TEXT("true") : TEXT("false"), *Lidar->Message);
	}
}

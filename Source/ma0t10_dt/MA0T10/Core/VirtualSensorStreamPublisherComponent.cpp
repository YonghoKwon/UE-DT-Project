#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"

#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Base64.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorActorBase.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarPayloadCodec.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"

namespace
{
FString JoinPointTags(const TArray<FName>& Tags)
{
	TArray<FString> Values;
	Values.Reserve(Tags.Num());
	for (const FName Tag : Tags) Values.Add(Tag.ToString());
	return FString::Join(Values, TEXT("|"));
}

bool ContainsAnyName(const TArray<FName>& Values, const TArray<FName>& Candidates)
{
	for (const FName Candidate : Candidates)
	{
		if (Values.Contains(Candidate)) return true;
	}
	return false;
}

bool PointPassesStreamFilter(const FVirtualLidarPoint& Point, const FVirtualPointCloudFilterConfig& Filter)
{
	if (!Point.bHit) return false;

	if (!Filter.IncludeActorTags.IsEmpty() && !ContainsAnyName(Point.HitActorTags, Filter.IncludeActorTags)) return false;
	if (!Filter.IncludeSemanticLabels.IsEmpty() && !Filter.IncludeSemanticLabels.Contains(Point.SemanticLabel)) return false;

	const FVector LocalCm = Point.SensorLocalPositionMeters * 100.0;
	if (Filter.bEnableSensorLocalRoi &&
		(LocalCm.X < Filter.SensorLocalRoiMinCm.X || LocalCm.Y < Filter.SensorLocalRoiMinCm.Y || LocalCm.Z < Filter.SensorLocalRoiMinCm.Z ||
		 LocalCm.X > Filter.SensorLocalRoiMaxCm.X || LocalCm.Y > Filter.SensorLocalRoiMaxCm.Y || LocalCm.Z > Filter.SensorLocalRoiMaxCm.Z))
	{
		return false;
	}

	const float RangeCm = Point.RangeMillimeters > 0 ? Point.RangeMillimeters * 0.1f : Point.Distance;
	if (RangeCm < FMath::Max(0.0f, Filter.MinRangeCm)) return false;
	if (Filter.MaxRangeCm > 0.0f && RangeCm > Filter.MaxRangeCm) return false;

	if (ContainsAnyName(Point.HitActorTags, Filter.ExcludeActorTags) ||
		Filter.ExcludeSemanticLabels.Contains(Point.SemanticLabel))
	{
		return false;
	}
	return true;
}

void AppendUint16LittleEndian(TArray<uint8>& OutBytes, uint16 Value)
{
	OutBytes.Add(static_cast<uint8>(Value));
	OutBytes.Add(static_cast<uint8>(Value >> 8));
}

void AppendUint32LittleEndian(TArray<uint8>& OutBytes, uint32 Value)
{
	OutBytes.Add(static_cast<uint8>(Value));
	OutBytes.Add(static_cast<uint8>(Value >> 8));
	OutBytes.Add(static_cast<uint8>(Value >> 16));
	OutBytes.Add(static_cast<uint8>(Value >> 24));
}

void AppendUint64LittleEndian(TArray<uint8>& OutBytes, uint64 Value)
{
	for (int32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
	{
		OutBytes.Add(static_cast<uint8>(Value >> (ByteIndex * 8)));
	}
}

void AppendFloatLittleEndian(TArray<uint8>& OutBytes, float Value)
{
	uint32 Bits = 0;
	static_assert(sizeof(Bits) == sizeof(Value), "PCD float layout changed");
	FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
	AppendUint32LittleEndian(OutBytes, Bits);
}

bool SerializeBinaryPcd(
	const TArray<FVirtualLidarPoint>& Source,
	const FVirtualPointCloudFilterConfig& Filter,
	TArray<uint8>& OutBytes,
	int32& OutPointCount)
{
	TArray<const FVirtualLidarPoint*> FilteredPoints;
	FilteredPoints.Reserve(Source.Num());
	for (const FVirtualLidarPoint& Point : Source)
	{
		if (PointPassesStreamFilter(Point, Filter)) FilteredPoints.Add(&Point);
	}
	OutPointCount = FilteredPoints.Num();

	const FString Header = FString::Printf(
		TEXT("# .PCD v0.7\nVERSION 0.7\nFIELDS x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence\n")
		TEXT("SIZE 4 4 4 2 2 2 1 1 8 1 4\nTYPE F F F U U U U U I U F\nCOUNT 1 1 1 1 1 1 1 1 1 1 1\n")
		TEXT("WIDTH %d\nHEIGHT 1\nVIEWPOINT 0 0 0 1 0 0 0\nPOINTS %d\nDATA binary\n"),
		OutPointCount,
		OutPointCount);
	FTCHARToUTF8 HeaderUtf8(*Header);
	constexpr int32 PointRecordBytes = 33;
	OutBytes.Reserve(HeaderUtf8.Length() + OutPointCount * PointRecordBytes);
	OutBytes.Append(reinterpret_cast<const uint8*>(HeaderUtf8.Get()), HeaderUtf8.Length());

	for (const FVirtualLidarPoint* Point : FilteredPoints)
	{
		AppendFloatLittleEndian(OutBytes, static_cast<float>(Point->SensorLocalPositionMeters.X));
		AppendFloatLittleEndian(OutBytes, static_cast<float>(Point->SensorLocalPositionMeters.Y));
		AppendFloatLittleEndian(OutBytes, static_cast<float>(Point->SensorLocalPositionMeters.Z));
		AppendUint16LittleEndian(OutBytes, static_cast<uint16>(FMath::Clamp(Point->RawIntensity, 0, 65535)));
		AppendUint16LittleEndian(OutBytes, static_cast<uint16>(FMath::Clamp(Point->Ring, 0, 65535)));
		AppendUint16LittleEndian(OutBytes, static_cast<uint16>(FMath::Clamp(Point->HorizontalIndex, 0, 65535)));
		OutBytes.Add(static_cast<uint8>(FMath::Clamp(Point->EchoIndex, 0, 255)));
		OutBytes.Add(static_cast<uint8>(FMath::Clamp(Point->EchoCount, 0, 255)));
		AppendUint64LittleEndian(OutBytes, static_cast<uint64>(Point->PointTimeOffsetNanoseconds));
		OutBytes.Add(static_cast<uint8>(Point->Validity));
		AppendFloatLittleEndian(OutBytes, Point->Confidence);
	}
	return true;
}

template <typename T>
void WriteLasValue(FBufferArchive& Archive, const T& Value)
{
	Archive.Serialize(const_cast<T*>(&Value), sizeof(T));
}

void WriteLasFixedString(FBufferArchive& Archive, const ANSICHAR* Text, int32 Length)
{
	TArray<ANSICHAR> Buffer;
	Buffer.SetNumZeroed(Length);
	if (Text) FCStringAnsi::Strncpy(Buffer.GetData(), Text, Length);
	Archive.Serialize(Buffer.GetData(), Length);
}

bool SerializeLas(const TArray<FVirtualLidarPoint>& Source, TArray<uint8>& OutBytes, int32& OutPointCount)
{
	TArray<const FVirtualLidarPoint*> Points;
	for (const FVirtualLidarPoint& Point : Source) if (Point.bHit) Points.Add(&Point);
	OutPointCount = Points.Num();
	if (Points.IsEmpty()) return false;

	FVector Min(FLT_MAX), Max(-FLT_MAX);
	for (const FVirtualLidarPoint* Point : Points)
	{
		Min.X = FMath::Min(Min.X, Point->WorldLocation.X); Min.Y = FMath::Min(Min.Y, Point->WorldLocation.Y); Min.Z = FMath::Min(Min.Z, Point->WorldLocation.Z);
		Max.X = FMath::Max(Max.X, Point->WorldLocation.X); Max.Y = FMath::Max(Max.Y, Point->WorldLocation.Y); Max.Z = FMath::Max(Max.Z, Point->WorldLocation.Z);
	}
	constexpr double Scale = 0.001;
	constexpr double CmToM = 0.01;
	const double OX = Min.X * CmToM, OY = Min.Y * CmToM, OZ = Min.Z * CmToM;
	FBufferArchive Archive;
	const uint8 Signature[4] = {'L', 'A', 'S', 'F'};
	Archive.Serialize(const_cast<uint8*>(Signature), 4);
	WriteLasValue<uint16>(Archive, 0); WriteLasValue<uint16>(Archive, 0); WriteLasValue<uint32>(Archive, 0);
	WriteLasValue<uint16>(Archive, 0); WriteLasValue<uint16>(Archive, 0);
	for (int32 Index = 0; Index < 8; ++Index) WriteLasValue<uint8>(Archive, 0);
	WriteLasValue<uint8>(Archive, 1); WriteLasValue<uint8>(Archive, 2);
	WriteLasFixedString(Archive, "MA0T10", 32); WriteLasFixedString(Archive, "VirtualSensorStream", 32);
	const FDateTime Now = FDateTime::Now();
	WriteLasValue<uint16>(Archive, static_cast<uint16>(Now.GetDayOfYear())); WriteLasValue<uint16>(Archive, static_cast<uint16>(Now.GetYear()));
	WriteLasValue<uint16>(Archive, 227); WriteLasValue<uint32>(Archive, 227); WriteLasValue<uint32>(Archive, 0);
	WriteLasValue<uint8>(Archive, 0); WriteLasValue<uint16>(Archive, 20);
	WriteLasValue<uint32>(Archive, static_cast<uint32>(Points.Num())); WriteLasValue<uint32>(Archive, static_cast<uint32>(Points.Num()));
	for (int32 Index = 1; Index < 5; ++Index) WriteLasValue<uint32>(Archive, 0);
	WriteLasValue<double>(Archive, Scale); WriteLasValue<double>(Archive, Scale); WriteLasValue<double>(Archive, Scale);
	WriteLasValue<double>(Archive, OX); WriteLasValue<double>(Archive, OY); WriteLasValue<double>(Archive, OZ);
	WriteLasValue<double>(Archive, Max.X * CmToM); WriteLasValue<double>(Archive, Min.X * CmToM);
	WriteLasValue<double>(Archive, Max.Y * CmToM); WriteLasValue<double>(Archive, Min.Y * CmToM);
	WriteLasValue<double>(Archive, Max.Z * CmToM); WriteLasValue<double>(Archive, Min.Z * CmToM);
	for (const FVirtualLidarPoint* Point : Points)
	{
		WriteLasValue<int32>(Archive, static_cast<int32>(FMath::RoundToDouble(((Point->WorldLocation.X * CmToM) - OX) / Scale)));
		WriteLasValue<int32>(Archive, static_cast<int32>(FMath::RoundToDouble(((Point->WorldLocation.Y * CmToM) - OY) / Scale)));
		WriteLasValue<int32>(Archive, static_cast<int32>(FMath::RoundToDouble(((Point->WorldLocation.Z * CmToM) - OZ) / Scale)));
		WriteLasValue<uint16>(Archive, static_cast<uint16>(FMath::Clamp(Point->RawIntensity, 0, 65535)));
		const uint8 ReturnBits = static_cast<uint8>(
			(FMath::Clamp(Point->EchoIndex + 1, 1, 7) & 0x07)
			| ((FMath::Clamp(Point->EchoCount, 1, 7) & 0x07) << 3));
		WriteLasValue<uint8>(Archive, ReturnBits);
		WriteLasValue<uint8>(Archive, 1);
		WriteLasValue<int8>(Archive, 0);
		WriteLasValue<uint8>(Archive, 0);
		WriteLasValue<uint16>(Archive, static_cast<uint16>(FMath::Clamp(Point->Ring, 0, 65535)));
	}
	OutBytes.Append(Archive.GetData(), Archive.Num());
	return true;
}

bool SerializePointCloud(
	const FVirtualSensorFrameEnvelope& Frame,
	const FVirtualSensorStreamConfig& Config,
	FString& OutExtension,
	TArray<uint8>& OutBytes,
	int32& OutPointCount,
	FString& OutError)
{
	if (!Frame.PointSnapshot.IsValid())
	{
		OutError = TEXT("포인트 스냅샷이 없습니다.");
		return false;
	}
	const TArray<FVirtualLidarPoint>& Points = *Frame.PointSnapshot;
	FString Text;
	if (Config.PointCloudFormat == EVirtualPointCloudStreamFormat::CompactBinary)
	{
		if (!Frame.LidarFrameSnapshot.IsValid())
		{
			OutError = TEXT("Compact Binary 전송에는 virtual-lidar.v2 물리 프레임이 필요합니다.");
			return false;
		}
		FVirtualLidarV2EncodeOptions Options;
		Options.bIncludeInvalidPoints = false;
		TArray64<uint8> CompactBytes;
		if (!FVirtualLidarPayloadCodec::EncodeCompactBinary(
			*Frame.LidarFrameSnapshot,
			Options,
			CompactBytes,
			OutPointCount))
		{
			OutError = TEXT("Compact Binary로 인코딩할 유효점이 없습니다.");
			return false;
		}
		if (CompactBytes.Num() > MAX_int32)
		{
			OutError = TEXT("Compact Binary 프레임이 단일 STOMP 메시지 한도를 초과합니다.");
			return false;
		}
		OutBytes.Append(CompactBytes.GetData(), static_cast<int32>(CompactBytes.Num()));
		OutExtension = TEXT("vlb2");
		return true;
	}
	if (Config.PointCloudFormat == EVirtualPointCloudStreamFormat::CSV)
	{
		OutExtension = TEXT("csv");
		Text = TEXT("x,y,z,distance,hit,sensor_x_m,sensor_y_m,sensor_z_m,range_mm,intensity,ring,horizontal_index,echo_index,echo_count,time_offset_ns,validity,confidence,actor,actor_class,semantic_label,tags\n");
		for (const FVirtualLidarPoint& Point : Points) if (Point.bHit)
		{
			++OutPointCount;
			Text += FString::Printf(
				TEXT("%.6f,%.6f,%.6f,%.6f,1,%.9f,%.9f,%.9f,%d,%d,%d,%d,%d,%d,%lld,%d,%.6f,%s,%s,%s,%s\n"),
				Point.WorldLocation.X,
				Point.WorldLocation.Y,
				Point.WorldLocation.Z,
				Point.Distance,
				Point.SensorLocalPositionMeters.X,
				Point.SensorLocalPositionMeters.Y,
				Point.SensorLocalPositionMeters.Z,
				Point.RangeMillimeters,
				Point.RawIntensity,
				Point.Ring,
				Point.HorizontalIndex,
				Point.EchoIndex,
				Point.EchoCount,
				Point.PointTimeOffsetNanoseconds,
				static_cast<int32>(Point.Validity),
				Point.Confidence,
				*Point.HitActorName.ToString(),
				*Point.HitActorClassName.ToString(),
				*Point.SemanticLabel.ToString(),
				*JoinPointTags(Point.HitActorTags));
		}
	}
	else if (Config.PointCloudFormat == EVirtualPointCloudStreamFormat::JSONL)
	{
		OutExtension = TEXT("jsonl");
		for (const FVirtualLidarPoint& Point : Points) if (Point.bHit)
		{
			++OutPointCount;
			Text += FString::Printf(
				TEXT("{\"x\":%.6f,\"y\":%.6f,\"z\":%.6f,\"distance\":%.6f,\"hit\":true,\"sensorPositionMeters\":[%.9f,%.9f,%.9f],\"rangeMillimeters\":%d,\"intensity\":%d,\"ring\":%d,\"horizontalIndex\":%d,\"echoIndex\":%d,\"echoCount\":%d,\"pointTimeOffsetNanoseconds\":\"%lld\",\"validity\":%d,\"confidence\":%.6f,\"semanticLabel\":\"%s\"}\n"),
				Point.WorldLocation.X,
				Point.WorldLocation.Y,
				Point.WorldLocation.Z,
				Point.Distance,
				Point.SensorLocalPositionMeters.X,
				Point.SensorLocalPositionMeters.Y,
				Point.SensorLocalPositionMeters.Z,
				Point.RangeMillimeters,
				Point.RawIntensity,
				Point.Ring,
				Point.HorizontalIndex,
				Point.EchoIndex,
				Point.EchoCount,
				Point.PointTimeOffsetNanoseconds,
				static_cast<int32>(Point.Validity),
				Point.Confidence,
				*Point.SemanticLabel.ToString());
		}
	}
	else if (Config.PointCloudFormat == EVirtualPointCloudStreamFormat::PCD)
	{
		OutExtension = TEXT("pcd");
		if (Config.PcdDataMode == EVirtualPcdDataMode::Binary)
		{
			return SerializeBinaryPcd(Points, Config.PointCloudFilter, OutBytes, OutPointCount);
		}
		for (const FVirtualLidarPoint& Point : Points) if (Point.bHit) ++OutPointCount;
		Text = FString::Printf(TEXT("# .PCD v0.7\nVERSION 0.7\nFIELDS x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence\nSIZE 4 4 4 2 2 2 1 1 8 1 4\nTYPE F F F U U U U U I U F\nCOUNT 1 1 1 1 1 1 1 1 1 1 1\nWIDTH %d\nHEIGHT 1\nPOINTS %d\nDATA ascii\n"), OutPointCount, OutPointCount);
		for (const FVirtualLidarPoint& Point : Points) if (Point.bHit)
		{
			Text += FString::Printf(
				TEXT("%.9f %.9f %.9f %d %d %d %d %d %lld %d %.6f\n"),
				Point.SensorLocalPositionMeters.X,
				Point.SensorLocalPositionMeters.Y,
				Point.SensorLocalPositionMeters.Z,
				Point.RawIntensity,
				Point.Ring,
				Point.HorizontalIndex,
				Point.EchoIndex,
				Point.EchoCount,
				Point.PointTimeOffsetNanoseconds,
				static_cast<int32>(Point.Validity),
				Point.Confidence);
		}
	}
	else
	{
		TArray<uint8> LasBytes;
		if (!SerializeLas(Points, LasBytes, OutPointCount))
		{
			OutError = TEXT("LAS로 변환할 검출점이 없습니다.");
			return false;
		}
		if (Config.PointCloudFormat == EVirtualPointCloudStreamFormat::LAS)
		{
			OutExtension = TEXT("las");
			OutBytes = MoveTemp(LasBytes);
			return true;
		}
		if (Config.LazCompressorPath.IsEmpty() || !FPaths::FileExists(Config.LazCompressorPath))
		{
			OutError = TEXT("실제 LAZ 압축 실행 파일이 설정되지 않았습니다. LAS로 위장한 LAZ는 전송하지 않습니다.");
			return false;
		}
		if (!Config.LazCompressorArguments.Contains(TEXT("{input}")) || !Config.LazCompressorArguments.Contains(TEXT("{output}")))
		{
			OutError = TEXT("LAZ 인수에는 {input}, {output}이 모두 필요합니다.");
			return false;
		}
		const FString TempDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SensorStreamTemp"));
		IFileManager::Get().MakeDirectory(*TempDir, true);
		const FString Stem = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString InputPath = FPaths::Combine(TempDir, Stem + TEXT(".las"));
		const FString OutputPath = FPaths::Combine(TempDir, Stem + TEXT(".laz"));
		if (!FFileHelper::SaveArrayToFile(LasBytes, *InputPath))
		{
			OutError = TEXT("LAZ 임시 LAS 파일 생성에 실패했습니다.");
			return false;
		}
		FString Args = Config.LazCompressorArguments;
		Args.ReplaceInline(TEXT("{input}"), *FString::Printf(TEXT("\"%s\""), *InputPath));
		Args.ReplaceInline(TEXT("{output}"), *FString::Printf(TEXT("\"%s\""), *OutputPath));
		int32 ReturnCode = INDEX_NONE; FString StdOut, StdErr;
		const bool bRan = FPlatformProcess::ExecProcess(*Config.LazCompressorPath, *Args, &ReturnCode, &StdOut, &StdErr);
		const bool bLoaded = bRan && ReturnCode == 0 && FFileHelper::LoadFileToArray(OutBytes, *OutputPath) && !OutBytes.IsEmpty();
		IFileManager::Get().Delete(*InputPath, false, true);
		IFileManager::Get().Delete(*OutputPath, false, true);
		if (!bLoaded)
		{
			OutError = FString::Printf(TEXT("LAZ 압축 실패(code=%d): %s"), ReturnCode, *StdErr.Left(256));
			return false;
		}
		OutExtension = TEXT("laz");
		return true;
	}
	FTCHARToUTF8 Utf8(*Text);
	OutBytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	return OutPointCount > 0;
}

FString BuildPointCloudEnvelope(const FVirtualSensorFrameEnvelope& Frame, const FString& Extension, const TArray<uint8>& Bytes, int32 PointCount)
{
	const FString Encoded = FBase64::Encode(Bytes);
	return FString::Printf(
		TEXT("{\"schema\":\"virtual-pointcloud.v1\",\"sensorId\":\"%s\",\"frameId\":%lld,\"timestampUtc\":\"%s\",\"format\":\"%s\",\"encoding\":\"base64\",\"pointCount\":%d,\"byteCount\":%d,\"data\":\"%s\"}"),
		*Frame.SensorId, Frame.FrameId, *Frame.TimestampUtc.ToIso8601(), *Extension.ToUpper(), PointCount, Bytes.Num(), *Encoded);
}
}

UVirtualSensorStreamPublisherComponent::UVirtualSensorStreamPublisherComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.0f;
}

bool UVirtualSensorStreamPublisherComponent::SerializePointCloudForTesting(
	const FVirtualSensorFrameEnvelope& Frame,
	const FVirtualSensorStreamConfig& Config,
	FString& OutExtension,
	TArray<uint8>& OutBytes,
	int32& OutPointCount,
	FString& OutError)
{
	return SerializePointCloud(Frame, Config, OutExtension, OutBytes, OutPointCount, OutError);
}

void UVirtualSensorStreamPublisherComponent::BeginPlay()
{
	Super::BeginPlay();
	LastTokenUpdateSeconds = FPlatformTime::Seconds();
	TokenBucketBytes = BandwidthLimitMegabytesPerSecond * 1024.0 * 1024.0;
	PointCloudTokenBucketBytes = PointCloudBandwidthLimitMegabytesPerSecond * 1024.0 * 1024.0;
	if (TransportComponent) TransportComponent->OnDataSent.AddUniqueDynamic(this, &UVirtualSensorStreamPublisherComponent::OnTransportResult);
}

void UVirtualSensorStreamPublisherComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	if (TransportComponent) TransportComponent->OnDataSent.RemoveDynamic(this, &UVirtualSensorStreamPublisherComponent::OnTransportResult);
	WaitingReceipts.Reset();
	RequestToStreamKey.Reset();
	StreamRuntimes.Reset();
	Super::EndPlay(EndPlayReason);
}

void UVirtualSensorStreamPublisherComponent::SetTransportComponent(UVirtualSensorTransportComponent* InTransportComponent)
{
	if (TransportComponent == InTransportComponent) return;
	if (TransportComponent) TransportComponent->OnDataSent.RemoveDynamic(this, &UVirtualSensorStreamPublisherComponent::OnTransportResult);
	TransportComponent = InTransportComponent;
	if (TransportComponent) TransportComponent->OnDataSent.AddUniqueDynamic(this, &UVirtualSensorStreamPublisherComponent::OnTransportResult);
}

FString UVirtualSensorStreamPublisherComponent::MakeStreamKey(EVirtualSensorStreamKind StreamKind, const FString& SensorId) const
{
	return FString::Printf(TEXT("%d|%s"), static_cast<int32>(StreamKind), SensorId.IsEmpty() ? TEXT("*") : *SensorId);
}

UVirtualSensorStreamPublisherComponent::FStreamRuntime& UVirtualSensorStreamPublisherComponent::FindOrAddRuntime(EVirtualSensorStreamKind StreamKind, const FString& SensorId)
{
	const FString Key = MakeStreamKey(StreamKind, SensorId);
	FStreamRuntime& Runtime = StreamRuntimes.FindOrAdd(Key);
	Runtime.Config.StreamKind = StreamKind;
	Runtime.Config.SensorId = SensorId;
	Runtime.Status.StreamKind = StreamKind;
	Runtime.Status.SensorId = SensorId;
	return Runtime;
}

void UVirtualSensorStreamPublisherComponent::ConfigureStream(const FVirtualSensorStreamConfig& Config)
{
	FStreamRuntime& Runtime = FindOrAddRuntime(Config.StreamKind, Config.SensorId.TrimStartAndEnd());
	const bool bWasEnabled = Runtime.Config.bEnabled;
	const bool bSerializationContractChanged = Runtime.Config.PointCloudFormat != Config.PointCloudFormat ||
		Runtime.Config.PcdDataMode != Config.PcdDataMode ||
		Runtime.Config.DeliveryMode != Config.DeliveryMode ||
		Runtime.Config.PointCloudFilter.Revision != Config.PointCloudFilter.Revision ||
		Runtime.Config.FrameStride != Config.FrameStride ||
		Runtime.Config.LazCompressorPath != Config.LazCompressorPath ||
		Runtime.Config.LazCompressorArguments != Config.LazCompressorArguments;
	if (bSerializationContractChanged)
	{
		++Runtime.ConfigRevision;
		Runtime.PendingFrame.Reset();
		Runtime.PreparedMessage.Reset();
		Runtime.PendingFrameQueue.Reset();
		Runtime.PreparedMessageQueue.Reset();
		Runtime.Status.bPendingLatestFrame = false;
	}
	Runtime.Config = Config;
	Runtime.Config.SensorId = Config.SensorId.TrimStartAndEnd();
	Runtime.Config.FrameStride = FMath::Max(1, Config.FrameStride);
	Runtime.Config.ReceiptSampleInterval = FMath::Max(1, Config.ReceiptSampleInterval);
	Runtime.Config.MaxBufferedFrames = FMath::Clamp(Config.MaxBufferedFrames, 1, 120);
	Runtime.Config.MaxReceiptRetries = FMath::Clamp(Config.MaxReceiptRetries, 0, 10);
	if (Runtime.Config.StreamKind == EVirtualSensorStreamKind::PointCloud &&
		Runtime.Config.DeliveryMode == EVirtualPointCloudDeliveryMode::ConnectedNoLoss)
	{
		Runtime.Config.PointCloudFormat = EVirtualPointCloudStreamFormat::PCD;
		Runtime.Config.PcdDataMode = EVirtualPcdDataMode::Binary;
		Runtime.Config.FrameStride = 1;
		Runtime.Config.ReceiptSampleInterval = 1;
	}
	Runtime.Config.MaxLazHz = FMath::Clamp(Config.MaxLazHz, 0.1f, 1.0f);
	Runtime.Status.bEnabled = Runtime.Config.bEnabled;
	Runtime.Status.ConfigRevision = Runtime.ConfigRevision;
	RefreshQueueTelemetry(Runtime);
	if (bWasEnabled != Runtime.Config.bEnabled) UpdateCameraStreamDemand();
}

void UVirtualSensorStreamPublisherComponent::StartStream(EVirtualSensorStreamKind StreamKind, const FString& SensorId)
{
	FStreamRuntime& Runtime = FindOrAddRuntime(StreamKind, SensorId.TrimStartAndEnd());
	Runtime.Config.bEnabled = true;
	Runtime.Status.bEnabled = true;
	Runtime.Status.bOverloaded = false;
	Runtime.Status.Message = TEXT("실시간 전송 대기 중");
	AddLog(MakeStreamKey(StreamKind, SensorId.TrimStartAndEnd()), TEXT("started"), TEXT("스트림을 시작했습니다."));
	UpdateCameraStreamDemand();
}

void UVirtualSensorStreamPublisherComponent::StopStream(EVirtualSensorStreamKind StreamKind, const FString& SensorId)
{
	const FString Key = MakeStreamKey(StreamKind, SensorId.TrimStartAndEnd());
	if (FStreamRuntime* Runtime = StreamRuntimes.Find(Key))
	{
		Runtime->Config.bEnabled = false;
		Runtime->Status.bEnabled = false;
		Runtime->PendingFrame.Reset();
		Runtime->PreparedMessage.Reset();
		Runtime->PendingFrameQueue.Reset();
		Runtime->PreparedMessageQueue.Reset();
		Runtime->Status.bPendingLatestFrame = false;
		RefreshQueueTelemetry(*Runtime);
		Runtime->Status.Message = TEXT("중지됨");
		AddLog(Key, TEXT("stopped"), TEXT("스트림을 중지했습니다."));
	}
	UpdateCameraStreamDemand();
}

void UVirtualSensorStreamPublisherComponent::StartAllStreams(const FString& SensorId)
{
	StartStream(EVirtualSensorStreamKind::LidarPayload, SensorId);
	StartStream(EVirtualSensorStreamKind::CameraImage, SensorId);
	StartStream(EVirtualSensorStreamKind::PointCloud, SensorId);
}

void UVirtualSensorStreamPublisherComponent::StopAllStreams(const FString& SensorId)
{
	if (SensorId.TrimStartAndEnd().IsEmpty())
	{
		for (TPair<FString, FStreamRuntime>& Pair : StreamRuntimes)
		{
			Pair.Value.Config.bEnabled = false;
			Pair.Value.Status.bEnabled = false;
			Pair.Value.PendingFrame.Reset();
			Pair.Value.PreparedMessage.Reset();
			Pair.Value.PendingFrameQueue.Reset();
			Pair.Value.PreparedMessageQueue.Reset();
			Pair.Value.Status.bPendingLatestFrame = false;
			RefreshQueueTelemetry(Pair.Value);
			Pair.Value.Status.Message = TEXT("중지됨");
		}
		UpdateCameraStreamDemand();
		return;
	}
	StopStream(EVirtualSensorStreamKind::LidarPayload, SensorId);
	StopStream(EVirtualSensorStreamKind::CameraImage, SensorId);
	StopStream(EVirtualSensorStreamKind::PointCloud, SensorId);
}

bool UVirtualSensorStreamPublisherComponent::IsStreamEnabled(EVirtualSensorStreamKind StreamKind, const FString& SensorId) const
{
	if (const FStreamRuntime* Runtime = StreamRuntimes.Find(MakeStreamKey(StreamKind, SensorId.TrimStartAndEnd()))) return Runtime->Config.bEnabled;
	return false;
}

bool UVirtualSensorStreamPublisherComponent::StreamMatchesFrame(EVirtualSensorStreamKind StreamKind, EVirtualSensorKind SensorKind)
{
	return StreamKind == EVirtualSensorStreamKind::CameraImage
		? SensorKind == EVirtualSensorKind::Camera
		: SensorKind == EVirtualSensorKind::Lidar;
}

void UVirtualSensorStreamPublisherComponent::SubmitFrame(const FVirtualSensorFrameEnvelope& Frame)
{
	if (bEndingPlay || Frame.SensorId.IsEmpty()) return;
	TArray<FString> Keys;
	StreamRuntimes.GetKeys(Keys);
	for (const FString& Key : Keys)
	{
		FStreamRuntime* Runtime = StreamRuntimes.Find(Key);
		if (!Runtime || !Runtime->Config.bEnabled || !StreamMatchesFrame(Runtime->Config.StreamKind, Frame.SensorKind)) continue;
		if (!Runtime->Config.SensorId.IsEmpty() && !Runtime->Config.SensorId.Equals(Frame.SensorId, ESearchCase::CaseSensitive)) continue;
		if (Runtime->Config.SensorId.IsEmpty())
		{
			const FStreamRuntime* Exact = StreamRuntimes.Find(MakeStreamKey(Runtime->Config.StreamKind, Frame.SensorId));
			if (Exact && Exact->Config.bEnabled) continue;
		}
		QueueFrameForRuntime(Key, *Runtime, Frame);
	}
}

void UVirtualSensorStreamPublisherComponent::QueueFrameForRuntime(const FString& StreamKey, FStreamRuntime& Runtime, const FVirtualSensorFrameEnvelope& Frame)
{
	if (Runtime.Status.InputFrameCount > 0 && Runtime.Status.LastInputFrameId == Frame.FrameId)
	{
		return;
	}
	if (Runtime.Status.InputFrameCount > 0 && Frame.FrameId > Runtime.Status.LastInputFrameId + 1)
	{
		Runtime.Status.FrameGapCount += Frame.FrameId - Runtime.Status.LastInputFrameId - 1;
	}
	++Runtime.Status.InputFrameCount;
	Runtime.Status.LastInputFrameId = Frame.FrameId;
	const double Now = FPlatformTime::Seconds();
	if (Runtime.FirstInputSeconds <= 0.0) Runtime.FirstInputSeconds = Now;
	Runtime.Status.InputHz = static_cast<float>(Runtime.Status.InputFrameCount / FMath::Max(0.001, Now - Runtime.FirstInputSeconds));
	if (((Runtime.Status.InputFrameCount - 1) % Runtime.Config.FrameStride) != 0) return;

	if (Runtime.Config.StreamKind == EVirtualSensorStreamKind::PointCloud)
	{
		const bool bNoLoss = Runtime.Config.DeliveryMode == EVirtualPointCloudDeliveryMode::ConnectedNoLoss;
		if (!Runtime.bSerializationInFlight)
		{
			if (bNoLoss && Runtime.PreparedMessageQueue.Num() >= Runtime.Config.MaxBufferedFrames)
			{
				if (Runtime.PendingFrameQueue.Num() >= Runtime.Config.MaxBufferedFrames)
				{
					StopForPointCloudOverload(StreamKey, Runtime, TEXT("Binary PCD input queue reached its limit before transport drained."));
				}
				else
				{
					Runtime.PendingFrameQueue.Add(Frame);
				}
			}
			else
			{
				StartPointCloudSerialization(StreamKey, Runtime, Frame);
			}
		}
		else
		{
			if (bNoLoss)
			{
				if (Runtime.PendingFrameQueue.Num() >= Runtime.Config.MaxBufferedFrames)
				{
					StopForPointCloudOverload(StreamKey, Runtime, TEXT("Binary PCD serialization queue reached its limit."));
				}
				else
				{
					Runtime.PendingFrameQueue.Add(Frame);
				}
			}
			else
			{
				if (Runtime.PendingFrame.IsSet()) ++Runtime.Status.ReplacedPendingFrameCount;
				Runtime.PendingFrame = Frame;
				Runtime.Status.bPendingLatestFrame = true;
			}
		}
		RefreshQueueTelemetry(Runtime);
		return;
	}
	if (!Frame.HasJsonPayload())
	{
		++Runtime.Status.EncodeFailureCount;
		Runtime.Status.Message = TEXT("완료된 JSON Payload가 없습니다.");
		return;
	}
	FPreparedMessage Message;
	Message.SensorId = Frame.SensorId;
	Message.StreamKind = Runtime.Config.StreamKind;
	Message.FrameId = Frame.FrameId;
		Message.Json = *Frame.JsonPayload;
		Message.ByteCount = FTCHARToUTF8(*Message.Json).Length();
		Message.ConfigRevision = Runtime.ConfigRevision;
	if (Runtime.PreparedMessage.IsSet()) ++Runtime.Status.ReplacedPendingFrameCount;
	Runtime.PreparedMessage = MoveTemp(Message);
	Runtime.Status.bPendingLatestFrame = true;
}

void UVirtualSensorStreamPublisherComponent::RefreshQueueTelemetry(FStreamRuntime& Runtime)
{
	Runtime.Status.InputQueueDepth = Runtime.PendingFrameQueue.Num() + (Runtime.PendingFrame.IsSet() ? 1 : 0);
	Runtime.Status.PreparedQueueDepth = Runtime.PreparedMessageQueue.Num() + (Runtime.PreparedMessage.IsSet() ? 1 : 0);
	int32 ReceiptDepth = 0;
	for (const TPair<FString, FReceiptWait>& Pair : WaitingReceipts)
	{
		if (Pair.Value.StreamKey == MakeStreamKey(Runtime.Config.StreamKind, Runtime.Config.SensorId)) ++ReceiptDepth;
	}
	Runtime.Status.ReceiptQueueDepth = ReceiptDepth;
	Runtime.Status.bPendingLatestFrame = Runtime.Status.InputQueueDepth > 0 || Runtime.Status.PreparedQueueDepth > 0;
}

void UVirtualSensorStreamPublisherComponent::StopForPointCloudOverload(
	const FString& StreamKey,
	FStreamRuntime& Runtime,
	const FString& Reason)
{
	++Runtime.Status.OverloadCount;
	Runtime.Status.bOverloaded = true;
	Runtime.Status.bEnabled = false;
	Runtime.Config.bEnabled = false;
	Runtime.Status.Message = Reason;
	Runtime.PendingFrame.Reset();
	Runtime.PendingFrameQueue.Reset();
	Runtime.PreparedMessage.Reset();
	Runtime.PreparedMessageQueue.Reset();
	TArray<FString> ReceiptIdsToRemove;
	for (const TPair<FString, FReceiptWait>& Pair : WaitingReceipts)
	{
		if (Pair.Value.StreamKey == StreamKey) ReceiptIdsToRemove.Add(Pair.Key);
	}
	for (const FString& RequestId : ReceiptIdsToRemove)
	{
		WaitingReceipts.Remove(RequestId);
		RequestToStreamKey.Remove(RequestId);
	}
	RefreshQueueTelemetry(Runtime);
	AddLog(StreamKey, TEXT("overload-stopped"), Reason);
}

void UVirtualSensorStreamPublisherComponent::TryStartNextPointCloudSerialization(
	const FString& StreamKey,
	FStreamRuntime& Runtime)
{
	if (!Runtime.Config.bEnabled || Runtime.bSerializationInFlight || Runtime.PendingFrameQueue.IsEmpty()) return;
	if (Runtime.PreparedMessageQueue.Num() >= Runtime.Config.MaxBufferedFrames) return;
	FVirtualSensorFrameEnvelope Next = MoveTemp(Runtime.PendingFrameQueue[0]);
	Runtime.PendingFrameQueue.RemoveAt(0, 1, false);
	StartPointCloudSerialization(StreamKey, Runtime, Next);
	RefreshQueueTelemetry(Runtime);
}

void UVirtualSensorStreamPublisherComponent::StartPointCloudSerialization(const FString& StreamKey, FStreamRuntime& Runtime, const FVirtualSensorFrameEnvelope& Frame)
{
	const double Now = FPlatformTime::Seconds();
	if (Runtime.Config.PointCloudFormat == EVirtualPointCloudStreamFormat::LAZ &&
		Now - Runtime.LastLazSubmitSeconds < 1.0 / FMath::Max(0.1f, Runtime.Config.MaxLazHz))
	{
		++Runtime.Status.ReplacedPendingFrameCount;
		Runtime.PendingFrame = Frame;
		Runtime.Status.bPendingLatestFrame = true;
		return;
	}
	Runtime.bSerializationInFlight = true;
	Runtime.Status.bProcessing = true;
	const FVirtualSensorStreamConfig Config = Runtime.Config;
	const int32 CapturedConfigRevision = Runtime.ConfigRevision;
	const TWeakObjectPtr<UVirtualSensorStreamPublisherComponent> WeakThis(this);
	Async(EAsyncExecution::ThreadPool, [WeakThis, StreamKey, Frame, Config, CapturedConfigRevision]()
	{
		const double SerializationStartedSeconds = FPlatformTime::Seconds();
		FString Extension, Error;
		TArray<uint8> Bytes;
		int32 PointCount = 0;
		const bool bSucceeded = SerializePointCloud(Frame, Config, Extension, Bytes, PointCount, Error);
		FPreparedMessage Message;
		Message.SensorId = Frame.SensorId;
			Message.StreamKind = EVirtualSensorStreamKind::PointCloud;
			Message.FrameId = Frame.FrameId;
			Message.ConfigRevision = CapturedConfigRevision;
		if (bSucceeded)
		{
			const bool bRawBinaryPcd = Config.PointCloudFormat == EVirtualPointCloudStreamFormat::PCD &&
				Config.PcdDataMode == EVirtualPcdDataMode::Binary;
			if (bRawBinaryPcd)
			{
				uint8 Hash[FSHA1::DigestSize];
				FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash);
				Message.bBinaryPcd = true;
				Message.BinaryMetadata.SensorId = Frame.SensorId;
				Message.BinaryMetadata.FrameId = Frame.FrameId;
				Message.BinaryMetadata.TimestampUtc = Frame.TimestampUtc.ToIso8601();
				Message.BinaryMetadata.ProfileKey = Frame.LidarFrameSnapshot.IsValid()
					? Frame.LidarFrameSnapshot->ProfileKey : TEXT("unknown");
				Message.BinaryMetadata.SourcePointCount = Frame.PointSnapshot.IsValid() ? Frame.PointSnapshot->Num() : 0;
				Message.BinaryMetadata.PointCount = PointCount;
				Message.BinaryMetadata.ByteCount = Bytes.Num();
				Message.BinaryMetadata.FilterRevision = Config.PointCloudFilter.Revision;
				Message.BinaryMetadata.ChecksumSha1 = BytesToHex(Hash, FSHA1::DigestSize).ToLower();
				Message.ByteCount = Bytes.Num();
				Message.BinaryBody = MakeShared<const TArray<uint8>, ESPMode::ThreadSafe>(MoveTemp(Bytes));
			}
			else
			{
				Message.Json = BuildPointCloudEnvelope(Frame, Extension, Bytes, PointCount);
				Message.ByteCount = FTCHARToUTF8(*Message.Json).Length();
			}
		}
		const float SerializationLatencyMs = static_cast<float>((FPlatformTime::Seconds() - SerializationStartedSeconds) * 1000.0);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, StreamKey, Message = MoveTemp(Message), Error = MoveTemp(Error), CapturedConfigRevision, SerializationLatencyMs]() mutable
		{
			if (WeakThis.IsValid())
			{
				if (FStreamRuntime* Runtime = WeakThis->StreamRuntimes.Find(StreamKey))
				{
					Runtime->Status.LastSerializationLatencyMs = SerializationLatencyMs;
					Runtime->SerializationLatencySamples.Add(SerializationLatencyMs);
					if (Runtime->SerializationLatencySamples.Num() > 256)
					{
						Runtime->SerializationLatencySamples.RemoveAt(0, Runtime->SerializationLatencySamples.Num() - 256, false);
					}
					TArray<float> SortedLatency = Runtime->SerializationLatencySamples;
					SortedLatency.Sort();
					if (!SortedLatency.IsEmpty())
					{
						const int32 P95Index = FMath::Clamp(FMath::CeilToInt(SortedLatency.Num() * 0.95f) - 1, 0, SortedLatency.Num() - 1);
						Runtime->Status.SerializationP95LatencyMs = SortedLatency[P95Index];
					}
					const double NowSeconds = FPlatformTime::Seconds();
					if (Runtime->FirstSerializationSeconds <= 0.0) Runtime->FirstSerializationSeconds = NowSeconds;
					++Runtime->SerializationCompletedCount;
					Runtime->Status.SerializedFrameCount = Runtime->SerializationCompletedCount;
					Runtime->Status.SerializationHz = static_cast<float>(Runtime->SerializationCompletedCount /
						FMath::Max(0.001, NowSeconds - Runtime->FirstSerializationSeconds));
				}
				WeakThis->CompletePointCloudSerialization(StreamKey, MoveTemp(Message), Error, CapturedConfigRevision);
			}
		});
	});
}

void UVirtualSensorStreamPublisherComponent::CompletePointCloudSerialization(const FString& StreamKey, FPreparedMessage&& Message, const FString& Error, int32 CapturedConfigRevision)
{
	FStreamRuntime* Runtime = StreamRuntimes.Find(StreamKey);
	if (!Runtime) return;
	Runtime->bSerializationInFlight = false;
	Runtime->Status.bProcessing = false;
	if (!Runtime->Config.bEnabled || Runtime->Status.bOverloaded)
	{
		// An overload stop deliberately clears every retained frame. A worker
		// that was already running must not repopulate the prepared queue after
		// that stop, otherwise the UI would report a drained error state while a
		// megabyte-scale body remains retained indefinitely.
		RefreshQueueTelemetry(*Runtime);
		return;
	}
	const bool bNoLoss = Runtime->Config.DeliveryMode == EVirtualPointCloudDeliveryMode::ConnectedNoLoss;
	if (CapturedConfigRevision != Runtime->ConfigRevision)
	{
		++Runtime->Status.StaleResultDiscardCount;
		Runtime->Status.Message = TEXT("설정 변경 전에 시작된 Point Cloud 결과를 폐기했습니다.");
		if (bNoLoss)
		{
			TryStartNextPointCloudSerialization(StreamKey, *Runtime);
		}
		else if (Runtime->Config.bEnabled && Runtime->PendingFrame.IsSet())
		{
			FVirtualSensorFrameEnvelope Pending = MoveTemp(Runtime->PendingFrame.GetValue());
			Runtime->PendingFrame.Reset();
			StartPointCloudSerialization(StreamKey, *Runtime, Pending);
		}
		RefreshQueueTelemetry(*Runtime);
		return;
	}
	if (!Error.IsEmpty() || (Message.Json.IsEmpty() && !Message.BinaryBody.IsValid()))
	{
		++Runtime->Status.EncodeFailureCount;
		Runtime->Status.Message = Error.IsEmpty() ? TEXT("포인트 클라우드 직렬화 실패") : Error;
		AddLog(StreamKey, TEXT("encode-failed"), Runtime->Status.Message, nullptr, Message.FrameId);
	}
	else
	{
		if (bNoLoss)
		{
			if (Runtime->PreparedMessageQueue.Num() >= Runtime->Config.MaxBufferedFrames)
			{
				StopForPointCloudOverload(StreamKey, *Runtime, TEXT("Binary PCD prepared-body queue reached its limit."));
				return;
			}
			Runtime->PreparedMessageQueue.Add(MoveTemp(Message));
		}
		else
		{
			if (Runtime->PreparedMessage.IsSet()) ++Runtime->Status.ReplacedPendingFrameCount;
			Runtime->PreparedMessage = MoveTemp(Message);
		}
	}
	if (bNoLoss)
	{
		TryStartNextPointCloudSerialization(StreamKey, *Runtime);
	}
	else if (Runtime->Config.bEnabled && Runtime->PendingFrame.IsSet())
	{
		FVirtualSensorFrameEnvelope Pending = MoveTemp(Runtime->PendingFrame.GetValue());
		Runtime->PendingFrame.Reset();
		StartPointCloudSerialization(StreamKey, *Runtime, Pending);
	}
	RefreshQueueTelemetry(*Runtime);
}

void UVirtualSensorStreamPublisherComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	PumpPublisherOnce(FPlatformTime::Seconds());
}

void UVirtualSensorStreamPublisherComponent::PumpPublisherOnce(double Now)
{
	const double RateBytes = FMath::Max(1.0f, BandwidthLimitMegabytesPerSecond) * 1024.0 * 1024.0;
	const double PointCloudRateBytes = FMath::Max(1.0f, PointCloudBandwidthLimitMegabytesPerSecond) * 1024.0 * 1024.0;
	TokenBucketBytes = FMath::Min(RateBytes, TokenBucketBytes + (Now - LastTokenUpdateSeconds) * RateBytes);
	PointCloudTokenBucketBytes = FMath::Min(PointCloudRateBytes, PointCloudTokenBucketBytes + (Now - LastTokenUpdateSeconds) * PointCloudRateBytes);
	LastTokenUpdateSeconds = Now;
	PumpPreparedMessages(Now);
	CheckReceiptTimeouts(Now);
}

void UVirtualSensorStreamPublisherComponent::PumpPreparedMessages(double NowSeconds)
{
	if (!TransportComponent || StreamRuntimes.IsEmpty()) return;
	TArray<FString> Keys;
	StreamRuntimes.GetKeys(Keys);
	Keys.Sort();
	int32 SubmittedThisFrame = 0;
	for (int32 Attempt = 0; Attempt < Keys.Num() && SubmittedThisFrame < FMath::Max(1, MaxSubmissionsPerFrame); ++Attempt)
	{
		const int32 Index = (RoundRobinCursor + Attempt) % Keys.Num();
		FStreamRuntime* Runtime = StreamRuntimes.Find(Keys[Index]);
		if (!Runtime || !Runtime->Config.bEnabled || NowSeconds < Runtime->NextSubmitAttemptSeconds) continue;
		const bool bNoLoss = Runtime->Config.StreamKind == EVirtualSensorStreamKind::PointCloud &&
			Runtime->Config.DeliveryMode == EVirtualPointCloudDeliveryMode::ConnectedNoLoss;
		RefreshQueueTelemetry(*Runtime);
		if (bNoLoss && Runtime->Status.ReceiptQueueDepth >= Runtime->Config.MaxBufferedFrames) continue;
		FPreparedMessage* MessagePtr = bNoLoss
			? (Runtime->PreparedMessageQueue.IsEmpty() ? nullptr : &Runtime->PreparedMessageQueue[0])
			: (Runtime->PreparedMessage.IsSet() ? &Runtime->PreparedMessage.GetValue() : nullptr);
		if (!MessagePtr) continue;
		FPreparedMessage& Message = *MessagePtr;
		double& ActiveTokenBucket = Message.bBinaryPcd ? PointCloudTokenBucketBytes : TokenBucketBytes;
		if (Message.ByteCount > ActiveTokenBucket)
		{
			++Runtime->Status.BandwidthDeferredFrameCount;
			continue;
		}
		const bool bReceipt = Message.bBinaryPcd || ((Runtime->Status.SubmittedFrameCount + 1) % Runtime->Config.ReceiptSampleInterval) == 0;
		const FString SensorType = Message.StreamKind == EVirtualSensorStreamKind::CameraImage ? TEXT("camera") : TEXT("lidar");
		const FString DataKind = Message.StreamKind == EVirtualSensorStreamKind::PointCloud ? TEXT("pointcloud-stream")
			: Message.StreamKind == EVirtualSensorStreamKind::CameraImage ? TEXT("camera-stream") : TEXT("lidar-stream");
		const FVirtualSensorTransportResult Result = Message.bBinaryPcd
			? TransportComponent->SendStompBinaryStreamRequest(*Message.BinaryBody, Message.BinaryMetadata)
			: TransportComponent->SendJsonStreamRequest(Message.SensorId, SensorType, DataKind, Message.FrameId, Message.Json, bReceipt);
		Runtime->Status.LastRequestId = Result.RequestId;
		Runtime->Status.Destination = Result.Destination;
		Runtime->Status.Message = Result.Message;
		if (Result.bSubmitted)
		{
			Runtime->NextSubmitAttemptSeconds = 0.0;
			ActiveTokenBucket -= Message.ByteCount;
			if (Message.RetryAttempt == 0) ++Runtime->Status.SubmittedFrameCount;
			Runtime->Status.LastSubmittedFrameId = Message.FrameId;
			if (Message.RetryAttempt == 0) Runtime->Status.TotalSubmittedBytes += Message.ByteCount;
			Runtime->Status.LastFrameBytes = Message.ByteCount;
			if (Message.bBinaryPcd)
			{
				Runtime->Status.LastPointCount = Message.BinaryMetadata.PointCount;
				Runtime->Status.LastSourcePointCount = Message.BinaryMetadata.SourcePointCount;
			}
			if (Runtime->FirstSubmitSeconds <= 0.0) Runtime->FirstSubmitSeconds = NowSeconds;
			Runtime->Status.SubmittedHz = static_cast<float>(Runtime->Status.SubmittedFrameCount / FMath::Max(0.001, NowSeconds - Runtime->FirstSubmitSeconds));
			Runtime->Status.SubmittedMegabytesPerSecond = static_cast<float>(Runtime->Status.TotalSubmittedBytes /
				(FMath::Max(0.001, NowSeconds - Runtime->FirstSubmitSeconds) * 1024.0 * 1024.0));
			if (Runtime->Config.PointCloudFormat == EVirtualPointCloudStreamFormat::LAZ) Runtime->LastLazSubmitSeconds = NowSeconds;
			if (bReceipt && !Result.RequestId.IsEmpty()) WaitingReceipts.Add(Result.RequestId, {Keys[Index], NowSeconds, Message});
			if (!Result.RequestId.IsEmpty())
			{
				RequestToStreamKey.Add(Result.RequestId, Keys[Index]);
				if (RequestToStreamKey.Num() > 512)
				{
					for (auto It = RequestToStreamKey.CreateIterator(); It && RequestToStreamKey.Num() > 384; ++It) It.RemoveCurrent();
				}
			}
			AddLog(Keys[Index], bReceipt ? TEXT("submitted-receipt") : TEXT("submitted"), Result.Message, &Result, Message.FrameId);
			if (bNoLoss)
			{
				Runtime->PreparedMessageQueue.RemoveAt(0, 1, false);
				TryStartNextPointCloudSerialization(Keys[Index], *Runtime);
			}
			else
			{
				Runtime->PreparedMessage.Reset();
			}
			RefreshQueueTelemetry(*Runtime);
			++SubmittedThisFrame;
			RoundRobinCursor = (Index + 1) % Keys.Num();
		}
		else
		{
			// Do not retry a disconnected broker on every game frame. The latest
			// prepared message remains bounded to one item while acquisition continues.
			Runtime->NextSubmitAttemptSeconds = NowSeconds + 0.5;
			AddLog(Keys[Index], TEXT("submit-failed"), Result.Message, &Result, Message.FrameId);
			break;
		}
	}
}

void UVirtualSensorStreamPublisherComponent::CheckReceiptTimeouts(double NowSeconds)
{
	TArray<FString> TimedOut;
	for (const TPair<FString, FReceiptWait>& Pair : WaitingReceipts)
	{
		if (NowSeconds - Pair.Value.SubmittedSeconds >= ReceiptTimeoutSeconds) TimedOut.Add(Pair.Key);
	}
	for (const FString& RequestId : TimedOut)
	{
		const FReceiptWait Wait = WaitingReceipts.FindAndRemoveChecked(RequestId);
		RequestToStreamKey.Remove(RequestId);
		if (FStreamRuntime* Runtime = StreamRuntimes.Find(Wait.StreamKey))
		{
			++Runtime->Status.ReceiptTimeoutCount;
			Runtime->Status.Message = TEXT("Broker receipt 제한 시간 초과");
		}
		if (RequeueReceiptForRetry(Wait, TEXT("Broker receipt timeout"))) continue;
		++ConsecutiveReceiptTimeouts;
		AddLog(Wait.StreamKey, TEXT("receipt-timeout"), TEXT("Broker receipt가 5초 안에 도착하지 않았습니다."));
	}
	if (ConsecutiveReceiptTimeouts >= 3 && TransportComponent && NowSeconds - LastReconnectSeconds >= ReconnectCooldownSeconds)
	{
		LastReconnectSeconds = NowSeconds;
		ConsecutiveReceiptTimeouts = 0;
		TransportComponent->RequestStompReconnect();
		AddLog(FString(), TEXT("reconnect"), TEXT("연속 receipt timeout으로 STOMP 재연결을 시작했습니다."));
	}
}

bool UVirtualSensorStreamPublisherComponent::RequeueReceiptForRetry(const FReceiptWait& Wait, const FString& Error)
{
	FStreamRuntime* Runtime = StreamRuntimes.Find(Wait.StreamKey);
	if (!Runtime || !Runtime->Config.bEnabled ||
		Runtime->Config.DeliveryMode != EVirtualPointCloudDeliveryMode::ConnectedNoLoss ||
		!Wait.Message.bBinaryPcd)
	{
		return false;
	}
	if (Wait.Message.RetryAttempt >= Runtime->Config.MaxReceiptRetries)
	{
		StopForPointCloudOverload(Wait.StreamKey, *Runtime,
			FString::Printf(TEXT("Binary PCD receipt retry limit exhausted: %s"), *Error));
		return true;
	}
	if (Runtime->PreparedMessageQueue.Num() >= Runtime->Config.MaxBufferedFrames)
	{
		StopForPointCloudOverload(Wait.StreamKey, *Runtime,
			FString::Printf(TEXT("Binary PCD retry queue is full: %s"), *Error));
		return true;
	}
	FPreparedMessage RetryMessage = Wait.Message;
	++RetryMessage.RetryAttempt;
	Runtime->PreparedMessageQueue.Insert(MoveTemp(RetryMessage), 0);
	++Runtime->Status.RetryCount;
	Runtime->NextSubmitAttemptSeconds = FPlatformTime::Seconds() + 0.05;
	Runtime->Status.Message = FString::Printf(TEXT("Binary PCD receipt retry %d/%d: %s"),
		Wait.Message.RetryAttempt + 1,
		Runtime->Config.MaxReceiptRetries,
		*Error);
	RefreshQueueTelemetry(*Runtime);
	AddLog(Wait.StreamKey, TEXT("receipt-retry-queued"), Runtime->Status.Message, nullptr, Wait.Message.FrameId);
	return true;
}

void UVirtualSensorStreamPublisherComponent::OnTransportResult(const FVirtualSensorTransportResult& Result)
{
	HandleTransportResult(Result);
}

void UVirtualSensorStreamPublisherComponent::HandleTransportResult(const FVirtualSensorTransportResult& Result)
{
	if (Result.RequestId.IsEmpty()) return;
	const FString* KnownStreamKey = RequestToStreamKey.Find(Result.RequestId);
	if (Result.bConsumerAckReceived)
	{
		if (KnownStreamKey) AddLog(*KnownStreamKey, TEXT("consumer-ack"), Result.Message, &Result);
		RequestToStreamKey.Remove(Result.RequestId);
		return;
	}
	FReceiptWait Wait;
	if (!WaitingReceipts.RemoveAndCopyValue(Result.RequestId, Wait))
	{
		if (KnownStreamKey && Result.bReceiptReceived)
		{
			if (FStreamRuntime* Runtime = StreamRuntimes.Find(*KnownStreamKey)) ++Runtime->Status.DuplicateReceiptCount;
		}
		if (KnownStreamKey && !Result.bSubmitted) AddLog(*KnownStreamKey, TEXT("transport-error"), Result.Message, &Result);
		return;
	}
	if (!Result.bAccepted)
	{
		RequestToStreamKey.Remove(Result.RequestId);
		AddLog(Wait.StreamKey, TEXT("receipt-failed"), Result.Message, &Result, Wait.Message.FrameId);
		if (RequeueReceiptForRetry(Wait, Result.Message)) return;
	}
	if (FStreamRuntime* Runtime = StreamRuntimes.Find(Wait.StreamKey))
	{
		Runtime->Status.LastReceiptLatencyMs = Result.LatencyMs;
		Runtime->Status.Message = Result.Message;
		if (Result.bReceiptReceived) ++Runtime->Status.ReceiptReceivedCount;
		RefreshQueueTelemetry(*Runtime);
	}
	ConsecutiveReceiptTimeouts = Result.bAccepted ? 0 : ConsecutiveReceiptTimeouts + 1;
	AddLog(Wait.StreamKey, Result.bAccepted ? TEXT("receipt") : TEXT("receipt-failed"), Result.Message, &Result);
	if (TransportComponent && TransportComponent->GetTransportProfile().AckTopic.IsEmpty()) RequestToStreamKey.Remove(Result.RequestId);
}

void UVirtualSensorStreamPublisherComponent::AddLog(const FString& StreamKey, const FString& State, const FString& Message, const FVirtualSensorTransportResult* Result, int64 FrameId)
{
	FVirtualSensorTransportLogEntry Entry;
	Entry.TimestampUtc = FDateTime::UtcNow();
	Entry.State = State;
	Entry.Message = Message;
	Entry.FrameId = FrameId;
	if (const FStreamRuntime* Runtime = StreamRuntimes.Find(StreamKey))
	{
		Entry.SensorId = Runtime->Status.SensorId;
		Entry.StreamKind = Runtime->Status.StreamKind;
	}
	if (Result)
	{
		Entry.SensorId = Result->SensorId;
		Entry.RequestId = Result->RequestId;
		Entry.Destination = Result->Destination;
		Entry.Protocol = Result->Protocol;
		Entry.Bytes = Result->DataLength;
		Entry.LatencyMs = Result->LatencyMs;
	}
	RecentLogEntries.Insert(MoveTemp(Entry), 0);
	if (RecentLogEntries.Num() > 200) RecentLogEntries.SetNum(200);
}

TArray<FVirtualSensorStreamStatus> UVirtualSensorStreamPublisherComponent::GetStreamStatuses() const
{
	TArray<FVirtualSensorStreamStatus> Result;
	for (const TPair<FString, FStreamRuntime>& Pair : StreamRuntimes) Result.Add(Pair.Value.Status);
	Result.Sort([](const FVirtualSensorStreamStatus& A, const FVirtualSensorStreamStatus& B)
	{
		if (A.StreamKind != B.StreamKind) return static_cast<uint8>(A.StreamKind) < static_cast<uint8>(B.StreamKind);
		return A.SensorId < B.SensorId;
	});
	return Result;
}

bool UVirtualSensorStreamPublisherComponent::ExportDiagnosticReport(FString& OutJsonPath, FString& OutMarkdownPath) const
{
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Reports"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
	OutJsonPath = FPaths::Combine(Directory, FString::Printf(TEXT("sensor_stream_%s.json"), *Timestamp));
	OutMarkdownPath = FPaths::Combine(Directory, FString::Printf(TEXT("sensor_stream_%s.md"), *Timestamp));
	TArray<TSharedPtr<FJsonValue>> StatusValues;
	FString Markdown = TEXT("# Sensor Stream Diagnostic\n\n| Sensor | Stream | Input | Sent | Replaced | Deferred | Encode fail | Receipt timeout | Message |\n|---|---:|---:|---:|---:|---:|---:|---:|---|\n");
	for (const FVirtualSensorStreamStatus& Status : GetStreamStatuses())
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("sensorId"), Status.SensorId);
		Object->SetNumberField(TEXT("streamKind"), static_cast<int32>(Status.StreamKind));
		Object->SetBoolField(TEXT("enabled"), Status.bEnabled);
		Object->SetNumberField(TEXT("inputFrames"), static_cast<double>(Status.InputFrameCount));
		Object->SetNumberField(TEXT("submittedFrames"), static_cast<double>(Status.SubmittedFrameCount));
		Object->SetNumberField(TEXT("replacedPending"), static_cast<double>(Status.ReplacedPendingFrameCount));
		Object->SetNumberField(TEXT("bandwidthDeferred"), static_cast<double>(Status.BandwidthDeferredFrameCount));
		Object->SetNumberField(TEXT("encodeFailures"), static_cast<double>(Status.EncodeFailureCount));
		Object->SetNumberField(TEXT("receiptTimeouts"), static_cast<double>(Status.ReceiptTimeoutCount));
		Object->SetNumberField(TEXT("receipts"), static_cast<double>(Status.ReceiptReceivedCount));
		Object->SetStringField(TEXT("message"), Status.Message);
		StatusValues.Add(MakeShared<FJsonValueObject>(Object));
		Markdown += FString::Printf(TEXT("| %s | %d | %lld | %lld | %lld | %lld | %lld | %lld | %s |\n"), *Status.SensorId, static_cast<int32>(Status.StreamKind), Status.InputFrameCount, Status.SubmittedFrameCount, Status.ReplacedPendingFrameCount, Status.BandwidthDeferredFrameCount, Status.EncodeFailureCount, Status.ReceiptTimeoutCount, *Status.Message.Replace(TEXT("|"), TEXT("/")));
	}
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("generatedUtc"), FDateTime::UtcNow().ToIso8601());
	Root->SetNumberField(TEXT("bandwidthLimitMBps"), BandwidthLimitMegabytesPerSecond);
	Root->SetArrayField(TEXT("streams"), StatusValues);
	FString Json;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return FFileHelper::SaveStringToFile(Json, *OutJsonPath) && FFileHelper::SaveStringToFile(Markdown, *OutMarkdownPath);
}

void UVirtualSensorStreamPublisherComponent::UpdateCameraStreamDemand()
{
	if (AVirtualSensorCoordinator* Coordinator = Cast<AVirtualSensorCoordinator>(GetOwner()))
	{
		for (AVirtualSensorActorBase* SensorActor : Coordinator->GetSensorActors())
		{
			if (!SensorActor || SensorActor->GetSensorKind() != EVirtualSensorKind::Camera) continue;
			if (UVirtualCameraCaptureComponent* Camera = SensorActor->FindComponentByClass<UVirtualCameraCaptureComponent>())
			{
				const FStreamRuntime* Global = StreamRuntimes.Find(MakeStreamKey(EVirtualSensorStreamKind::CameraImage, FString()));
				const FStreamRuntime* Exact = StreamRuntimes.Find(MakeStreamKey(EVirtualSensorStreamKind::CameraImage, SensorActor->GetSensorId()));
				Camera->SetRuntimeStreamOutputDemand((Global && Global->Config.bEnabled) || (Exact && Exact->Config.bEnabled));
			}
		}
	}
}

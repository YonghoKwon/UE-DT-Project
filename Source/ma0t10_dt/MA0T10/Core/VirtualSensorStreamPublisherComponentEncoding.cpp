#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Contracts/VirtualSensorDefaults.h"
#include "VirtualPointCloudCoordinates.h"
#include "VirtualSensorWireHeaders.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"

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
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorOutputComponent.h"
#include "EngineUtils.h"
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
	const FVirtualSensorFrameEnvelope& Frame,
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
	// Per-frame metadata stays with a saved PCD without changing its point layout.
	const auto Metadata = MakeShared<FJsonObject>();
	Metadata->SetStringField(TEXT("schema"), TEXT("virtual-pointcloud.context.v1"));
	Metadata->SetStringField(TEXT("sensor_id"), Frame.SensorId);
	Metadata->SetStringField(TEXT("sensor_frame_id"), LexToString(Frame.FrameId));
	Metadata->SetStringField(TEXT("timestamp_utc"), Frame.TimestampUtc.ToIso8601());
	// External frames without an acquisition snapshot must not claim an identity pose.
	if (Frame.LidarFrameSnapshot.IsValid())
	{
		VirtualPointCloudCoordinates::AddMetadata(*Metadata, Frame.LidarFrameSnapshot->AcquisitionTransform);
	}
	if (Frame.SlabContext.bEligible)
	{
		Metadata->SetStringField(TEXT("run_uuid"), Frame.SlabContext.RunId);
		if(!Frame.SlabContext.ScenarioUUID.IsEmpty()) Metadata->SetStringField(TEXT("scenario_uuid"),Frame.SlabContext.ScenarioUUID);
		Metadata->SetStringField(TEXT("mtl_no"), Frame.SlabContext.MtlNo);
		Metadata->SetStringField(TEXT("frame_no"), LexToString(Frame.SlabContext.SlabFrameNo));
		Metadata->SetStringField(TEXT("elapsed_sec"), FString::Printf(TEXT("%.6f"), Frame.SlabContext.ElapsedSec));
		Metadata->SetStringField(TEXT("session_segment"), LexToString(Frame.SlabContext.Segment));
	}
	FString MetadataJson;
	const auto MetadataWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&MetadataJson);
	FJsonSerializer::Serialize(Metadata, MetadataWriter);

	const FString Header = FString::Printf(
		TEXT("# .PCD v0.7\n# MA0T10_META %s\nVERSION 0.7\nFIELDS x y z intensity ring horizontal_index return_index return_count time_offset_ns validity confidence\n")
		TEXT("SIZE 4 4 4 2 2 2 1 1 8 1 4\nTYPE F F F U U U U U I U F\nCOUNT 1 1 1 1 1 1 1 1 1 1 1\n")
		TEXT("WIDTH %d\nHEIGHT 1\nVIEWPOINT 0 0 0 1 0 0 0\nPOINTS %d\nDATA binary\n"),
		*MetadataJson, OutPointCount,
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
			return SerializeBinaryPcd(Points, Frame, Config.PointCloudFilter, OutBytes, OutPointCount);
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
		const FString TempDir = FPaths::Combine(FPaths::ProjectSavedDir(), VirtualSensorDefaults::StreamTempDirectory);
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

FString BuildLidarTelemetryJson(const FVirtualSensorFrameEnvelope& Frame)
{
	if (!Frame.LidarFrameSnapshot.IsValid()) return FString();
	const FVirtualLidarFrameSnapshot& Snapshot = *Frame.LidarFrameSnapshot;
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schema"), TEXT("virtual-lidar.telemetry.v1"));
	Root->SetStringField(TEXT("sensorId"), Frame.SensorId);
	Root->SetNumberField(TEXT("frameId"), static_cast<double>(Frame.FrameId));
	Root->SetStringField(TEXT("timestampUtc"), Frame.TimestampUtc.ToIso8601());
	Root->SetStringField(TEXT("profile"), Snapshot.ProfileKey);
	Root->SetNumberField(TEXT("acquisitionBackend"), static_cast<int32>(Snapshot.AcquisitionBackend));
	Root->SetNumberField(TEXT("horizontalSamples"), Snapshot.HorizontalSamples);
	Root->SetNumberField(TEXT("verticalChannels"), Snapshot.VerticalChannels);
	Root->SetNumberField(TEXT("requestedRayCount"), Snapshot.RequestedRayCount);
	Root->SetNumberField(TEXT("validPointCount"), Snapshot.ValidPointCount);
	Root->SetNumberField(TEXT("invalidPointCount"), Snapshot.InvalidPointCount);
	Root->SetNumberField(TEXT("firstEchoCount"), Snapshot.FirstEchoCount);
	Root->SetNumberField(TEXT("secondEchoCount"), Snapshot.SecondEchoCount);
	Root->SetNumberField(TEXT("minRangeMeters"), Snapshot.MinRangeMeters);
	Root->SetNumberField(TEXT("maxRangeMeters"), Snapshot.MaxRangeMeters);
	Root->SetNumberField(TEXT("meanRangeMeters"), Snapshot.MeanRangeMeters);
	Root->SetNumberField(TEXT("minIntensity"), Snapshot.MinIntensity);
	Root->SetNumberField(TEXT("maxIntensity"), Snapshot.MaxIntensity);
	Root->SetNumberField(TEXT("meanIntensity"), Snapshot.MeanIntensity);
	Root->SetNumberField(TEXT("acquisitionStartUnixNanoseconds"), static_cast<double>(Snapshot.AcquisitionStartUnixNanoseconds));
	Root->SetNumberField(TEXT("acquisitionEndUnixNanoseconds"), static_cast<double>(Snapshot.AcquisitionEndUnixNanoseconds));
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	return Json;
}
}


bool UVirtualSensorStreamPublisherComponent::SerializePointCloudFrame(
	const FVirtualSensorFrameEnvelope& Frame,
	const FVirtualSensorStreamConfig& Config,
	FString& OutExtension,
	TArray<uint8>& OutBytes,
	int32& OutPointCount,
	FString& OutError)
{
	return SerializePointCloud(Frame, Config, OutExtension, OutBytes, OutPointCount, OutError);
}

#if WITH_DEV_AUTOMATION_TESTS
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
#endif


void UVirtualSensorStreamPublisherComponent::QueueFrameForRuntime(const FString& StreamKey, FStreamRuntime& Runtime, const FVirtualSensorFrameEnvelope& Frame)
{
	const bool bHighThroughputBinary = Runtime.Config.TransportBackend == EVirtualSensorStreamTransportBackend::TcpStompHighThroughput &&
		CanUseHighThroughputTransport() &&
		((Runtime.Config.StreamKind == EVirtualSensorStreamKind::CameraImage && Frame.BinaryPayload.IsValid()) ||
		 (Runtime.Config.StreamKind == EVirtualSensorStreamKind::LidarPayload && Frame.LidarFrameSnapshot.IsValid()));
	// Preview/acquisition envelopes can arrive before asynchronous JSON
	// derivation. They are not failed stream frames and should not affect
	// no-loss input or encode-failure telemetry.
	if (Runtime.Config.StreamKind != EVirtualSensorStreamKind::PointCloud && !Frame.HasJsonPayload() && !bHighThroughputBinary)
	{
		return;
	}
	const FString SegmentKey=Frame.SlabContext.RunId+TEXT("|")+LexToString(Frame.SlabContext.Segment);
	auto& Sequence=Runtime.SensorSequences.FindOrAdd(Frame.SensorId);
	if(Sequence.Segment!=SegmentKey){Sequence=FStreamRuntime::FSensorSequence();Sequence.Segment=SegmentKey;}
	if (Sequence.bHave && Sequence.Frame == Frame.FrameId)
	{
		return;
	}
	Runtime.LastInputSlabContext=Frame.SlabContext;
	if (Sequence.bHave && Frame.FrameId > Sequence.Frame + 1)
	{
		Runtime.Status.FrameGapCount += Frame.FrameId - Sequence.Frame - 1;
	}
	Sequence.Frame=Frame.FrameId;Sequence.bHave=true;Runtime.LastSlabSegment=SegmentKey;
	++Runtime.Status.InputFrameCount;
	Runtime.Status.LastInputFrameId = Frame.FrameId;
	Runtime.bHasInputFrameInSegment=true;
	const double Now = FPlatformTime::Seconds();
	if (Runtime.FirstInputSeconds <= 0.0) Runtime.FirstInputSeconds = Now;
	Runtime.Status.InputHz = static_cast<float>(Runtime.Status.InputFrameCount / FMath::Max(0.001, Now - Runtime.FirstInputSeconds));
	if (((Runtime.Status.InputFrameCount - 1) % Runtime.Config.FrameStride) != 0) return;

	if (Runtime.Config.StreamKind == EVirtualSensorStreamKind::PointCloud)
	{
		if (TransportComponent && Frame.PointSnapshot.IsValid())
		{
			const int64 Estimate = Frame.PointSnapshot->Num() * 33LL + 1024;
			FString SizeError;
			if (!ValidateBinaryBodySize(Estimate, TransportComponent->GetTransportProfile().MaxMessageBytes, SizeError))
			{
				StopForBodyLimit(StreamKey, Runtime, TEXT("[시작 전 크기 검사] ") + SizeError, Frame.FrameId);
				return;
			}
		}
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
	if (bHighThroughputBinary)
	{
		FPreparedMessage Message;
		Message.SensorId = Frame.SensorId;
		Message.SlabContext=Frame.SlabContext;
		Message.StreamKind = Runtime.Config.StreamKind;
		Message.FrameId = Frame.FrameId;
		Message.TimestampUtc = Frame.TimestampUtc;
		Message.ConfigRevision = Runtime.ConfigRevision;
		Message.bHighThroughputBinary = true;
		Message.BinaryHeaders=Frame.SlabContext.ToHeaders();
		if (Runtime.Config.StreamKind == EVirtualSensorStreamKind::CameraImage)
		{
			Message.BinaryBody64 = Frame.BinaryPayload;
			Message.CameraMetadata = Frame.CameraJpegMetadata;
			Message.BinarySchema = TEXT("virtual-camera.jpeg.v1");
			Message.BinaryContentType = TEXT("image/jpeg");
			Message.ByteCount = static_cast<int32>(FMath::Min<int64>(MAX_int32, Frame.BinaryPayload->Num()));
			Message.BinaryHeaders.Add(TEXT("checksum"), Frame.CameraJpegMetadata.ChecksumSha1);
			Message.BinaryHeaders.Add(TEXT("width"), FString::FromInt(Frame.CameraJpegMetadata.Width));
			Message.BinaryHeaders.Add(TEXT("height"), FString::FromInt(Frame.CameraJpegMetadata.Height));
			Message.BinaryHeaders.Add(TEXT("jpeg-quality"), FString::FromInt(Frame.CameraJpegMetadata.JpegQuality));
		}
		else
		{
			const FString TelemetryJson = BuildLidarTelemetryJson(Frame);
			FTCHARToUTF8 Utf8(*TelemetryJson);
			TArray<uint8> Body;
			Body.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			uint8 Hash[FSHA1::DigestSize];
			FSHA1::HashBuffer(Body.GetData(), Body.Num(), Hash);
			Message.BinaryBody = MakeShared<const TArray<uint8>, ESPMode::ThreadSafe>(MoveTemp(Body));
			Message.BinarySchema = TEXT("virtual-lidar.telemetry.v1");
			Message.BinaryContentType = TEXT("application/json");
			Message.ByteCount = Message.BinaryBody->Num();
			Message.BinaryHeaders.Add(TEXT("checksum"), BytesToHex(Hash, FSHA1::DigestSize).ToLower());
			Message.BinaryHeaders.Add(TEXT("profile"), Frame.LidarFrameSnapshot->ProfileKey);
			Message.BinaryHeaders.Add(TEXT("valid-point-count"), FString::FromInt(Frame.LidarFrameSnapshot->ValidPointCount));
			Message.BinaryHeaders.Add(TEXT("invalid-point-count"), FString::FromInt(Frame.LidarFrameSnapshot->InvalidPointCount));
		}

		const int32 MaxQueue = Runtime.Config.StreamKind == EVirtualSensorStreamKind::CameraImage ? 8 : 20;
		if (Runtime.PreparedMessageQueue.Num() >= MaxQueue)
		{
			StopForPointCloudOverload(StreamKey, Runtime, TEXT("High-throughput stream prepared queue reached its limit."));
			return;
		}
		Runtime.PreparedMessageQueue.Add(MoveTemp(Message));
		Runtime.Status.bPendingLatestFrame = true;
		RefreshQueueTelemetry(Runtime);
		return;
	}
	if (!Frame.HasJsonPayload())
	{
		RecordSlabFailure(Frame.SlabContext.RunId,Frame.SensorId,Runtime.Config.StreamKind,Frame.FrameId);
		++Runtime.Status.EncodeFailureCount;
		Runtime.Status.Message = TEXT("완료된 JSON Payload가 없습니다.");
		return;
	}
	FPreparedMessage Message;
	Message.SensorId = Frame.SensorId;
	Message.SlabContext=Frame.SlabContext;
	Message.StreamKind = Runtime.Config.StreamKind;
	Message.FrameId = Frame.FrameId;
		Message.Json = *Frame.JsonPayload;
	Message.BinaryHeaders=Frame.SlabContext.ToHeaders();
		Message.ByteCount = FTCHARToUTF8(*Message.Json).Length();
		Message.ConfigRevision = Runtime.ConfigRevision;
	if (Runtime.PreparedMessage.IsSet()) ++Runtime.Status.ReplacedPendingFrameCount;
	Runtime.PreparedMessage = MoveTemp(Message);
	Runtime.Status.bPendingLatestFrame = true;
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
	Runtime.SerializationRunId=Frame.SlabContext.RunId;
	Runtime.SerializationSensorId=Frame.SensorId;
	Runtime.SerializationFrameId=Frame.FrameId;
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
			Message.SlabContext=Frame.SlabContext;
			Message.BinaryHeaders=Frame.SlabContext.ToHeaders();
			Message.FrameId = Frame.FrameId;
			Message.TimestampUtc = Frame.TimestampUtc;
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
				Message.BinaryMetadata.SlabContext=Frame.SlabContext;
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

#include "VirtualLidarPayloadCodec.h"

#include "Json.h"
#include "Serialization/BufferArchive.h"

namespace
{
template <typename T>
void WriteBinaryValue(FBufferArchive& Archive, const T& Value)
{
	Archive.Serialize(const_cast<T*>(&Value), sizeof(T));
}

void AddVectorMeters(TSharedRef<FJsonObject> Object, const TCHAR* FieldName, const FVector& Value)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	Values.Reserve(3);
	Values.Add(MakeShared<FJsonValueNumber>(Value.X));
	Values.Add(MakeShared<FJsonValueNumber>(Value.Y));
	Values.Add(MakeShared<FJsonValueNumber>(Value.Z));
	Object->SetArrayField(FieldName, Values);
}

FString FidelityModeName(EVirtualSensorFidelityMode Mode)
{
	switch (Mode)
	{
	case EVirtualSensorFidelityMode::IdealTruth: return TEXT("ideal_truth");
	case EVirtualSensorFidelityMode::PublicSpecBased: return TEXT("public_spec_based");
	case EVirtualSensorFidelityMode::HardwareCalibrated: return TEXT("hardware_calibrated");
	case EVirtualSensorFidelityMode::ReplayOrHardwareInput: return TEXT("replay_or_hardware_input");
	default: return TEXT("unknown");
	}
}

FString BackendName(EVirtualLidarAcquisitionBackend Backend)
{
	switch (Backend)
	{
	case EVirtualLidarAcquisitionBackend::AccurateCpuTrace: return TEXT("accurate_cpu_trace");
	case EVirtualLidarAcquisitionBackend::GpuDepthProjection: return TEXT("gpu_depth_projection");
	case EVirtualLidarAcquisitionBackend::HardwareRayTracing: return TEXT("hardware_ray_tracing");
	case EVirtualLidarAcquisitionBackend::ReplayOrExternal: return TEXT("replay_or_external");
	case EVirtualLidarAcquisitionBackend::Auto: return TEXT("auto");
	default: return TEXT("unknown");
	}
}

FString ValidityName(EVirtualLidarPointValidity Validity)
{
	switch (Validity)
	{
	case EVirtualLidarPointValidity::Valid: return TEXT("valid");
	case EVirtualLidarPointValidity::NoReturn: return TEXT("no_return");
	case EVirtualLidarPointValidity::BelowSignalThreshold: return TEXT("below_signal_threshold");
	case EVirtualLidarPointValidity::Saturated: return TEXT("saturated");
	case EVirtualLidarPointValidity::OutOfRange: return TEXT("out_of_range");
	case EVirtualLidarPointValidity::InvalidCalibration: return TEXT("invalid_calibration");
	default: return TEXT("unknown");
	}
}

FString EchoTypeName(EVirtualLidarEchoType EchoType)
{
	switch (EchoType)
	{
	case EVirtualLidarEchoType::Single: return TEXT("single");
	case EVirtualLidarEchoType::First: return TEXT("first");
	case EVirtualLidarEchoType::Strongest: return TEXT("strongest");
	case EVirtualLidarEchoType::Last: return TEXT("last");
	default: return TEXT("none");
	}
}
}

FString FVirtualLidarPayloadCodec::EncodeV2Json(
	const FVirtualLidarFrameSnapshot& Frame,
	const FVirtualLidarPayloadDescriptor& Descriptor,
	const FVirtualLidarV2EncodeOptions& Options)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("schemaVersion"), TEXT("virtual-lidar.v2"));
	Root->SetStringField(TEXT("wireCompatibility"), TEXT("project_contract_not_vendor_packet"));
	Root->SetStringField(TEXT("sensorType"), TEXT("lidar"));
	Root->SetStringField(TEXT("sensorId"), Descriptor.SensorId);
	Root->SetStringField(TEXT("manufacturer"), Descriptor.Manufacturer);
	Root->SetStringField(TEXT("model"), Descriptor.Model);
	Root->SetStringField(TEXT("profileKey"), Frame.ProfileKey);
	Root->SetStringField(TEXT("calibrationId"), Frame.CalibrationId);
	Root->SetStringField(TEXT("firmwareVersion"), Frame.FirmwareVersion);
	Root->SetStringField(TEXT("coordinateConvention"), Frame.CoordinateConvention);
	Root->SetNumberField(TEXT("frameId"), static_cast<double>(Frame.FrameId));
	Root->SetStringField(TEXT("acquisitionStartUnixNanoseconds"), LexToString(Frame.AcquisitionStartUnixNanoseconds));
	Root->SetStringField(TEXT("acquisitionEndUnixNanoseconds"), LexToString(Frame.AcquisitionEndUnixNanoseconds));
	Root->SetStringField(TEXT("fidelityMode"), FidelityModeName(Frame.FidelityMode));
	Root->SetStringField(TEXT("acquisitionBackend"), BackendName(Frame.AcquisitionBackend));
	Root->SetBoolField(TEXT("protocolVerifiedAgainstHardware"), Frame.bProtocolVerifiedAgainstHardware);
	Root->SetNumberField(TEXT("horizontalSamples"), Frame.HorizontalSamples);
	Root->SetNumberField(TEXT("verticalChannels"), Frame.VerticalChannels);
	Root->SetNumberField(TEXT("horizontalFovDegrees"), Descriptor.HorizontalFovDegrees);
	Root->SetNumberField(TEXT("verticalFovDegrees"), Descriptor.VerticalFovDegrees);
	Root->SetNumberField(TEXT("maxRangeMeters"), Descriptor.MaxRangeMeters);
	Root->SetNumberField(TEXT("requestedRayCount"), Frame.RequestedRayCount);
	Root->SetNumberField(TEXT("validPointCount"), Frame.ValidPointCount);
	Root->SetNumberField(TEXT("invalidPointCount"), Frame.InvalidPointCount);
	Root->SetNumberField(TEXT("firstEchoCount"), Frame.FirstEchoCount);
	Root->SetNumberField(TEXT("secondEchoCount"), Frame.SecondEchoCount);

	TArray<TSharedPtr<FJsonValue>> PhysicalPoints;
	TArray<TSharedPtr<FJsonValue>> DigitalTwinPoints;
	const int32 SafeStride = FMath::Max(1, Options.PointStride);
	const int32 SafeMaximum = FMath::Max(0, Options.MaxPoints);
	int32 EncodedPointCount = 0;
	if (Frame.Points.IsValid())
	{
		for (int32 PointIndex = 0; PointIndex < Frame.Points->Num(); PointIndex += SafeStride)
		{
			if (SafeMaximum > 0 && EncodedPointCount >= SafeMaximum) break;
			const FVirtualLidarPoint& Point = (*Frame.Points)[PointIndex];
			if (!Options.bIncludeInvalidPoints && !Point.bHit) continue;

			TSharedRef<FJsonObject> Physical = MakeShared<FJsonObject>();
			Physical->SetNumberField(TEXT("pointIndex"), PointIndex);
			AddVectorMeters(Physical, TEXT("positionMeters"), Point.SensorLocalPositionMeters);
			Physical->SetNumberField(TEXT("rangeMillimeters"), Point.RangeMillimeters);
			Physical->SetNumberField(TEXT("intensity"), Point.RawIntensity);
			Physical->SetNumberField(TEXT("intensityNormalized"), Point.NormalizedIntensity);
			Physical->SetNumberField(TEXT("ring"), Point.Ring);
			Physical->SetNumberField(TEXT("horizontalIndex"), Point.HorizontalIndex);
			Physical->SetNumberField(TEXT("echoIndex"), Point.EchoIndex);
			Physical->SetNumberField(TEXT("echoCount"), Point.EchoCount);
			Physical->SetStringField(TEXT("echoType"), EchoTypeName(Point.EchoType));
			Physical->SetStringField(TEXT("pointTimeOffsetNanoseconds"), LexToString(Point.PointTimeOffsetNanoseconds));
			Physical->SetStringField(TEXT("validity"), ValidityName(Point.Validity));
			Physical->SetNumberField(TEXT("confidence"), Point.Confidence);
			PhysicalPoints.Add(MakeShared<FJsonValueObject>(Physical));

			if (Options.bIncludeDigitalTwinExtensions)
			{
				TSharedRef<FJsonObject> Extension = MakeShared<FJsonObject>();
				Extension->SetNumberField(TEXT("pointIndex"), PointIndex);
				AddVectorMeters(Extension, TEXT("worldLocationCentimeters"), Point.WorldLocation);
				Extension->SetStringField(TEXT("semanticLabel"), Point.SemanticLabel.ToString());
				Extension->SetStringField(TEXT("actor"), Point.HitActorName.ToString());
				Extension->SetStringField(TEXT("actorClass"), Point.HitActorClassName.ToString());
				DigitalTwinPoints.Add(MakeShared<FJsonValueObject>(Extension));
			}
			++EncodedPointCount;
		}
	}
	Root->SetNumberField(TEXT("encodedPointCount"), EncodedPointCount);
	Root->SetArrayField(TEXT("points"), PhysicalPoints);
	if (Options.bIncludeDigitalTwinExtensions)
	{
		TSharedRef<FJsonObject> Extensions = MakeShared<FJsonObject>();
		Extensions->SetStringField(TEXT("coordinateUnit"), TEXT("centimeters"));
		Extensions->SetArrayField(TEXT("points"), DigitalTwinPoints);
		Root->SetObjectField(TEXT("digitalTwinExtensions"), Extensions);
	}

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	return Json;
}

bool FVirtualLidarPayloadCodec::EncodeCompactBinary(
	const FVirtualLidarFrameSnapshot& Frame,
	const FVirtualLidarV2EncodeOptions& Options,
	TArray64<uint8>& OutBytes,
	int32& OutEncodedPointCount)
{
	OutBytes.Reset();
	OutEncodedPointCount = 0;
	if (!Frame.Points.IsValid())
	{
		return false;
	}

	TArray<const FVirtualLidarPoint*> SelectedPoints;
	const int32 SafeStride = FMath::Max(1, Options.PointStride);
	const int32 SafeMaximum = FMath::Max(0, Options.MaxPoints);
	for (int32 PointIndex = 0; PointIndex < Frame.Points->Num(); PointIndex += SafeStride)
	{
		if (SafeMaximum > 0 && SelectedPoints.Num() >= SafeMaximum) break;
		const FVirtualLidarPoint& Point = (*Frame.Points)[PointIndex];
		if (!Options.bIncludeInvalidPoints && !Point.bHit) continue;
		SelectedPoints.Add(&Point);
	}

	FBufferArchive Archive;
	const uint8 Magic[8] = {'V', 'L', 'D', 'R', '2', 0, 0, 0};
	Archive.Serialize(const_cast<uint8*>(Magic), UE_ARRAY_COUNT(Magic));
	WriteBinaryValue<uint16>(Archive, 1);
	WriteBinaryValue<uint16>(Archive, 36);
	WriteBinaryValue<int64>(Archive, Frame.FrameId);
	WriteBinaryValue<int64>(Archive, Frame.AcquisitionStartUnixNanoseconds);
	WriteBinaryValue<uint32>(Archive, static_cast<uint32>(SelectedPoints.Num()));
	WriteBinaryValue<uint16>(Archive, static_cast<uint16>(FMath::Clamp(Frame.HorizontalSamples, 0, 65535)));
	WriteBinaryValue<uint16>(Archive, static_cast<uint16>(FMath::Clamp(Frame.VerticalChannels, 0, 65535)));

	for (const FVirtualLidarPoint* Point : SelectedPoints)
	{
		const float X = static_cast<float>(Point->SensorLocalPositionMeters.X);
		const float Y = static_cast<float>(Point->SensorLocalPositionMeters.Y);
		const float Z = static_cast<float>(Point->SensorLocalPositionMeters.Z);
		WriteBinaryValue<float>(Archive, X);
		WriteBinaryValue<float>(Archive, Y);
		WriteBinaryValue<float>(Archive, Z);
		WriteBinaryValue<uint32>(Archive, static_cast<uint32>(FMath::Max(0, Point->RangeMillimeters)));
		WriteBinaryValue<uint16>(Archive, static_cast<uint16>(FMath::Clamp(Point->RawIntensity, 0, 65535)));
		WriteBinaryValue<uint16>(Archive, static_cast<uint16>(FMath::Clamp(Point->Ring, 0, 65535)));
		WriteBinaryValue<uint16>(Archive, static_cast<uint16>(FMath::Clamp(Point->HorizontalIndex, 0, 65535)));
		WriteBinaryValue<uint8>(Archive, static_cast<uint8>(FMath::Clamp(Point->EchoIndex, 0, 255)));
		WriteBinaryValue<uint8>(Archive, static_cast<uint8>(FMath::Clamp(Point->EchoCount, 0, 255)));
		WriteBinaryValue<uint8>(Archive, static_cast<uint8>(Point->Validity));
		WriteBinaryValue<uint8>(Archive, static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Point->Confidence * 255.0f), 0, 255)));
		WriteBinaryValue<int32>(Archive, static_cast<int32>(FMath::Clamp<int64>(Point->PointTimeOffsetNanoseconds / 1000, MIN_int32, MAX_int32)));
	}

	OutEncodedPointCount = SelectedPoints.Num();
	OutBytes.Append(Archive.GetData(), Archive.Num());
	return true;
}

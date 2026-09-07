#include "VirtualSensorWireHeaders.h"
#include "VirtualSensorHighThroughputTransportSubsystem.h"

TMap<FString,FString> FVirtualSensorWireHeaders::RawPcd(const FVirtualPointCloudBinaryMetadata& M)
{
	auto H = M.SlabContext.ToHeaders();
	H.Add(TEXT("checksum"), M.ChecksumSha1);
	H.Add(TEXT("point-count"), LexToString(M.PointCount));
	H.Add(TEXT("x-point-count"), LexToString(M.PointCount));
	H.Add(TEXT("source-point-count"), LexToString(M.SourcePointCount));
	H.Add(TEXT("x-source-point-count"), LexToString(M.SourcePointCount));
	H.Add(TEXT("filter-revision"), LexToString(M.FilterRevision));
	H.Add(TEXT("x-filter-revision"), LexToString(M.FilterRevision));
	H.Add(TEXT("acquisition-profile"), M.ProfileKey);
	return H;
}

TMap<FString,FString> FVirtualSensorWireHeaders::RawFrame(const FVirtualSensorBinaryFrame& F)
{
	auto H = F.Headers;
	const FString Id = F.RequestId.IsEmpty() ? FString::Printf(TEXT("%s-%lld-%s"), *F.SensorId,F.FrameId,*H.FindRef(TEXT("checksum"))) : F.RequestId;
	H.Add(TEXT("destination"), F.Destination); H.Add(TEXT("content-type"), F.ContentType);
	H.Add(TEXT("content-length"), LexToString(F.NumBytes())); H.Add(TEXT("receipt"), Id);
	H.Add(TEXT("persistent"), TEXT("true")); H.Add(TEXT("destination-type"), TEXT("MULTICAST"));
	H.Add(TEXT("schema"),F.Schema); H.Add(TEXT("sensor-id"),F.SensorId); H.Add(TEXT("x-sensor-id"),F.SensorId);
	H.Add(TEXT("frame-id"),LexToString(F.FrameId)); H.Add(TEXT("x-frame-id"),LexToString(F.FrameId));
	H.Add(TEXT("timestamp-utc"),F.TimestampUtc.ToIso8601()); H.Add(TEXT("request-id"),Id); H.Add(TEXT("x-request-id"),Id);
	H.Add(TEXT("x-data-kind"),F.StreamKind==EVirtualSensorStreamKind::PointCloud ? TEXT("pointcloud-stream") : F.StreamKind==EVirtualSensorStreamKind::CameraImage ? TEXT("camera-stream") : TEXT("lidar-stream"));
	H.Add(TEXT("x-sensor-type"),F.StreamKind==EVirtualSensorStreamKind::CameraImage ? TEXT("camera") : TEXT("lidar"));
	return H;
}

TMap<FName,FString> FVirtualSensorWireHeaders::EnginePcd(const FVirtualPointCloudBinaryMetadata& M,const FString& Id)
{
	TMap<FName,FString> H;
	H.Add(TEXT("destination-type"),TEXT("MULTICAST")); H.Add(TEXT("content-type"),TEXT("application/vnd.pcd"));
	H.Add(TEXT("persistent"),TEXT("true")); H.Add(TEXT("schema"),M.Schema); H.Add(TEXT("x-sensor-id"),M.SensorId);
	H.Add(TEXT("x-sensor-type"),TEXT("lidar")); H.Add(TEXT("x-data-kind"),TEXT("pointcloud-stream"));
	H.Add(TEXT("x-frame-id"),LexToString(M.FrameId)); H.Add(TEXT("x-request-id"),Id);
	FDateTime Utc; const int64 Ms=FDateTime::ParseIso8601(*M.TimestampUtc,Utc) ? Utc.ToUnixTimestamp()*1000LL+Utc.GetMillisecond() : 0;
	H.Add(TEXT("x-utc"),LexToString(Ms)); H.Add(TEXT("x-point-count"),LexToString(M.PointCount));
	H.Add(TEXT("x-source-point-count"),LexToString(M.SourcePointCount)); H.Add(TEXT("x-filter-revision"),LexToString(M.FilterRevision));
	H.Add(TEXT("x-acquisition-profile"),M.ProfileKey); H.Add(TEXT("x-checksum-sha1"),M.ChecksumSha1);
	for (const auto& P : M.SlabContext.ToHeaders()) H.Add(FName(*P.Key),P.Value);
	return H;
}

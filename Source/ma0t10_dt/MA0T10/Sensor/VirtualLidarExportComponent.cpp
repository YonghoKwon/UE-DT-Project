#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarExportComponent.h"

#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"

UVirtualLidarExportComponent::UVirtualLidarExportComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}
bool UVirtualLidarExportComponent::SerializeImmutableFrame(const FVirtualSensorFrameEnvelope& Frame,const FVirtualSensorStreamConfig& Config,FString& Extension,TArray<uint8>& Bytes,int32& PointCount,FString& Error)
{
	const bool Success=UVirtualSensorStreamPublisherComponent::SerializePointCloudForTesting(Frame,Config,Extension,Bytes,PointCount,Error);
	if(!Success&&Error.IsEmpty())Error=TEXT("선택 형식으로 저장할 검출점이 없거나 프레임 변환에 실패했습니다.");
	return Success;
}

void UVirtualLidarExportComponent::BindScanComponent(UVirtualLidarScanComponent* InScanComponent)
{
	ScanComponent = InScanComponent;
}

bool UVirtualLidarExportComponent::ExportCsv(const FString& FileNamePrefix) const
{
	return ScanComponent && ScanComponent->ExportLastPointCloudCsv(FileNamePrefix);
}

bool UVirtualLidarExportComponent::ExportJsonLines(const FString& FileNamePrefix) const
{
	return ScanComponent && ScanComponent->ExportLastPointCloudJsonLines(FileNamePrefix);
}

bool UVirtualLidarExportComponent::ExportPcd(const FString& FileNamePrefix) const
{
	return ScanComponent && ScanComponent->ExportLastPointCloudPcd(FileNamePrefix);
}

bool UVirtualLidarExportComponent::ExportLas(const FString& FileNamePrefix) const
{
	return ScanComponent && ScanComponent->ExportLastPointCloudLas(FileNamePrefix);
}

bool UVirtualLidarExportComponent::ExportLaz(const FString& FileNamePrefix) const
{
	return ScanComponent && ScanComponent->ExportLastPointCloudLaz(FileNamePrefix);
}

FString UVirtualLidarExportComponent::GetLastExportPath() const
{
	return ScanComponent ? ScanComponent->GetLastPointCloudExportPath() : FString();
}

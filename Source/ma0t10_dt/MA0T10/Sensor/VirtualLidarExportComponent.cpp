#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarExportComponent.h"

#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"

UVirtualLidarExportComponent::UVirtualLidarExportComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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

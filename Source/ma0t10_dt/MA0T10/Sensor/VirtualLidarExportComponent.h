#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRuntimeTypes.h"
#include "VirtualLidarExportComponent.generated.h"

class UVirtualLidarScanComponent;

UCLASS(ClassGroup = (DigitalTwin), meta = (BlueprintSpawnableComponent))
class MA0T10_DT_API UVirtualLidarExportComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVirtualLidarExportComponent();
	void BindScanComponent(UVirtualLidarScanComponent* InScanComponent);

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
	bool ExportCsv(const FString& FileNamePrefix = TEXT("")) const;

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
	bool ExportJsonLines(const FString& FileNamePrefix = TEXT("")) const;

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
	bool ExportPcd(const FString& FileNamePrefix = TEXT("")) const;

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
	bool ExportLas(const FString& FileNamePrefix = TEXT("")) const;

	UFUNCTION(BlueprintCallable, Category = "DigitalTwin|VirtualLidar|Export")
	bool ExportLaz(const FString& FileNamePrefix = TEXT("")) const;

	UFUNCTION(BlueprintPure, Category = "DigitalTwin|VirtualLidar|Export")
	FString GetLastExportPath() const;
	/** Stateless worker-safe serializer; all inputs must be frozen before dispatch. */
	static bool SerializeImmutableFrame(const FVirtualSensorFrameEnvelope& Frame,const FVirtualSensorStreamConfig& Config,FString& Extension,TArray<uint8>& Bytes,int32& PointCount,FString& Error);

private:
	UPROPERTY(Transient)
	TObjectPtr<UVirtualLidarScanComponent> ScanComponent;
};

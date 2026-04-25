#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VirtualSensorTypes.h"
#include "VirtualLidarComp.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVirtualLidarPacketReady, const FVirtualSensorPacket&, Packet, const TArray<FVirtualLidarPoint>&, Points);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class M7AT10_DT_API UVirtualLidarComp : public USceneComponent
{
	GENERATED_BODY()

public:
	UVirtualLidarComp();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	UFUNCTION(BlueprintCallable, Category = "VirtualLidar")
	void ScanAndSend();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualLidar")
	float ScanInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualLidar")
	int32 HorizontalSamples = 180;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualLidar")
	int32 VerticalChannels = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualLidar")
	float HorizontalFov = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualLidar")
	float VerticalFov = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualLidar")
	float MaxDistanceCm = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualLidar")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualLidar")
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VirtualLidar")
	bool bPublishToHttpAutomatically = false;

	UPROPERTY(BlueprintAssignable, Category = "VirtualLidar")
	FOnVirtualLidarPacketReady OnLidarPacketReady;

private:
	FVirtualSensorPacket BuildLidarPacket(const TArray<FVirtualLidarPoint>& Points) const;

private:
	FTimerHandle ScanTimerHandle;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SensorDataRelayComp.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnVirtualSensorPayload, const FString&, SensorId, const FString&, SensorType, const FString&, PayloadJson);

/**
 * 센서별 페이로드를 외부 시스템으로 넘기기 전 한 곳에서 모아주는 릴레이 컴포넌트.
 * - 현재는 브로드캐스트 + 로그 기반으로 동작
 * - 추후 WebSocket/HTTP 전송 모듈을 이 컴포넌트에 결합하여 확장 가능
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class M7AT10_DT_API USensorDataRelayComp : public UActorComponent
{
	GENERATED_BODY()

public:
	USensorDataRelayComp();

	UFUNCTION(BlueprintCallable, Category="VirtualSensor|Relay")
	void PublishPayload(const FString& SensorId, const FString& SensorType, const FString& PayloadJson);

	UFUNCTION(BlueprintCallable, Category="VirtualSensor|Relay")
	bool GetLastPayload(const FString& SensorId, FString& OutPayloadJson) const;

	UPROPERTY(BlueprintAssignable, Category="VirtualSensor|Relay")
	FOnVirtualSensorPayload OnPayloadReady;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VirtualSensor|Relay")
	bool bEnableLogEcho = true;

private:
	UPROPERTY()
	TMap<FString, FString> LastPayloadBySensorId;
};

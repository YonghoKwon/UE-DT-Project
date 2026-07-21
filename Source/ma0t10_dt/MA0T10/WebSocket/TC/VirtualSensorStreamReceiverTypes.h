#pragma once

#include "CoreMinimal.h"
#include "WebSocket/FTransactionCodeDataBase.h"
#include "VirtualSensorStreamReceiverTypes.generated.h"

UENUM(BlueprintType)
enum class EVirtualSensorTopicReceiveKind : uint8
{
	Lidar UMETA(DisplayName = "LiDAR 값"),
	Camera UMETA(DisplayName = "Camera 이미지"),
	PointCloud UMETA(DisplayName = "Point Cloud")
};

UENUM(BlueprintType)
enum class EVirtualSensorTopicReceiverState : uint8
{
	Stopped UMETA(DisplayName = "중지"),
	WaitingForConnection UMETA(DisplayName = "연결 대기"),
	Subscribing UMETA(DisplayName = "구독 중"),
	Active UMETA(DisplayName = "수신 중"),
	Error UMETA(DisplayName = "오류")
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorTopicReceiverStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	EVirtualSensorTopicReceiveKind Kind = EVirtualSensorTopicReceiveKind::Lidar;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	EVirtualSensorTopicReceiverState State = EVirtualSensorTopicReceiverState::Stopped;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	FString Topic;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	int64 ReceivedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	int64 ValidatedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	int64 ValidationFailureCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	int64 ReplacedPendingCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	int64 DeepValidationCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	FString LastSensorId;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	int64 LastFrameId = -1;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	int32 LastMessageBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	float LastParseLatencyMs = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	FDateTime LastReceivedUtc;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	FString LastMessage;
};

USTRUCT(BlueprintType)
struct MA0T10_DT_API FVirtualSensorTopicReceiveLogEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	FDateTime TimestampUtc;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	EVirtualSensorTopicReceiveKind Kind = EVirtualSensorTopicReceiveKind::Lidar;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	FString Topic;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	FString SensorId;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	int64 FrameId = -1;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	int32 MessageBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	float ParseLatencyMs = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	bool bDeepValidated = false;

	UPROPERTY(BlueprintReadOnly, Category = "DigitalTwin|SensorReceiver")
	FString Message;
};

/** 순수 파싱 스레드에서 생성되고 게임 스레드에서 소비되는 공통 결과입니다. */
struct FVirtualSensorTopicReceivedDataBase : FTransactionCodeDataBase
{
	EVirtualSensorTopicReceiveKind Kind = EVirtualSensorTopicReceiveKind::Lidar;
	bool bValid = false;
	bool bDeepValidated = false;
	FString SchemaVersion;
	FString SensorId;
	int64 FrameId = -1;
	int32 MessageBytes = 0;
	FString Message;
};

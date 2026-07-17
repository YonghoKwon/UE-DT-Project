#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorExternalSourceHostActor.h"

#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/CameraJsonLiveSourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarCsvReplaySourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarHttpJsonLiveSourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarJsonLinesReplaySourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarJsonLiveSourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/LidarUdpJsonLiveSourceComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"

AVirtualSensorExternalSourceHostActor::AVirtualSensorExternalSourceHostActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	LidarCsvReplay = CreateDefaultSubobject<ULidarCsvReplaySourceComponent>(TEXT("LidarCsvReplay"));
	LidarJsonLinesReplay = CreateDefaultSubobject<ULidarJsonLinesReplaySourceComponent>(TEXT("LidarJsonLinesReplay"));
	LidarBufferedJson = CreateDefaultSubobject<ULidarJsonLiveSourceComponent>(TEXT("LidarBufferedJson"));
	LidarHttpJson = CreateDefaultSubobject<ULidarHttpJsonLiveSourceComponent>(TEXT("LidarHttpJson"));
	LidarUdpJson = CreateDefaultSubobject<ULidarUdpJsonLiveSourceComponent>(TEXT("LidarUdpJson"));
	CameraBufferedJson = CreateDefaultSubobject<UCameraJsonLiveSourceComponent>(TEXT("CameraBufferedJson"));
	for (URealSensorSourceComponent* Source : { static_cast<URealSensorSourceComponent*>(LidarCsvReplay), static_cast<URealSensorSourceComponent*>(LidarJsonLinesReplay), static_cast<URealSensorSourceComponent*>(LidarBufferedJson), static_cast<URealSensorSourceComponent*>(LidarHttpJson), static_cast<URealSensorSourceComponent*>(LidarUdpJson), static_cast<URealSensorSourceComponent*>(CameraBufferedJson) })
	{
		Source->SetupAttachment(Root);
		Source->bAutoStartSource = false;
		Source->bSendTransportByDefault = false;
	}
	LidarCsvReplay->SourceId = TEXT("Demo-LiDAR-CSV");
	LidarCsvReplay->CsvFilePath = TEXT("Samples/slab_replay_sample.csv");
	LidarJsonLinesReplay->SourceId = TEXT("Demo-LiDAR-JSONL");
	LidarBufferedJson->SourceId = TEXT("Live-LiDAR-Buffered");
	LidarJsonLinesReplay->JsonLinesFilePath = TEXT("Samples/slab_replay_sample.jsonl");
	LidarHttpJson->SourceId = TEXT("Live-LiDAR-HTTP");
	LidarHttpJson->ListenPort = 8082;
	LidarHttpJson->RoutePath = TEXT("/ma0t10/lidar/live");
	LidarUdpJson->SourceId = TEXT("Live-LiDAR-UDP");
	LidarUdpJson->BindAddress = TEXT("127.0.0.1");
	LidarUdpJson->BindPort = 0;
	CameraBufferedJson->SourceId = TEXT("Live-Camera-JSON");
}

void AVirtualSensorExternalSourceHostActor::BeginPlay()
{
	Super::BeginPlay();
	AVirtualLidarSensorActor* Lidar = nullptr;
	AVirtualCameraSensorActor* Camera = nullptr;
	for (TActorIterator<AVirtualLidarSensorActor> It(GetWorld()); It; ++It) { Lidar = *It; break; }
	for (TActorIterator<AVirtualCameraSensorActor> It(GetWorld()); It; ++It) { Camera = *It; break; }
	for (URealSensorSourceComponent* Source : { static_cast<URealSensorSourceComponent*>(LidarCsvReplay), static_cast<URealSensorSourceComponent*>(LidarJsonLinesReplay), static_cast<URealSensorSourceComponent*>(LidarBufferedJson), static_cast<URealSensorSourceComponent*>(LidarHttpJson), static_cast<URealSensorSourceComponent*>(LidarUdpJson) })
	{
		Source->TargetSensorActor = Lidar;
		Source->TargetLidar = Lidar ? Lidar->ScanComponent : nullptr;
	}
	CameraBufferedJson->TargetSensorActor = Camera;
	CameraBufferedJson->TargetCamera = Camera ? Camera->CaptureComponent : nullptr;
}

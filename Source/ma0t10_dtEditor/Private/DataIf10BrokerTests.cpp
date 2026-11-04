#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Device/Core/DxDeviceSensorBulkChannel.h"
#include "Device/Core/DxDeviceSensorStompChannel.h"
#include "Device/Core/VirtualSensorHighThroughputTransportSubsystem.h"
#include "Device/Sensor/VirtualPcdCodec.h"
#include "Core/DxWebSocketSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
// Docker 기능 검수 전용. GPU 획득/성능 gate가 아니라 실제 두 채널에 합성 33-byte PCD를 넣는다.
// 환경 변수는 자식 프로세스에만 주입하며 인증값은 결과/로그에 쓰지 않는다.
struct FDataIfBrokerMetrics
{
	int32 JsonConnections = 0, JsonReceipts = 0, JsonFailures = 0;
	TArray<double> JsonReceiptMs;
};

class FDataIfBrokerRun final : public IAutomationLatentCommand
{
public:
	explicit FDataIfBrokerRun(FAutomationTestBase& InTest)
	    : Test(InTest) {}
	virtual ~FDataIfBrokerRun() override { Cleanup(); }
	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (!Game.IsValid())
		{
			if (!Start(Now))
			{
				Test.AddError(TEXT("Broker functional probe initialization failed; no successful run is claimed."));
				return true;
			}
			return false;
		}
		const float Delta = static_cast<float>(FMath::Clamp(Now - LastUpdate, 0.0, 1.0));
		LastUpdate = Now;
		Game->GetTimerManager().Tick(Delta);
		Transport->Tick(Delta);
		if (!ReadyAt)
		{
			if (Transport->IsTransportConnected() && Json->IsConnected())
			{
				ReadyAt = Now;
				NextFrame = NextJson = NextSnapshot = Now;
			}
			else if (Now - StartedAt > 30)
			{
				Test.AddError(TEXT("Local Artemis channels did not connect within 30 seconds."));
				Cleanup();
				return true;
			}
			return false;
		}
		if (Now >= NextFrame)
		{
			// 지연된 프레임을 burst로 몰아 보내 20Hz로 보이게 만들지 않는다.
			NextFrame += 0.05;
			if (NextFrame < Now - 0.05)
			{
				++MissedSlots;
				NextFrame = Now + 0.05;
			}
			++FrameId;
			if (bPcdOn)
				for (int32 Sensor = 0; Sensor < 3; ++Sensor)
				{
					FVirtualSensorBinaryFrame Frame;
					Frame.StreamKind = EVirtualSensorStreamKind::PointCloud;
					Frame.SensorId = FString::Printf(TEXT("DATAIF-%d"), Sensor + 1);
					Frame.FrameId = FrameId;
					Frame.TimestampUtc = FDateTime::UtcNow();
					Frame.Schema = TEXT("virtual-pointcloud.pcd.v1");
					Frame.ContentType = TEXT("application/vnd.pcd");
					Frame.Destination = VirtualSensorDefaults::PointCloudTopic;
					Frame.RequestId = FString::Printf(TEXT("%s-%lld"), *Frame.SensorId, FrameId);
					Frame.Body32 = Body;
					Frame.Headers.Add(TEXT("checksum"), Checksum);
					Frame.Headers.Add(TEXT("point-count"), LexToString(PointCount));
					Frame.Headers.Add(TEXT("source-point-count"), LexToString(PointCount));
					Frame.Headers.Add(TEXT("acquisition-profile"), TEXT("transport-fixture-no-gpu"));
					FString Error;
					if (!Bulk->PublishFrame(Frame, Error))
						++EnqueueRejected;
				}
		}
		if (Now >= NextJson)
		{
			NextJson = Now + 1;
			const double Sent = FPlatformTime::Seconds();
			auto Shared = Metrics;
			Json->Send(VirtualSensorDefaults::LidarTopic, FString::Printf(TEXT("{\"type\":\"data-if-functional-probe\",\"sequence\":%lld}"), FrameId), {{TEXT("destination-type"), TEXT("MULTICAST")}, {TEXT("content-type"), TEXT("application/json;charset=utf-8")}}, FStompRequestCompleted::CreateLambda([Shared, Sent](bool bSuccess, const FString&)
			                                                                                                                                                                                                                                                                                                 {
				if (bSuccess)
				{
					++Shared->JsonReceipts;
					Shared->JsonReceiptMs.Add((FPlatformTime::Seconds() - Sent) * 1000);
				}
				else
					++Shared->JsonFailures; }));
		}
		const bool bFinished = Now - ReadyAt >= Seconds;
		if (Now >= NextSnapshot || bFinished)
		{
			NextSnapshot = Now + 5;
			WriteSnapshot(Now, bFinished);
		}
		if (!bFinished)
			return false;
		Test.TestTrue(TEXT("ordinary JSON broker receipts"), Metrics->JsonReceipts > 0);
		if (bPcdOn)
		{
			int64 Submitted = 0;
			for (const auto& Telemetry : Transport->GetStreamTelemetry())
				Submitted += Telemetry.SubmittedCount;
			Test.TestTrue(TEXT("PCD frames submitted by original worker"), Submitted > 0);
		}
		Cleanup();
		return true;
	}

private:
	bool Start(double Now)
	{
		const auto Env = [](const TCHAR* Key)
		{
			return FPlatformMisc::GetEnvironmentVariable(Key);
		};
		const FString Tcp = Env(TEXT("DTDEVICE_ARTEMIS_TCP_URL"));
		const FString Ws = Env(TEXT("DTDEVICE_ARTEMIS_WS_URL"));
		if (!Tcp.StartsWith(TEXT("tcp://127.0.0.1:")) || !Ws.StartsWith(TEXT("ws://127.0.0.1:")))
		{
			Test.AddError(TEXT("Broker functional probe accepts loopback URLs only."));
			return false;
		}
		const FString RequestedSeconds = Env(TEXT("DTDEVICE_ARTEMIS_SECONDS"));
		const FString RequestedPoints = Env(TEXT("DTDEVICE_ARTEMIS_POINTS"));
		Seconds = RequestedSeconds.IsEmpty() ? 120 : FMath::Clamp(FCString::Atoi(*RequestedSeconds), 10, 600);
		PointCount = RequestedPoints.IsEmpty() ? 40000 : FMath::Clamp(FCString::Atoi(*RequestedPoints), 1, 250000);
		bPcdOn = Env(TEXT("DTDEVICE_ARTEMIS_PCD")) != TEXT("0");
		Game.Reset(NewObject<UGameInstance>());
		World.Reset(UWorld::CreateWorld(EWorldType::Game, false));
		Transport.Reset(NewObject<UVirtualSensorHighThroughputTransportSubsystem>(World.Get()));
		Socket.Reset(NewObject<UDxWebSocketSubsystem>(Game.Get()));
		Bulk.Reset(NewObject<UDxDeviceSensorBulkChannel>(Socket.Get()));
		Json.Reset(NewObject<UDxDeviceSensorStompChannel>(Socket.Get()));
		FDxDataChannelSettings Settings;
		Settings.bAutoConnect = false;
		Settings.bUseDefaultCredentials = false;
		Settings.ChannelName = TEXT("sensor-bulk");
		if (!Socket->RegisterChannelWithSettings(Bulk.Get(), Settings))
			return false;
		Settings.ChannelName = TEXT("sensor");
		Settings.ReceiptTimeoutSeconds = 5;
		if (!Socket->RegisterChannelWithSettings(Json.Get(), Settings))
			return false;
		Bulk->SetExplicitSettings(false);
		Json->SetExplicitSettings(false);
		FVirtualSensorTransportProfile Profile;
		Profile.UserName = Env(TEXT("DTDEVICE_ARTEMIS_USER"));
		Profile.BrokerUrl = Tcp;
		const FString Passcode = Env(TEXT("DTDEVICE_ARTEMIS_PASSCODE"));
		FString Error;
		if (!Bulk->Prepare(*Transport, UVirtualSensorHighThroughputTransportSubsystem::MakeProfile(Profile), Passcode, Error, false))
		{
			Test.AddError(TEXT("Bulk worker preparation failed."));
			return false;
		}
		Profile.BrokerUrl = Ws;
		if (!Json->AcquireOwner(Game.Get(), Profile, Passcode, FString(), Error))
			return false;
		Json->Connected.AddLambda([Shared = Metrics](const FString&, const FString&, const FString&)
		                          { ++Shared->JsonConnections; });
		Json->Connect();
		FVirtualLidarPoint Point;
		Point.SensorLocalPositionMeters = FVector(1, 2, 3);
		Point.bHit = true;
		TArray<const FVirtualLidarPoint*> Points;
		Points.Init(&Point, PointCount);
		TArray<uint8> Bytes;
		if (!FVirtualPcdCodec::Encode(Points, TEXT("{}"), FVirtualPcdSendOptions{}, Bytes))
			return false;
		FSHAHash Hash;
		FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash.Hash);
		Checksum = Hash.ToString();
		Body = MakeShared<const TArray<uint8>, ESPMode::ThreadSafe>(MoveTemp(Bytes));
		Report = FPaths::ProjectSavedDir() / TEXT("Reports/DATA-IF-10/BrokerRuntime");
		IFileManager::Get().MakeDirectory(*Report, true);
		StartedAt = LastUpdate = Now;
		return true;
	}
	void WriteSnapshot(double Now, bool bFinal)
	{
		auto Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("scope"), TEXT("synthetic-PCD-real-channels-no-GPU"));
		Root->SetNumberField(TEXT("seconds"), Now - ReadyAt);
		Root->SetBoolField(TEXT("final"), bFinal);
		Root->SetBoolField(TEXT("pcd_on"), bPcdOn);
		Root->SetNumberField(TEXT("frames_per_sensor"), FrameId);
		Root->SetNumberField(TEXT("target_hz"), 20);
		Root->SetNumberField(TEXT("scheduled_hz"), FrameId / FMath::Max(0.001, Now - ReadyAt));
		Root->SetNumberField(TEXT("missed_slots"), MissedSlots);
		Root->SetNumberField(TEXT("enqueue_rejected"), EnqueueRejected);
		Root->SetNumberField(TEXT("frame_bytes"), Body->Num());
		Root->SetNumberField(TEXT("json_connections"), Metrics->JsonConnections);
		Root->SetNumberField(TEXT("json_receipts"), Metrics->JsonReceipts);
		Root->SetNumberField(TEXT("json_failures"), Metrics->JsonFailures);
		Root->SetNumberField(TEXT("bulk_connection_revision"), Transport->GetConnectionRevision());
		double Total = 0;
		auto Sorted = Metrics->JsonReceiptMs;
		Sorted.Sort();
		for (double Value : Sorted)
			Total += Value;
		if (!Sorted.IsEmpty())
		{
			Root->SetNumberField(TEXT("json_receipt_mean_ms"), Total / Sorted.Num());
			Root->SetNumberField(TEXT("json_receipt_p95_ms"), Sorted[FMath::Clamp(FMath::CeilToInt(Sorted.Num() * .95) - 1, 0, Sorted.Num() - 1)]);
		}
		TArray<TSharedPtr<FJsonValue>> Statuses;
		for (const auto& Status : Socket->GetAllChannelStatus(false))
		{
			auto Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("name"), Status.ChannelName.ToString());
			Row->SetNumberField(TEXT("state"), static_cast<int32>(Status.State));
			Row->SetNumberField(TEXT("published"), Status.PublishedCount);
			Row->SetNumberField(TEXT("dropped"), Status.DroppedCount);
			Row->SetNumberField(TEXT("backpressure"), Status.BackpressureCount);
			Statuses.Add(MakeShared<FJsonValueObject>(Row));
		}
		Root->SetArrayField(TEXT("channels"), Statuses);
		FString Text;
		FJsonSerializer::Serialize(Root, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
		FFileHelper::SaveStringToFile(Text + LINE_TERMINATOR, *(Report / TEXT("status.jsonl")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
		if (bFinal)
			FFileHelper::SaveStringToFile(Text, *(Report / TEXT("summary.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
	void Cleanup()
	{
		if (Json.IsValid())
			Json->ShutdownChannel();
		if (Bulk.IsValid())
			Bulk->ShutdownChannel();
		if (Transport.IsValid())
			Transport->StopHighThroughputTransport();
		Transport.Reset();
		Json.Reset();
		Bulk.Reset();
		Socket.Reset();
		if (World.IsValid())
			World->DestroyWorld(false);
		World.Reset();
		Game.Reset();
	}
	FAutomationTestBase& Test;
	TStrongObjectPtr<UGameInstance> Game;
	TStrongObjectPtr<UWorld> World;
	TStrongObjectPtr<UVirtualSensorHighThroughputTransportSubsystem> Transport;
	TStrongObjectPtr<UDxWebSocketSubsystem> Socket;
	TStrongObjectPtr<UDxDeviceSensorBulkChannel> Bulk;
	TStrongObjectPtr<UDxDeviceSensorStompChannel> Json;
	TSharedRef<FDataIfBrokerMetrics> Metrics = MakeShared<FDataIfBrokerMetrics>();
	TSharedPtr<const TArray<uint8>, ESPMode::ThreadSafe> Body;
	FString Checksum, Report;
	double StartedAt = 0, LastUpdate = 0, ReadyAt = 0, NextFrame = 0, NextJson = 0, NextSnapshot = 0;
	int64 FrameId = 0, MissedSlots = 0, EnqueueRejected = 0;
	int32 Seconds = 120, PointCount = 40000;
	bool bPcdOn = true;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDataIf10BrokerRuntime, "MA0T10.DataIf10.BrokerRuntime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDataIf10BrokerRuntime::RunTest(const FString&)
{
	if (FPlatformMisc::GetEnvironmentVariable(TEXT("DTDEVICE_ARTEMIS_RUN")) != TEXT("1"))
	{
		AddInfo(TEXT("[OPT-IN SKIP] DTDEVICE_ARTEMIS_RUN=1 enables loopback-only real-channel broker checks after Unreal performance/memory gates."));
		return true;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FDataIfBrokerRun(*this));
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/Training/TrainBatchSubsystem.h"
#include "ma0t10_dt/MA0T10/Training/TrainBatchCodec.h"
#include "ma0t10_dt/MA0T10/Training/TrainBatchProcessProtocol.h"
#include "Device/Sensor/VirtualPcdCodec.h"
#include "ma0t10_dt/MA0T10/Dataset/VirtualDatasetSubsystem.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "Device/Sensor/VirtualLidarSensorActor.h"
#include "Device/Sensor/VirtualLidarScanComp.h"
#include "SensorOwnedRayTracing.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "ShaderCompiler.h"
#include "RHIGlobals.h"
#include "Json.h"

namespace
{
FString RuntimeJson(const TSharedRef<FJsonObject>& O)
{
	FString S;
	FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&S));
	return S;
}
class FTrainM1Command final : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	double Started = FPlatformTime::Seconds(), StageAt = 0;
	int32 Stage = 0, Labels = 0, WorkerCount = 0;
	UWorld* World = nullptr;
	TWeakObjectPtr<UWorld> ObservedWorld, WorldOwner;
	double ObservedAt = 0;
	TWeakObjectPtr<UTrainBatchSubsystem> Service;
	TWeakObjectPtr<ASlabActor> Slab;
	TWeakObjectPtr<AVirtualLidarSensorActor> Lidar;
	FString JobId, Root;
	int64 PreviewFrame = 0;
	uint64 NativeBefore = 0, NativeAfter = 0;
	TArray<double> Times;
	TArray<TSharedPtr<FJsonValue>> PayloadHashes;

public:
	explicit FTrainM1Command(FAutomationTestBase* T, int32 Workers = 0)
	    : Test(T), WorkerCount(Workers) {}
	~FTrainM1Command()
	{
		if (WorldOwner.IsValid())
			if (auto* D = WorldOwner->GetSubsystem<UVirtualDatasetSubsystem>())
				D->OnSceneSnapshotForTests = nullptr;
	}
	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > (WorkerCount ? 600 : 180))
		{
			Test->AddError(TEXT("Train runtime timeout: ") + (Service.IsValid() ? Service->GetStatus() : TEXT("no World")));
			return Finish(false);
		}
		if (!World)
		{
			UWorld* Candidate = nullptr;
			for (const auto& Context : GEngine->GetWorldContexts())
				if (Context.WorldType == EWorldType::PIE)
				{
					Candidate = Context.World();
					break;
				}
			if (Candidate != ObservedWorld.Get())
			{
				ObservedWorld = Candidate;
				ObservedAt = Now;
				return false;
			}
			if (!Candidate || !Candidate->GetGameInstance() || Now - ObservedAt < 3)
				return false;
			World = Candidate;
			WorldOwner = World;
			Service = World->GetSubsystem<UTrainBatchSubsystem>();
			if (!Service.IsValid())
			{
				Test->AddError(TEXT("Train subsystem absent in PIE"));
				return Finish(false);
			}
			StageAt = Now;
		}
		if (!WorldOwner.IsValid() || !Service.IsValid())
		{
			Test->AddError(TEXT("PIE World changed during training test"));
			return Finish(false);
		}
		if (Stage == 0)
		{
			const auto C = Service->CountersForTests();
			if (Service->IsEnabled() || C.PumpCalls || C.AsyncStarts || C.SubscriptionRequests || C.TimerActive || C.FutureActive || C.SubscriptionActive)
			{
				Test->AddError(TEXT("Default-off started tick/async/subscription work"));
				return Finish(false);
			}
			if (Now - StageAt < 3 || (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()))
				return false;
			Test->AddInfo(TEXT("Default-off observed across PIE ticks: pump=0 async=0 subscribe=0 active resources=0"));
			if (WorkerCount)
			{
				for (TActorIterator<ASlabActor> It(World); It; ++It)
				{
					It->StopSimulation();
					if (!Slab.IsValid())
						Slab = *It;
				}
				for (TActorIterator<AVirtualLidarSensorActor> It(World); It; ++It)
				{
					It->StopSensor();
					if (!Lidar.IsValid())
						Lidar = *It;
				}
			}
			else
			{
				Slab = World->SpawnActor<ASlabActor>(FVector(1500, 0, 200), FRotator::ZeroRotator);
				Lidar = World->SpawnActor<AVirtualLidarSensorActor>(FVector(1500, -1000, 1000), FRotator::ZeroRotator);
			}
			if (!Slab.IsValid() || !Lidar.IsValid())
			{
				Test->AddError(TEXT("Managed fixture spawn failed"));
				return Finish(false);
			}
			Slab->Tags.Add(TEXT("SensorTestManaged"));
			Slab->Tags.Add(TEXT("Slab"));
			Lidar->Tags.Add(TEXT("SensorTestManaged"));
			Slab->SetDiagnosticHelpersVisible(false);
			Slab->ApplyDatasetGeometry(FVector(800, 200, 50));
			Slab->SetDimensionUnit(ESlabInputUnit::Centimeters);
			Slab->SetPositionUnit(ESlabInputUnit::Centimeters);
			Lidar->StopSensor();
			if (WorkerCount)
				Lidar->SetActorLocation(Slab->GetActorLocation() + FVector(0, -1000, 800));
			Lidar->SetActorRotation((Slab->GetActorLocation() - Lidar->GetActorLocation()).Rotation());
			auto* Scan = Lidar->ScanComp.Get();
			Scan->ApplyDeviceProfile(EVirtualLidarDeviceProfile::IYOBOT_MLX80_NATIVE);
			Scan->ApplySimulationQuality(EVirtualSensorSimulationQuality::FullSpec);
			Scan->SensorId = TEXT("TRAIN-M1-RHI");
			Scan->OutputMode = EVirtualLidarOutputMode::None;
			Scan->AcquisitionBackend = EVirtualLidarAcquisitionBackend::HardwareRayTracing;
			Scan->bEnableRangeNoise = false;
			if (!WriteFixture())
			{
				Test->AddError(TEXT("Runtime fixture write failed"));
				return Finish(false);
			}
			Stage = 1;
			StageAt = Now;
			return false;
		}
		if (Stage == 1)
		{
			if (Now - StageAt < 2 || (GShaderCompilingManager && GShaderCompilingManager->IsCompiling()))
				return false;
			FString Error;
			if (!Service->SetEnabled(true, Error) || !Service->ConfigureTargets(TEXT("train-runtime"), Slab.Get(), Lidar.Get(), nullptr, Error) || !Service->ImportManifest(Root / TEXT("job.json"), Error))
			{
				Test->AddError(Error);
				return Finish(false);
			}
			if (WorkerCount)
			{
				if (!Service->SetProcessWorkers(WorkerCount, Error))
				{
					Test->AddError(Error);
					return Finish(false);
				}
			}
			else
				Service->UseInProcessForTests();
			auto* D = World->GetSubsystem<UVirtualDatasetSubsystem>();
			D->OnSceneSnapshotForTests = [this](const FJsonObject& L)
			{
				++Labels;
				Times.Add(L.GetNumberField(TEXT("simulation_elapsed_s")));
				Test->TestEqual(TEXT("actual hardware RT acquisition"), int32(L.GetNumberField(TEXT("lidar_backend"))), int32(EVirtualLidarAcquisitionBackend::HardwareRayTracing));
			};
			Stage = 2;
			return false;
		}
		if (Stage == 2)
		{
			const auto* J = Find();
			if (!J || !J->IsReady())
				return false;
			FString Error;
			NativeBefore = SensorOwnedRayTracing::GetSubmissionStats().NativeDispatches;
			if (!Service->StartJob(JobId, Error))
			{
				Test->AddError(Error);
				return Finish(false);
			}
			Stage = 3;
			return false;
		}
		if (Stage == 3)
		{
			const auto* J = Find();
			if (!J)
				return false;
			if (!WorkerCount)
				for (const auto& Item : J->Items)
					if (Item.State == ETrainBatchItemState::Failed)
					{
						Test->AddError(Item.Message);
						return Finish(false);
					}
			if (J->CompleteCount() != (WorkerCount ? 4 : 2))
			{
				if (WorkerCount && Service->GetStatus().StartsWith(TEXT("워커 Job 종료")))
				{
					Test->AddError(TEXT("Process pool ended with incomplete scenarios"));
					return Finish(false);
				}
				return false;
			}
			if (!WorkerCount)
			{
				Test->TestEqual(TEXT("all 20Hz samples, no demo one-frame cap"), Labels, 6);
				if (Times.Num() == 6)
					for (int32 I = 0; I < 6; ++I)
						Test->TestTrue(TEXT("simulation clock independent of wall pacing"), FMath::Abs(Times[I] - double(I % 3) / 20) < 1.e-6);
			}
			const auto Stats = SensorOwnedRayTracing::GetSubmissionStats();
			NativeAfter = Stats.NativeDispatches;
			if (!WorkerCount)
				Test->TestTrue(TEXT("persistent native actually dispatched"), NativeAfter > NativeBefore);
			else
			{
				Test->TestEqual(TEXT("coordinator does not acquire worker frames"), NativeAfter, NativeBefore);
				for (const auto& I : J->Items)
					for (int32 K = 0; K < 3; ++K)
					{
						TArray<uint8> B;
						FVirtualPcdDocument D;
						FString Error, Hash;
						const FString P = I.RunDirectory / FString::Printf(TEXT("scene_%05d/lidar.pcd"), K);
						if (!FFileHelper::LoadFileToArray(B, *P) || !FVirtualPcdCodec::Decode(B, D, Error) || !TrainBatchCodec::Sha256(D.Records.GetData(), D.Records.Num(), Hash))
						{
							Test->AddError(TEXT("Process oracle PCD read failed"));
							return Finish(false);
						}
						Test->TestTrue(TEXT("nonempty measured points"), D.PointCount > 0);
						PayloadHashes.Add(MakeShared<FJsonValueString>(Hash));
					}
			}
			for (const auto& Item : J->Items)
				Test->TestTrue(TEXT("confirmed result marker"), IFileManager::Get().FileExists(*(Item.RunDirectory / TEXT("complete.json"))));
			const auto Frame = Lidar->ScanComp->GetLastFrameSnapshot();
			PreviewFrame = Frame ? Frame->FrameId : 0;
			FString Error;
			if (!Service->SelectPreview(JobId, 0, Error))
			{
				Test->AddError(Error);
				return Finish(false);
			}
			Stage = 4;
			StageAt = Now;
			return false;
		}
		if (Stage == 4)
		{
			if (Now - StageAt < 3)
				return false;
			Test->TestNotNull(TEXT("saved PCD projected for read-only view"), Service->PreviewTexture());
			const auto Frame = Lidar->ScanComp->GetLastFrameSnapshot();
			Test->TestEqual(TEXT("viewer does not reacquire sensor"), Frame ? Frame->FrameId : 0, PreviewFrame);
			Test->TestEqual(TEXT("viewer creates no RT scan"), SensorOwnedRayTracing::GetSubmissionStats().NativeDispatches, NativeAfter);
			Service->StopPreview();
			FString Error;
			if (!Service->SetEnabled(false, Error))
			{
				Test->AddError(Error);
				return Finish(false);
			}
			const auto C = Service->CountersForTests();
			Test->TestFalse(TEXT("disabled timer"), C.TimerActive);
			Test->TestFalse(TEXT("disabled async"), C.FutureActive);
			Test->TestFalse(TEXT("disabled subscription"), C.SubscriptionActive);
			return Finish(true);
		}
		return false;
	}

private:
	const FTrainBatchJobView* Find() const
	{
		return Service->Jobs().FindByPredicate([&](const auto& J)
		                                       { return J.Spec.Id == JobId; });
	}
	bool WriteFixture()
	{
		JobId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
		Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Reports/TrainBatchRuntime") / JobId);
		IFileManager::Get().MakeDirectory(*Root, true);
		auto Job = MakeShared<FJsonObject>();
		Job->SetStringField(TEXT("schema"), TrainBatchContract::Schema);
		Job->SetStringField(TEXT("MESSAGE_ID"), TrainBatchContract::MessageId);
		Job->SetStringField(TEXT("kind"), TEXT("job"));
		Job->SetStringField(TEXT("job_id"), JobId);
		Job->SetStringField(TEXT("preset_id"), TEXT("train-runtime"));
		Job->SetStringField(TEXT("dimension_unit"), TEXT("cm"));
		Job->SetStringField(TEXT("position_unit"), TEXT("cm"));
		Job->SetNumberField(TEXT("scenario_count"), WorkerCount ? 4 : 2);
		Job->SetNumberField(TEXT("worker_limit"), WorkerCount ? WorkerCount : 1);
		auto E = MakeShared<FJsonObject>();
		E->SetStringField(TEXT("mode"), TEXT("sampled_timeline"));
		E->SetNumberField(TEXT("sample_hz"), 20);
		Job->SetObjectField(TEXT("execution"), E);
		TArray<TSharedPtr<FJsonValue>> Specs;
		for (int32 N = 0; N < (WorkerCount ? 4 : 2); ++N)
		{
			auto O = MakeShared<FJsonObject>();
			O->SetStringField(TEXT("MESSAGE_ID"), TEXT("IFactory-agent"));
			O->SetStringField(TEXT("CREATE_TIMESTAMP"), TEXT("20261102000000"));
			auto M = MakeShared<FJsonObject>();
			M->SetStringField(TEXT("UUID"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
			M->SetNumberField(TEXT("duration_sec"), .15);
			O->SetObjectField(TEXT("_meta"), M);
			TArray<TSharedPtr<FJsonValue>> Rows;
			for (int32 K = 0; K < 2; ++K)
			{
				auto R = MakeShared<FJsonObject>();
				R->SetNumberField(TEXT("frame_no"), 10 + K);
				R->SetStringField(TEXT("mtl_no"), TEXT("TRAIN-RUNTIME"));
				R->SetNumberField(TEXT("slab_thk"), 50);
				R->SetNumberField(TEXT("slab_wth"), 200);
				R->SetNumberField(TEXT("slab_len"), 800);
				R->SetNumberField(TEXT("slab_wt"), 1);
				R->SetNumberField(TEXT("left_skew_angle"), 0);
				R->SetNumberField(TEXT("right_skew_angle"), 0);
				R->SetNumberField(TEXT("center_x"), K + N);
				R->SetNumberField(TEXT("elapsed_sec"), K * .1);
				Rows.Add(MakeShared<FJsonValueObject>(R));
			}
			O->SetArrayField(TEXT("DATA_MAP"), Rows);
			const FString Raw = RuntimeJson(O), Name = FString::Printf(TEXT("case-%d.json"), N);
			FTCHARToUTF8 U(*Raw);
			FString Hash;
			if (!TrainBatchCodec::Sha256(U.Get(), U.Length(), Hash) || !FFileHelper::SaveStringToFile(Raw, *(Root / Name), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
				return false;
			auto Spec = MakeShared<FJsonObject>();
			Spec->SetStringField(TEXT("scenario_id"), FString::Printf(TEXT("case-%d"), N));
			Spec->SetStringField(TEXT("input_file"), Name);
			Spec->SetStringField(TEXT("input_sha256"), Hash);
			Spec->SetNumberField(TEXT("input_bytes"), U.Length());
			Spec->SetNumberField(TEXT("duration_sec"), .15);
			Specs.Add(MakeShared<FJsonValueObject>(Spec));
		}
		Job->SetArrayField(TEXT("scenarios"), Specs);
		return FFileHelper::SaveStringToFile(RuntimeJson(Job), *(Root / TEXT("job.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
	bool Finish(bool Success)
	{
		if (Service.IsValid())
			Service->CancelJob();
		if (WorldOwner.IsValid())
			if (auto* D = WorldOwner->GetSubsystem<UVirtualDatasetSubsystem>())
				D->OnSceneSnapshotForTests = nullptr;
		auto O = MakeShared<FJsonObject>();
		O->SetBoolField(TEXT("success"), Success);
		O->SetStringField(TEXT("job_id"), JobId);
		O->SetNumberField(TEXT("requested_workers"), WorkerCount);
		O->SetArrayField(TEXT("payload_hashes"), PayloadHashes);
		O->SetNumberField(TEXT("labels"), Labels);
		O->SetNumberField(TEXT("native_dispatch_delta"), double(NativeAfter >= NativeBefore ? NativeAfter - NativeBefore : 0));
		O->SetStringField(TEXT("scope"), TEXT("local real-RHI automation; no UI mouse acceptance or wall-clock 20Hz claim"));
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("Reports/TrainBatchRuntime");
		IFileManager::Get().MakeDirectory(*Dir, true);
		FFileHelper::SaveStringToFile(RuntimeJson(O), *(Dir / (WorkerCount ? FString::Printf(TEXT("process-%d-result.json"), WorkerCount) : FString(TEXT("m1-result.json")))));
		return true;
	}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchM1RhiTest, "MA0T10.TrainRuntime.M1Rhi", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTrainBatchM1RhiTest::RunTest(const FString&)
{
	if (GUsingNullRHI)
	{
		AddError(TEXT("Train M1 RHI test requires real D3D12"));
		return false;
	}
	FAutomationEditorCommonUtils::CreateNewMap();
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FTrainM1Command(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchProcessRhiTest, "MA0T10.TrainRuntime.ProcessRhi", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTrainBatchProcessRhiTest::RunTest(const FString&)
{
	const int32 Workers = FCString::Atoi(*FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_TRAIN_WORKERS")));
	if (GUsingNullRHI || Workers < 1 || Workers > 4)
	{
		AddError(TEXT("Process RHI requires real D3D12 and MA0T10_TRAIN_WORKERS=1..4"));
		return false;
	}
	if (!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"), true))
		return false;
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FTrainM1Command(this, Workers));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif

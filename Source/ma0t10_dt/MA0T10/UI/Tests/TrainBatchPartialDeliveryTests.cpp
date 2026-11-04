#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "ma0t10_dt/MA0T10/UI/Topview/DxWidgetTrainBatch.h"
#include "ma0t10_dt/MA0T10/Training/TrainBatchSubsystem.h"
#include "ma0t10_dt/MA0T10/Training/TrainBatchCodec.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/World.h"
#include "Json.h"

/** Exercises the real UI request builder without opening a panel or contacting a server. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainBatchPartialUiDeliveryTest, "MA0T10.TrainBatch.PartialUiDelivery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTrainBatchPartialUiDeliveryTest::RunTest(const FString&)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("isolated in-memory game world"), World))
		return false;
	struct FWorldCleanup
	{
		UWorld* World;
		~FWorldCleanup() { World->DestroyWorld(false); }
	} Cleanup{World};
	auto* Service = World->GetSubsystem<UTrainBatchSubsystem>();
	if (!TestNotNull(TEXT("training service"), Service))
		return false;
	FString Error;
	if (!TestTrue(TEXT("explicit fixture activation"), Service->SetEnabled(true, Error)))
		return false;

	FTrainBatchJobView Job;
	Job.Spec.Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	const FString JobRoot = TrainBatchCodec::JobDirectory(Job.Spec.Id);
	auto Save = [&](const FString& Relative, const FString& Text)
	{
		const FString File = JobRoot / Relative;
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
		return FFileHelper::SaveStringToFile(Text, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	};
	TestTrue(TEXT("fixture input"), Save(TEXT("input/job.json"), TEXT("{\"fixture\":\"partial-ui\"}")));
	FTrainBatchItemView Done;
	Done.Spec.Id = TEXT("successful-001");
	Done.State = ETrainBatchItemState::Completed;
	Done.RunDirectory = JobRoot / TEXT("scenarios/successful-001/run_confirmed");
	TestTrue(TEXT("confirmed marker"), Save(TEXT("scenarios/successful-001/run_confirmed/complete.json"), TEXT("{}")));
	TestTrue(TEXT("confirmed bytes"), Save(TEXT("scenarios/successful-001/run_confirmed/result.bin"), TEXT("confirmed fixture output\n")));
	TestTrue(TEXT("stale attempt bytes"), Save(TEXT("scenarios/successful-001/run_stale/result.bin"), TEXT("must not be packaged")));
	FTrainBatchItemView Failed;
	Failed.Spec.Id = TEXT("failed-002");
	Failed.State = ETrainBatchItemState::Failed;
	TestTrue(TEXT("failed attempt bytes"), Save(TEXT("scenarios/failed-002/run_failed/result.bin"), TEXT("must not be packaged")));
	Job.Items = {Done, Failed};
	// The fixture changes only the mutable UI view cache after explicit activation.
	// Storage/admission and sensor generation are covered by the reused r41c evidence.
	auto& Views = const_cast<TArray<FTrainBatchJobView>&>(Service->Jobs());
	Views.Add(Job);
	auto* Widget = NewObject<UDxWidgetTrainBatch>(World);
	Widget->SelectedJob = Job.Spec.Id;
	Widget->StartDelivery(false);
	TestFalse(TEXT("partial job refused by the actual default UI action"), Widget->Delivery.IsValid());
	TestFalse(TEXT("default refusal explains itself"), Widget->DeliveryFeedback.IsEmpty());
	TestFalse(TEXT("default refusal created no archive"), IFileManager::Get().DirectoryExists(*TrainBatchDelivery::OutputDirectory(Job.Spec.Id)));

	Widget->bSendSuccessOnly = true;
	TArray<TSharedPtr<FJsonValue>> Archives;
	for (const auto Format : {ETrainArchiveFormat::Zip, ETrainArchiveFormat::TarGz})
	{
		Widget->Format = Format;
		Widget->StartDelivery(false);
		if (!TestTrue(TEXT("explicit success-only UI action starts delivery"), Widget->Delivery.IsValid()))
			return false;
		Widget->Delivery->Wait();
		const auto Status = Widget->Delivery->Status();
		if (!TestTrue(*(TEXT("package-only completion: ") + Status.Error), Status.Phase == ETrainDeliveryPhase::Done))
			return false;
		const FString Excluded = Status.ExcludedPath;
		FString Text;
		TestTrue(TEXT("UI exclusion source exists"), FFileHelper::LoadFileToString(Text, *Excluded));
		TestEqual(TEXT("UI exclusion lists only failed scenario"), Text, FString(TEXT("failed-002\n")));
		auto A = MakeShared<FJsonObject>();
		A->SetStringField(TEXT("archive"), Status.ArchivePath);
		A->SetStringField(TEXT("checksums"), Status.ChecksumsPath);
		A->SetStringField(TEXT("sha256"), Status.ArchiveSha256);
		Archives.Add(MakeShared<FJsonValueObject>(A));
	}
	Widget->Delivery.Reset();
	Views.Reset();
	TestTrue(TEXT("fixture deactivated"), Service->SetEnabled(false, Error));
	auto Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("job_id"), Job.Spec.Id);
	Report->SetBoolField(TEXT("default_refused"), true);
	Report->SetBoolField(TEXT("upload"), false);
	Report->SetArrayField(TEXT("archives"), Archives);
	Report->SetStringField(TEXT("expected_excluded"), TEXT("failed-002\n"));
	Report->SetStringField(TEXT("scope"), TEXT("Synthetic partial-job view; actual UI StartDelivery and Delivery code. No mouse or sensor execution."));
	FString Json;
	FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
	const FString ReportRoot = FPaths::ProjectSavedDir() / TEXT("Reports/TrainBatchPartialDelivery");
	IFileManager::Get().MakeDirectory(*ReportRoot, true);
	TestTrue(TEXT("independent-verifier receipt"), FFileHelper::SaveStringToFile(Json, *(ReportRoot / TEXT("result.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	return true;
}
#endif

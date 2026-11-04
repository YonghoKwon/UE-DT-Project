#include "SensorOwnedRayTracing.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
class FNativeOracle final : public IAutomationLatentCommand
{
	FAutomationTestBase* Test;
	double Started = FPlatformTime::Seconds();
	int32 Epoch = 0;
	bool Submitted = false, ReferenceReady = false;
	bool Ready[3] = {false, false, false};
	TArray<FSensorRtMesh> Meshes;
	TArray<TArray<FSensorRtRay>> Rays;
	TArray<FSensorRtHit> Expected;
	TArray<TArray<FSensorRtHit>> Actual;
	TSharedPtr<ISensorRtScan, ESPMode::ThreadSafe> Reference;
	TArray<TSharedPtr<ISensorRtScan, ESPMode::ThreadSafe>> Scans;
	void Submit()
	{
		Meshes.Reset();
		for (int32 I = 0; I < 4; ++I)
		{
			auto V = MakeShared<TArray<FVector3f>, ESPMode::ThreadSafe>();
			V->Append({FVector3f(100, -100, -100), FVector3f(100, 100, -100), FVector3f(100, 100, 100), FVector3f(100, -100, -100), FVector3f(100, 100, 100), FVector3f(100, -100, 100)});
			auto N = MakeShared<TArray<FVector4f>, ESPMode::ThreadSafe>();
			const uint32 Surface = I + 11;
			float Bits;
			FMemory::Memcpy(&Bits, &Surface, sizeof(Bits));
			N->Add(FVector4f(-.8f, .3f, .2f, Bits));
			N->Add(FVector4f(-.2f, -.7f, .6f, Bits));
			FSensorRtMesh M;
			M.Vertices = V;
			M.NormalsAndSurfaces = N;
			M.Revision = 0x267000000000ull + I + uint64(Epoch / 2) * 4;
			M.LocalToWorld = FTransform(FRotator(7 + Epoch * 2, 13 + I * 11, 3), FVector(I * 350, Epoch * 10, I * 30), FVector(1.3, .7, 1.1));
			Meshes.Add(M);
		}
		Rays.SetNum(3);
		Actual.SetNum(3);
		Expected.Reset();
		Scans.Reset();
		ReferenceReady = false;
		for (int32 I = 0; I < 3; ++I)
		{
			Rays[I].Reset();
			Actual[I].Reset();
			Ready[I] = false;
			for (int32 J = 0; J < 257 + I * 256; ++J)
			{
				const FTransform& Pose = Meshes[J % 4].LocalToWorld;
				const FVector O = Pose.TransformPosition(FVector(0, (J % 29) - 14, (J % 23) - 11));
				const FVector D = Pose.TransformVector(FVector(1, .003 * (J % 5), .002 * (J % 7))).GetSafeNormal();
				Rays[I].Add({FVector3f(O), J % 19 == 0 ? 5.f : 250.f, FVector3f(D), 0});
			}
		}
		FSensorRtRequest Fresh;
		Fresh.Meshes = Meshes;
		Fresh.bFreshResourcesForTests = true;
		for (const auto& Part : Rays)
			Fresh.Rays.Append(Part);
		Reference = SensorOwnedRayTracing::Submit(MoveTemp(Fresh));
		for (int32 I = 0; I < 3; ++I)
		{
			FSensorRtRequest R;
			R.Meshes = Meshes;
			R.Rays = Rays[I];
			R.OwnerToken = I + 1;
			Scans.Add(SensorOwnedRayTracing::Submit(MoveTemp(R)));
		}
		SensorOwnedRayTracing::FlushPendingSubmissions();
		if (Epoch % 2)
			Scans[0]->Cancel(); // one cancelled member must not lose its siblings
		Submitted = true;
	}

public:
	explicit FNativeOracle(FAutomationTestBase* In)
	    : Test(In) {}
	bool Update() override
	{
		if (FPlatformTime::Seconds() - Started > 60)
		{
			Test->AddError(TEXT("native multi-scene oracle timeout"));
			for (auto& S : Scans)
				if (S)
					S->Cancel();
			if (Reference)
				Reference->Cancel();
			return true;
		}
		if (!Submitted)
		{
			if (!SensorOwnedRayTracing::IsAvailable() || (!SensorOwnedRayTracing::UsesNativeBackend() && !SensorOwnedRayTracing::UsesPersistentSceneStorage()))
			{
				Test->AddError(TEXT("rewritten oracle requires actual DXR rewritten backend; no skip"));
				return true;
			}
			Submit();
			if (!Reference || Scans.Contains(nullptr))
			{
				Test->AddError(TEXT("oracle admission missing"));
				return true;
			}
			return false;
		}
		FString Error;
		if (!ReferenceReady)
		{
			ReferenceReady = Reference->Poll(Expected, Error);
			if (!ReferenceReady)
				return false;
			if (!Error.IsEmpty())
			{
				Test->AddError(Error);
				return true;
			}
		}
		for (int32 I = 0; I < 3; ++I)
			if (!Ready[I])
			{
				Ready[I] = Scans[I]->Poll(Actual[I], Error);
				if (!Ready[I])
					return false;
				if (!Error.IsEmpty())
				{
					Test->AddError(Error);
					return true;
				}
			}
		int32 Offset = 0;
		for (int32 I = 0; I < 3; ++I)
		{
			const int32 Count = Rays[I].Num();
			if (I == 0 && Epoch % 2)
				Test->TestEqual(TEXT("cancelled member publishes no points"), Actual[I].Num(), 0);
			else
			{
				Test->TestEqual(TEXT("unequal member size preserved"), Actual[I].Num(), Count);
				if (Actual[I].Num() == Count && Expected.Num() >= Offset + Count)
					Test->TestTrue(TEXT("all hit/normal/identity/miss/reserved bytes equal fresh engine oracle after pose/revision changes"), FMemory::Memcmp(Actual[I].GetData(), Expected.GetData() + Offset, Count * sizeof(FSensorRtHit)) == 0);
			}
			Offset += Count;
		}
		Scans.Reset();
		Reference.Reset();
		const auto Stats = SensorOwnedRayTracing::GetSubmissionStats();
		Test->TestTrue(TEXT("rewritten backend actually ran"), Stats.NativeDispatches + (Epoch == 0 ? Stats.SceneStorageCreates : Stats.SceneStorageReuses) > 0);
		Test->TestTrue(TEXT("entire live native allocation ledger respects cap"), Stats.NativeBytes <= 512ull * 1024 * 1024);
		if (++Epoch == 12)
			return true;
		Submitted = false;
		return false;
	}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorNativeOracleTest, "MA0T10.GpuLidar.RewrittenMultiSceneByteOracleRhi", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSensorNativeOracleTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FNativeOracle(this));
	return true;
}
#endif

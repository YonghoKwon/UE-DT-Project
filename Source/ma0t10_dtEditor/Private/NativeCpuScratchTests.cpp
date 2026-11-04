#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Memory26Diagnostics.h"
#include "SensorOwnedRayTracing.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNativeCpuScratchTest, "MA0T10.GpuLidar.CpuScratchReuseAndBounds",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNativeCpuScratchTest::RunTest(const FString&)
{
	struct FBytes
	{
		TArray<uint8> Data;
		uint64 Bytes() const { return Data.GetAllocatedSize(); }
	};
	Memory26::TPool<FBytes, 2, 128> Pool;
	auto A = Pool.Take();
	A->Data.Reserve(64);
	Pool.Return(MoveTemp(A));
	auto B = MakeUnique<FBytes>();
	B->Data.Reserve(80);
	Pool.Return(MoveTemp(B));
	TestEqual(TEXT("byte limit rejects excess retained capacity"), Pool.RetainedCount(), 1);
	auto C = MakeUnique<FBytes>();
	C->Data.Reserve(64);
	Pool.Return(MoveTemp(C));
	auto E = MakeUnique<FBytes>();
	E->Data.Reserve(1);
	Pool.Return(MoveTemp(E));
	TestEqual(TEXT("free item limit stays bounded"), Pool.RetainedCount(), 2);
	TestTrue(TEXT("retained bytes respect the global cap"), Pool.RetainedBytes() <= 128);
	Memory26::TPool<FBytes, 4, 256> Fit;
	auto Large = MakeUnique<FBytes>();
	Large->Data.Reserve(128);
	auto Small = MakeUnique<FBytes>();
	Small->Data.Reserve(32);
	// Return deliberately in the opposite order to each upcoming request.
	Fit.Return(MoveTemp(Large));
	Fit.Return(MoveTemp(Small));
	auto Batch = Fit.Take(100);
	TestTrue(TEXT("large batch chooses an already fitting array"), Batch->Bytes() >= 100);
	auto Member = Fit.Take(16);
	TestTrue(TEXT("sensor member does not steal batch capacity"), Member->Bytes() < 100);
	auto Rays = SensorOwnedRayTracing::TakeRayArray();
	Rays.SetNumUninitialized(1024, false);
	auto* Storage = Rays.GetData();
	const auto Capacity = Rays.GetAllocatedSize();
	Rays[0].Origin = FVector3f(1, 2, 3);
	SensorOwnedRayTracing::RecycleRayArray(MoveTemp(Rays));
	auto Reused = SensorOwnedRayTracing::TakeRayArray();
	TestEqual(TEXT("returned storage has no logical admissions"), Reused.Num(), 0);
	TestTrue(TEXT("CPU admission allocation is reused"), Reused.GetData() == Storage);
	Reused.SetNumUninitialized(513, false);
	Reused.SetNumUninitialized(1024, false);
	TestEqual(TEXT("smaller then larger admissions retain high-water capacity"), Reused.GetAllocatedSize(), Capacity);
	SensorOwnedRayTracing::RecycleRayArray(MoveTemp(Reused));
	return true;
}
#endif

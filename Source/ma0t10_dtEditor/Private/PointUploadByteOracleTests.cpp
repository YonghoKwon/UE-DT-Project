#include "Device/Sensor/LidarPlayerDisplayComp.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "SensorUploadDiagnostics.h"
#include "Memory26Diagnostics.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPointUploadByteOracleTest, "MA0T10.Memory.PointUploadByteOracleRhi", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPointUploadByteOracleTest::RunTest(const FString&)
{
	if (GUsingNullRHI)
	{
		AddError(TEXT("Actual RHI required"));
		return false;
	}
	auto* World = FAutomationEditorCommonUtils::CreateNewMap();
	auto* Actor = World->SpawnActor<AActor>();
	Actor->Tags.Add(TEXT("SensorTestManaged"));
	auto* C = NewObject<ULidarPlayerDisplayComp>(Actor);
	Actor->AddInstanceComponent(C);
	Actor->SetRootComponent(C);
	C->bFixedPointSubmission = true;
	C->PointPixelScale = 1. / 1024;
	C->BatchedPoints.Emplace(FVector(128, 8, 16), FLinearColor(.125f, .5f, .75f, 1), 2.f, 0.f, SDPG_World);
	C->BatchedPoints.Emplace(FVector(256, -16, 32), FLinearColor(.5f, .25f, 1, 1), 4.f, 0.f, SDPG_World);
	C->RegisterComponent();
	C->CommitPointData();
	World->SendAllEndOfFrameUpdates();
	FlushRenderingCommands();
	const uint64 Creations = C->GetFixedPointProxyCreations();
	const FIntVector Resources = C->GetFixedPointRhiResources();
	TestEqual(TEXT("only one position/color/tangent/UV/index set"), Resources.X, 5);
	AddInfo(FString::Printf(TEXT("Point GPU buffers=%d SRVs=%d bytes=%d for%dpoints"), Resources.X, Resources.Y, Resources.Z, C->BatchedPoints.Num()));
	auto Verify = [&](const TCHAR* Label)
	{
		TArray<uint8> ExpectedPositions, ExpectedColors, Positions, Colors;
		for (const auto& P : C->BatchedPoints)
		{
			const double Depth = FVector::DotProduct(P.Position - C->PointViewOrigin, C->PointViewForward);
			const double Size = Depth > 0 && P.PointSize > 0 && P.Color.A > 0 ? P.PointSize * Depth * C->PointPixelScale : 0;
			const FVector X = C->PointViewRight * Size, Y = C->PointViewUp * Size;
			for (auto Corner : {FVector2D(-1, -1), FVector2D(1, -1), FVector2D(1, 1), FVector2D(-1, 1)})
			{
				const FVector3f Position(P.Position + X * Corner.X + Y * Corner.Y);
				const FColor Color = P.Color.ToFColor(true);
				ExpectedPositions.Append(reinterpret_cast<const uint8*>(&Position), sizeof(Position));
				ExpectedColors.Append(reinterpret_cast<const uint8*>(&Color), sizeof(Color));
			}
		}
		const bool Read = C->ReadFixedPointStreamsForTests(Positions, Colors);
		TestTrue(FString(Label) + TEXT(" actual GPU readback"), Read);
		if (Read)
		{
			TestTrue(FString(Label) + TEXT(" position bytes equal original quad formula"), Positions == ExpectedPositions);
			TestTrue(FString(Label) + TEXT(" color bytes equal original quantization"), Colors == ExpectedColors);
		}
		TestEqual(FString(Label) + TEXT(" persistent resource count/capacity"), C->GetFixedPointRhiResources(), Resources);
		TestEqual(FString(Label) + TEXT(" proxy persists"), C->GetFixedPointProxyCreations(), Creations);
	};
	auto Submit = [&]()
	{
		C->CommitPointData();
		World->SendAllEndOfFrameUpdates();
		FlushRenderingCommands();
	};
	Verify(TEXT("initial"));
	SensorUploadDiagnostics::ResetWindow();
	Submit();
	Verify(TEXT("identical"));
	if (SensorUploadDiagnostics::IsEnabled())
	{
		const auto S = SensorUploadDiagnostics::Snapshot();
		TestEqual(TEXT("identical data makes no RHI copy"), S.Sites[uint32(SensorUploadDiagnostics::ESite::PreviewPosition)].CopyCalls + S.Sites[uint32(SensorUploadDiagnostics::ESite::PreviewColor)].CopyCalls, uint64(0));
	}
	C->BatchedPoints[0].Color = FLinearColor::Red;
	Submit();
	Verify(TEXT("color change"));
	C->BatchedPoints[1].Position.Y += 16;
	Submit();
	Verify(TEXT("position change"));
	C->PointViewOrigin = FVector(8, 16, 32);
	C->PointViewRight = FVector(0, 0, 1);
	C->PointViewUp = FVector(0, -1, 0);
	C->PointPixelScale = 1. / 512;
	Submit();
	Verify(TEXT("view and pixel scale change"));
	C->BatchedPoints[0].Color.A = 0;
	C->BatchedPoints[1].PointSize = 0;
	Submit();
	Verify(TEXT("zero-area slots"));
	for (int32 I = 0; I < 12; ++I)
	{
		C->BatchedPoints[0].Position.Z += 1;
		Submit();
		Verify(TEXT("repeated updates"));
	}
	const auto Pool = C->GetPreviewScratchPoolForTests();
	TestTrue(TEXT("scratch free count bounded4"), Pool.X <= 4);
	TestTrue(TEXT("scratch free bytes bounded8MiB"), Pool.Y <= 8 * 1024 * 1024);
	if (Memory26::GetCase() == Memory26::ECase::None)
		TestTrue(TEXT("default path returns reusable exclusive scratch"), Pool.X > 0);
	Actor->Destroy();
	FlushRenderingCommands();
	return true;
}
#endif

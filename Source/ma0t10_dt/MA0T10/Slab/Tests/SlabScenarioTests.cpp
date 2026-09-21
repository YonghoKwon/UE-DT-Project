#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioCodec.h"
#include "ma0t10_dt/MA0T10/Slab/SlabMetricsComponent.h"
#include "Json.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabScenarioParserTest,"MA0T10.SlabScenario.Parser",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabScenarioParserTest::RunTest(const FString&)
{
	const FString Json=FSlabScenarioCodec::MakeSyntheticJson(); FSlabScenarioDataPtr Data; FString Error;
	TestTrue(TEXT("600-row bulk parses"),FSlabScenarioCodec::Parse(Json,Data,Error));
	if(!Data) { AddError(Error); return false; }
	TestEqual(TEXT("original JSON retained byte-for-character"),Data->OriginalJson,Json);
	TestEqual(TEXT("600 rows"),Data->Rows.Num(),600); TestEqual(TEXT("ends at 30 rather than 29.95"),Data->DurationSec,30.0);
	TestEqual(TEXT("anchor0"),Data->Rows[0].CenterX,0.0); TestEqual(TEXT("anchor1"),Data->Rows[1].CenterX,1.55);
	TestEqual(TEXT("anchor580"),Data->Rows[580].CenterX,909.0); TestEqual(TEXT("final hold"),Data->Rows[599].CenterX,909.0);
	TestEqual(TEXT("source dimension not silently converted"),Data->Rows[0].Length,10830.0);
	FSlabScenarioDataPtr Missing,Repeat;
	const FString MissingJson=FSlabScenarioCodec::MakeSyntheticJson(FString());
	TestFalse(TEXT("strict API rejects missing UUID"),FSlabScenarioCodec::Parse(MissingJson,Missing,Error));
	TestTrue(TEXT("explicit production policy accepts missing UUID"),FSlabScenarioCodec::Parse(MissingJson,Missing,Error,true));
	TestTrue(TEXT("internal identity repeat parses"),FSlabScenarioCodec::Parse(MissingJson,Repeat,Error,true));
	if(Missing&&Repeat) { FGuid Id; TestTrue(TEXT("internal identity valid GUID"),FGuid::Parse(Missing->ScenarioUUID,Id)); TestEqual(TEXT("content identity stable"),Repeat->ScenarioUUID,Missing->ScenarioUUID); TestTrue(TEXT("internal identity is labelled"),Missing->bGeneratedArchiveId); TestEqual(TEXT("missing UUID original not rewritten"),Missing->OriginalJson,MissingJson); }
	TSharedPtr<FJsonObject> Root; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root);
	auto Serialize=[&](){FString Value; FJsonSerializer::Serialize(Root.ToSharedRef(),TJsonWriterFactory<>::Create(&Value)); return Value;};
	Root->GetArrayField(TEXT("DATA_MAP"))[1]->AsObject()->SetNumberField(TEXT("frame_no"),0);
	TestFalse(TEXT("duplicate frame rejected"),FSlabScenarioCodec::Parse(Serialize(),Data,Error));
	Root->GetArrayField(TEXT("DATA_MAP"))[1]->AsObject()->SetNumberField(TEXT("frame_no"),1);
	Root->GetArrayField(TEXT("DATA_MAP"))[1]->AsObject()->SetNumberField(TEXT("slab_wth"),1000);
	TestFalse(TEXT("dimensions cannot change during fixed slab run"),FSlabScenarioCodec::Parse(Serialize(),Data,Error));
	Root->GetArrayField(TEXT("DATA_MAP"))[1]->AsObject()->SetNumberField(TEXT("slab_wth"),1100);
	Root->GetObjectField(TEXT("_meta"))->SetNumberField(TEXT("duration_sec"),29);
	TestFalse(TEXT("end before final row rejected"),FSlabScenarioCodec::Parse(Serialize(),Data,Error));
	Root->GetObjectField(TEXT("_meta"))->RemoveField(TEXT("duration_sec")); Root->GetObjectField(TEXT("_meta"))->SetNumberField(TEXT("sample_period_sec"),0.1);
	TestTrue(TEXT("period determines final hold"),FSlabScenarioCodec::Parse(Serialize(),Data,Error));
	if(Data) TestTrue(TEXT("fallback duration 30.05"),FMath::IsNearlyEqual(Data->DurationSec,30.05,1e-8));
	TestFalse(TEXT("invalid JSON rejected"),FSlabScenarioCodec::Parse(TEXT("{broken"),Data,Error));
	TestFalse(TEXT("oversized JSON rejected"),FSlabScenarioCodec::Parse(FString::ChrN(8*1024*1024+1,TEXT('x')),Data,Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabScenarioInterpolationTest,"MA0T10.SlabScenario.Interpolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabScenarioInterpolationTest::RunTest(const FString&)
{
	FSlabScenarioDataPtr D; FString Error; FSlabScenarioCodec::Parse(FSlabScenarioCodec::MakeSyntheticJson(),D,Error);
	if(!D) return false;
	FSlabScenarioRow R; int32 Index=-1;
	TestTrue(TEXT("half-frame sample valid"),FSlabScenarioCodec::Sample(*D,0.025,R,Index));
	TestTrue(TEXT("position exact midpoint not FInterp lag"),FMath::IsNearlyEqual(R.CenterX,0.775,1e-9));
	TestTrue(TEXT("yaw midpoint"),FMath::IsNearlyEqual(R.LeftAngle,-0.1925,1e-9));
	TestEqual(TEXT("source frame retained independently of render rate"),R.FrameNo,int64(0));
	TestEqual(TEXT("source timestamp stays paired"),R.ElapsedSec,0.0);
	FSlabScenarioCodec::Sample(*D,0.05,R,Index); TestEqual(TEXT("exact next boundary"),R.FrameNo,int64(1));
	FSlabScenarioCodec::Sample(*D,30,R,Index); TestEqual(TEXT("last frame held at finish"),R.FrameNo,int64(599)); TestEqual(TEXT("last center held"),R.CenterX,909.0);
	FSlabScenarioData Short; FSlabScenarioRow A,B; A.FrameNo=0; A.LeftAngle=179; B.FrameNo=5; B.ElapsedSec=0.25; B.LeftAngle=-179; B.CenterX=10; Short.Rows={A,B};
	FSlabScenarioCodec::Sample(Short,0.125,R,Index); TestTrue(TEXT("rotation takes shortest path"),FMath::IsNearlyEqual(FMath::Abs(R.LeftAngle),180.0,1e-9));
	TestEqual(TEXT("skipped input rows do not manufacture frame IDs"),R.FrameNo,int64(0));
	TestEqual(TEXT("centimetres default"),FSlabScenarioCodec::ToCm(250,ESlabInputUnit::Centimeters),250.0);
	TestEqual(TEXT("millimetres opt-in"),FSlabScenarioCodec::ToCm(250,ESlabInputUnit::Millimeters),25.0);
	TestEqual(TEXT("metres opt-in"),FSlabScenarioCodec::ToCm(1,ESlabInputUnit::Meters),100.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabScenarioMetricsTest,"MA0T10.SlabScenario.Metrics",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabScenarioMetricsTest::RunTest(const FString&)
{
	FSlabScenarioRow R; const FVector Size(1000,100,25);
	const FTransform Track(FRotator(0,32,0),FVector(350,-120,50));
	FTransform Pose(Track.GetRotation(),Track.TransformPositionNoScale(FVector(200,0,12.5)));
	auto M=USlabMetricsComponent::Calculate(R,Pose,Size,Track,-100,100,true);
	TestTrue(TEXT("valid parallel rails"),M.bMarginsValid); TestTrue(TEXT("left margin 50cm"),FMath::IsNearlyEqual(M.MarginLeftCm,50.0,1e-6));
	TestTrue(TEXT("right margin 50cm"),FMath::IsNearlyEqual(M.MarginRightCm,50.0,1e-6)); TestTrue(TEXT("forward travel is not center offset"),FMath::IsNearlyZero(M.CenterOffsetCm,1e-6));
	R.LeftAngle=-10; Pose.SetRotation(Track.GetRotation()*FRotator(0,R.LeftAngle,0).Quaternion());
	M=USlabMetricsComponent::Calculate(R,Pose,Size,Track,-100,100,true);
	const double Extent=500*FMath::Sin(FMath::DegreesToRadians(10.0))+50*FMath::Cos(FMath::DegreesToRadians(10.0));
	TestTrue(TEXT("rotated corner analytic gap"),FMath::IsNearlyEqual(M.MarginLeftCm,100-Extent,1e-6));
	TestTrue(TEXT("intrusion uses negative signed clearance"),M.MarginLeftCm<0); TestTrue(TEXT("rotation keeps centerline offset zero"),FMath::IsNearlyZero(M.CenterOffsetCm,1e-6));
	M=USlabMetricsComponent::Calculate(R,Pose,Size,Track,-100,100,false); TestFalse(TEXT("missing rails N/A rather than fabricated distance"),M.bMarginsValid);
	return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/UI/SlabScenarioChartWidget.h"
#include "ma0t10_dt/MA0T10/UI/SlabChartsPanelWidget.h"
#include "ma0t10_dt/MA0T10/Slab/SlabScenarioTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabProgressiveChartTest,"MA0T10.Slab.Chart.ProgressiveNoFutureLeak",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabProgressiveChartTest::RunTest(const FString&)
{
	auto Data=MakeShared<TArray<FSlabChartSample>>();
	for(int32 I=0;I<600;++I)
	{
		auto& S=Data->AddDefaulted_GetRef();S.Time=I*.05;S.Frame=I;S.LeftAngle=-.2;S.RightAngle=.2;S.bMarginsValid=true;S.MarginLeftCm=10;S.MarginRightCm=10;
	}
	(*Data)[500].LeftAngle=-10000;(*Data)[500].MarginLeftCm=-10000;
	const auto TimeDomain=USlabScenarioChartWidget::CalculateTimelineDomain(*Data,false,30);
	const auto FrameDomain=USlabScenarioChartWidget::CalculateTimelineDomain(*Data,true,30);
	TestTrue(TEXT("complete 0..30 second axis remains fixed before reveal"),TimeDomain.Equals(FVector2D(0,30),1.e-6));
	TestTrue(TEXT("frame axis includes final 0.05 second hold"),FrameDomain.Equals(FVector2D(0,600),1.e-6));
	TestEqual(TEXT("unstarted prefix empty"),USlabScenarioChartWidget::CountRevealedSamples(*Data,-1),0);
	TestEqual(TEXT("first frame visible at actual start"),USlabScenarioChartWidget::CountRevealedSamples(*Data,0),1);
	TestEqual(TEXT("5 sec reveals only 0..100"),USlabScenarioChartWidget::CountRevealedSamples(*Data,5),101);
	bool Found=false;
	const auto Range=USlabScenarioChartWidget::CalculateVisibleYRange(*Data,ESlabChartMetric::Angles,false,0,30,5,true,true,Found);
	TestTrue(TEXT("future outlier cannot expand auto Y range"),Found&&Range.X>-.5&&Range.Y<.5);
	const auto FrameRange=USlabScenarioChartWidget::CalculateVisibleYRange(*Data,ESlabChartMetric::Angles,true,0,600,5,true,true,Found);
	TestTrue(TEXT("frame-axis auto Y range also rejects future outlier"),Found&&FrameRange.Equals(Range,1.e-6));
	const auto Indices=USlabScenarioChartWidget::BuildMinMaxIndices(*Data,ESlabChartMetric::Angles,0,false,0,30,60,5);
	TestTrue(TEXT("future outlier absent from line decimation"),!Indices.Contains(500));
	for(int32 I:Indices)TestTrue(TEXT("every emitted point is revealed"),(*Data)[I].Time<=5);
	TestEqual(TEXT("future hover does not expose hidden values"),USlabScenarioChartWidget::FindNearestRevealedSample(*Data,false,25,5),INDEX_NONE);
	TestEqual(TEXT("frame-axis future hover also blocked"),USlabScenarioChartWidget::FindNearestRevealedSample(*Data,true,500,5),INDEX_NONE);
	TestEqual(TEXT("past hover resolves actual source row"),USlabScenarioChartWidget::FindNearestRevealedSample(*Data,false,2,5),40);
	const auto FutureWindow=USlabScenarioChartWidget::CalculateVisibleYRange(*Data,ESlabChartMetric::RailMargins,false,24,26,5,true,true,Found);
	TestFalse(TEXT("panning into future does not produce data"),Found);
	USlabScenarioChartWidget::CalculateVisibleYRange(*Data,ESlabChartMetric::RailMargins,true,480,520,5,true,true,Found);
	TestFalse(TEXT("frame-axis future window also empty"),Found);
	const auto RevealedRange=USlabScenarioChartWidget::CalculateVisibleYRange(*Data,ESlabChartMetric::Angles,false,0,30,25,true,true,Found);
	TestTrue(TEXT("outlier appears when actual timeline reaches it"),Found&&RevealedRange.X<=-10000);

	auto* A=NewObject<USlabScenarioChartWidget>();auto* B=NewObject<USlabScenarioChartWidget>();auto* C=NewObject<USlabScenarioChartWidget>();
	A->SetSharedSamples(Data);B->SetSharedSamples(Data);C->SetSharedSamples(Data);
	TestTrue(TEXT("three charts share immutable data rather than copy"),Data.GetSharedReferenceCount()>=4);
	TestFalse(TEXT("generic chart defaults remain non-progressive"),A->IsProgressiveRevealEnabled());
	TestEqual(TEXT("unrelated caller retains complete data behavior"),A->GetRevealedSampleCount(),600);
	auto* Legacy=NewObject<USlabScenarioChartWidget>();Legacy->SetSamples(*Data);Legacy->SetRevealedTime(5);
	TestEqual(TEXT("legacy SetSamples remains full dataset unless explicitly opted in"),Legacy->GetRevealedSampleCount(),600);
	for(auto* Chart:{A,B,C}){Chart->SetProgressiveReveal(true);Chart->SetRevealedTime(5);}
	TestEqual(TEXT("three progressive charts share same revealed count"),A->GetRevealedSampleCount(),B->GetRevealedSampleCount());
	A->SetRevealedTime(5);TestEqual(TEXT("pause does not advance prefix"),A->GetRevealedSampleCount(),101);
	A->SetRevealedTime(12.35);TestEqual(TEXT("hidden view can restore current prefix immediately"),A->GetRevealedSampleCount(),248);
	A->SetRevealedTime(12.35);TestEqual(TEXT("abort preserves last prefix"),A->GetRevealedSampleCount(),248);
	A->SetRevealedTime(-1);TestEqual(TEXT("new RunUUID reset can clear prefix without changing archive"),A->GetRevealedSampleCount(),0);
	A->SetRevealedTime(0);TestEqual(TEXT("new run starts at first source row"),A->GetRevealedSampleCount(),1);
	A->SetRevealedTime(30);TestEqual(TEXT("completed scenario exposes all original rows"),A->GetRevealedSampleCount(),600);
	TestEqual(TEXT("source data remains untouched"),(*Data)[500].LeftAngle,-10000.0);
	TArray<FSlabScenarioRow> Rows;Rows.SetNum(600);
	for(int32 I=0;I<600;++I){Rows[I].ElapsedSec=I*.05;Rows[I].CenterX=I;}
	Rows[500].CenterX=1000000;
	TestEqual(TEXT("first speed does not inspect future row"),USlabChartsPanelWidget::CalculateHistoricalSpeed(Rows,0,1),0.0);
	TestTrue(TEXT("future position anomaly cannot contaminate current speed"),FMath::IsNearlyEqual(USlabChartsPanelWidget::CalculateHistoricalSpeed(Rows,499,1),20.0,1.e-6));
	TestTrue(TEXT("anomaly visible only when its current row arrives"),USlabChartsPanelWidget::CalculateHistoricalSpeed(Rows,500,1)>1000000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabThreeChartSlotsTest,"MA0T10.Slab.Chart.ThreeSlotsAndLegacyApi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabThreeChartSlotsTest::RunTest(const FString&)
{
	auto* P=NewObject<USlabChartsPanelWidget>();
	TestEqual(TEXT("first defaults to angles"),P->GetChartSlotMetric(0),ESlabChartMetric::Angles);
	TestEqual(TEXT("second defaults to center offset"),P->GetChartSlotMetric(1),ESlabChartMetric::CenterOffset);
	TestEqual(TEXT("third defaults to margins"),P->GetChartSlotMetric(2),ESlabChartMetric::RailMargins);
	P->SetChartMetric(ESlabChartMetric::Speed);
	TestEqual(TEXT("legacy API selects first chart"),P->GetChartSlotMetric(0),ESlabChartMetric::Speed);
	TestEqual(TEXT("legacy API does not change second"),P->GetChartSlotMetric(1),ESlabChartMetric::CenterOffset);
	P->SetChartSlotMetric(2,ESlabChartMetric::CornerOffset);
	TestEqual(TEXT("independent third metric"),P->GetChartSlotMetric(2),ESlabChartMetric::CornerOffset);
	P->SetChartSlotMetric(-1,ESlabChartMetric::Angles);P->SetChartSlotMetric(3,ESlabChartMetric::Angles);
	TestEqual(TEXT("invalid slot does not affect first"),P->GetChartSlotMetric(0),ESlabChartMetric::Speed);
	return true;
}
#endif

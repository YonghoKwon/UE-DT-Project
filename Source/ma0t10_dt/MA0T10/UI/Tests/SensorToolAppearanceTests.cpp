#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/TextBlock.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiHostActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorFontIsolationTest, "MA0T10.SensorControl.ToolFontIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSensorFontIsolationTest::RunTest(const FString& Parameters)
{
	auto* Host = NewObject<AVirtualSensorUiHostActor>();
	auto* Own = NewObject<UVirtualSensorSettingsPanelWidget>();
	auto* Other = NewObject<UVirtualSensorSettingsPanelWidget>();
	auto* OwnText = NewObject<UTextBlock>(Own);
	auto* OtherText = NewObject<UTextBlock>(Other);
	FSlateFontInfo Font = OwnText->GetFont(); Font.Size = 20;
	OwnText->SetFont(Font); OtherText->SetFont(Font);
	Own->RegisterSensorTextControl(OwnText);
	Own->RegisterSensorTextControl(OtherText); // Reject controls owned by a nested/other widget.
	Other->RegisterSensorTextControl(OtherText);
	Own->SetSensorAppearanceOwner(Host);
	for (float Scale : {0.85f, 1.0f, 1.25f, 1.5f, 1.0f})
	{
		Own->ApplySensorToolFontScale(Scale);
		Other->ApplySensorToolFontScale(Scale);
		TestEqual(TEXT("owned font uses original baseline"), OwnText->GetFont().Size, static_cast<float>(FMath::RoundToInt(20 * Scale)));
		TestEqual(TEXT("same-parent unregistered panel stays unchanged"), OtherText->GetFont().Size, 20.0f);
	}
	auto NativeText = SNew(STextBlock).Font(Font).Text(FText::FromString(TEXT("Sensor")));
	auto Foreign = SNew(STextBlock).Font(Font).Text(FText::FromString(TEXT("Slab chart")));
	Foreign->SlatePrepass(1.0f);
	const FVector2D ForeignSize = Foreign->GetDesiredSize();
	Own->RegisterSensorNativeFont(NativeText, STextBlock::FArguments());
	Own->ApplySensorToolFontScale(1.5f);
	Foreign->SlatePrepass(1.0f);
	TestEqual(TEXT("native registered label scales"), NativeText->GetFont().Size, 30.0f);
	TestEqual(TEXT("foreign geometry unchanged"), FVector2D(Foreign->GetDesiredSize()), ForeignSize);
	return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/TextBlock.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorSettingsPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorUiHostActor.h"
#include "UObject/UnrealType.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolAppearance.h"

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
	const auto* SettingsProperty = FindFProperty<FObjectPropertyBase>(Host->GetClass(), TEXT("SettingsWidget"));
	TestNotNull(TEXT("host settings ownership slot exists"), SettingsProperty);
	if (!SettingsProperty) return false;
	SettingsProperty->SetObjectPropertyValue_InContainer(Host, Own);
	for (const FName Name : {FName(TEXT("SetGlobalSensorUiFontScale")), FName(TEXT("GetGlobalSensorUiFontScale")),
		FName(TEXT("ResetGlobalSensorUiFontScale")), FName(TEXT("OnSensorUiFontScaleChanged"))})
		TestNotNull(TEXT("legacy Blueprint function resolves"), Own->FindFunction(Name));
	Own->SetGlobalSensorUiFontScale(1.25f);
	TestEqual(TEXT("legacy setter scales only host owned controls"), OwnText->GetFont().Size, 25.0f);
	Other->SetGlobalSensorUiFontScale(1.5f);
	TestEqual(TEXT("foreign legacy setter cannot change host scale"), Host->GetSensorToolFontScale(), 1.25f);
	TestEqual(TEXT("foreign legacy getter has no global state"), Other->GetGlobalSensorUiFontScale(), 1.0f);
	TestEqual(TEXT("foreign control remains unchanged through compatibility API"), OtherText->GetFont().Size, 20.0f);
	Own->ResetGlobalSensorUiFontScale();
	TestEqual(TEXT("legacy reset restores baseline"), OwnText->GetFont().Size, 20.0f);
	Own->SetGlobalSensorUiFontScale(1.5f);
	Host->ResetSensorToolAppearance(); // the font part of ResetAll, without deleting user panel saves
	TestEqual(TEXT("appearance reset restores live scale"), OwnText->GetFont().Size, 20.0f);
	TestEqual(TEXT("appearance reset persists default for next launch"), USensorToolAppearance::LoadScale(), 1.0f);
	TestEqual(TEXT("appearance reset does not change colleague widget"), OtherText->GetFont().Size, 20.0f);
	return true;
}
#endif

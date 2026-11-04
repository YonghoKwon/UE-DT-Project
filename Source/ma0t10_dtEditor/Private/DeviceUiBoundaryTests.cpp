#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UnrealType.h"
#include "Kismet/GameplayStatics.h"
#include "DeviceUI/DxDeviceSensorPanelBase.h"
#include "DeviceUI/DevicePanelBodyCall.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/DxWidgetVirtualSensorPanelBase.h"
#include "ma0t10_dt/MA0T10/UI/Freeview/DxWidgetVirtualSensorMonitorPanel.h"
#include "ma0t10_dt/MA0T10/UI/Common/VirtualSensorUiPreferences.h"

namespace
{
class FDeviceBoundaryHost final : public IDevicePanelHostBridge
{
public:
	bool IsOwned() const override { return true; }
	float PixelScale() const override { return 1.5f; }
	bool NavigateBack() override { return false; }
	FText BackLabel() const override { return FText::GetEmpty(); }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceUiWeakHostLifetime, "MA0T10.DeviceBoundary.UiWeakHostLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceUiWeakHostLifetime::RunTest(const FString&)
{
	auto* Panel = NewObject<UDxWidgetVirtualSensorMonitorPanel>();
	Panel->AddToRoot();
	TWeakObjectPtr<UObject> Owner = NewObject<UVirtualSensorUiPreferencesSaveGame>();
	Panel->BindDevicePanelHost(Owner.Get(), MakeShared<FDeviceBoundaryHost>());
	TestTrue(TEXT("explicit host owns the panel"), Panel->IsDeviceHostOwned());
	int32 Calls = 0;
	TestEqual(TEXT("live body callback"), DevicePanelBody::Invoke(Owner, [&Calls](UObject*)
	                                                              {++Calls;return 7; }),
	          7);
	CollectGarbage(RF_NoFlags);
	TestFalse(TEXT("bridge does not retain host UObject"), Owner.IsValid());
	TestFalse(TEXT("expired bridge is unavailable"), Panel->IsDeviceHostOwned());
	TestEqual(TEXT("expired body callback fallback"), DevicePanelBody::Invoke(Owner, [&Calls](UObject*)
	                                                                          {++Calls;return 7; }),
	          0);
	TestEqual(TEXT("expired callback never mutates host state"), Calls, 1);
	const FReply Reply = DevicePanelBody::Invoke(Owner, [](UObject*)
	                                             { return FReply::Handled(); });
	TestFalse(TEXT("expired input callback does not consume input"), Reply.IsEventHandled());
	Panel->BindDevicePanelHost(nullptr, nullptr);
	Panel->RemoveFromRoot();
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceUiFacadePersistence, "MA0T10.DeviceBoundary.UiFacadePersistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDeviceUiFacadePersistence::RunTest(const FString&)
{
	UClass* Legacy = UDxWidgetVirtualSensorPanelBase::StaticClass();
	for (const TCHAR* Name : {TEXT("bPanelDraggable"), TEXT("bClampPanelToViewport"), TEXT("bPanelResizable"), TEXT("DragHandleHeight"), TEXT("ResizeHandleSize")})
	{
		auto* Property = FindFProperty<FProperty>(Legacy, Name);
		TestNotNull(Name, Property);
		if (Property)
			TestEqual(TEXT("public property stays on facade"), Property->GetOwnerStruct(), static_cast<UStruct*>(Legacy));
	}
	TestEqual(TEXT("legacy slot class path"), UVirtualSensorUiPreferencesSaveGame::StaticClass()->GetPathName(), FString(TEXT("/Script/ma0t10_dt.VirtualSensorUiPreferencesSaveGame")));
	auto* Preferences = NewObject<UVirtualSensorUiPreferencesSaveGame>();
	Preferences->LidarHeightMinMeters = 12.5f;
	Preferences->LidarHeightMaxMeters = 27.75f;
	TArray<uint8> Bytes;
	TestTrue(TEXT("SaveGame in-memory serialization"), UGameplayStatics::SaveGameToMemory(Preferences, Bytes));
	auto* Restored = Cast<UVirtualSensorUiPreferencesSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (TestNotNull(TEXT("same legacy SaveGame type"), Restored))
	{
		TestEqual(TEXT("saved range minimum unchanged"), Restored->LidarHeightMinMeters, 12.5f);
		TestEqual(TEXT("saved range maximum unchanged"), Restored->LidarHeightMaxMeters, 27.75f);
	}
	const FVector2D Drag = UDxDeviceSensorPanelBase::DeviceDraggedPosition(FVector2D(100, 200), FVector2D(30, 60), 1.5f);
	TestTrue(TEXT("DPI drag math unchanged"), Drag.Equals(FVector2D(120, 240), .001));
	TestEqual(TEXT("font clamp unchanged"), UDxDeviceSensorPanelBase::DeviceScaledFontSize(20, .5f), 17);
	return true;
}
#endif

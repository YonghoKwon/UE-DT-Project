#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"
#include "NiagaraSystem.h"
#include "Device/Core/DeviceSensorAssetSettings.h"
#include "Device/Sensor/VirtualLidarVisualizationComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeviceExplicitAssetPaths,"MA0T10.DeviceBoundary.ExplicitAssetPaths",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeviceExplicitAssetPaths::RunTest(const FString&)
{
    auto* Settings=GetMutableDefault<UDeviceSensorAssetSettings>();
    TestEqual(TEXT("semantic material path unchanged"),Settings->LidarSemanticMaterial.ToSoftObjectPath().ToString(),FString(TEXT("/Game/MA0T10/Sensor/Materials/M_LidarSemanticId.M_LidarSemanticId")));
    TestEqual(TEXT("preview path unchanged"),Settings->LidarPointCloudSystem.ToSoftObjectPath().ToString(),FString(TEXT("/Game/MA0T10/Sensor/VFX/NS_VirtualLidarPointCloud.NS_VirtualLidarPointCloud")));
    const auto Previous=Settings->LidarPointCloudSystem;
    ON_SCOPE_EXIT {Settings->LidarPointCloudSystem=Previous;};
    Settings->LidarPointCloudSystem=TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/DeviceBoundary/ExplicitHostPath.ExplicitHostPath")));
    auto* Component=NewObject<UVirtualLidarVisualizationComponent>();
    auto* Property=FindFProperty<FSoftObjectProperty>(Component->GetClass(),TEXT("NiagaraPointCloudSystem"));
    if(!TestNotNull(TEXT("legacy per-actor soft property preserved"),Property))return false;
    FString Value;Property->ExportText_InContainer(0,Value,Component,Component,Component,PPF_None);
    TestTrue(TEXT("explicit host default copied without loading or saving an asset"),Value.Contains(TEXT("/Game/DeviceBoundary/ExplicitHostPath.ExplicitHostPath")));
    Settings->LidarPointCloudSystem=Previous;
    Value.Reset();Property->ExportText_InContainer(0,Value,Component,Component,Component,PPF_None);
    TestTrue(TEXT("existing actor setting is not overwritten by later default changes"),Value.Contains(TEXT("/Game/DeviceBoundary/ExplicitHostPath.ExplicitHostPath")));
    return true;
}
#endif

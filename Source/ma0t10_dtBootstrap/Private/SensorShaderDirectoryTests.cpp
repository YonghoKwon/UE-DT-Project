#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "SensorOwnedRayTracing.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorShaderDirectoryTest,"MA0T10.DevicePlugin.ShaderDirectory",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorShaderDirectoryTest::RunTest(const FString&)
{
    const FString Dir=SensorOwnedRayTracing::ShaderSourceDirectory();
    TestTrue(TEXT("mapped folder carries the native shader source"),IFileManager::Get().FileExists(*FPaths::Combine(Dir,TEXT("Private/SensorOwnedRayTracing.usf"))));
    const TSharedPtr<IPlugin> Plugin=IPluginManager::Get().FindPlugin(TEXT("DTDevice"));
    const bool PluginOwnsShader=Plugin&&IFileManager::Get().FileExists(*FPaths::Combine(Plugin->GetBaseDir(),TEXT("Shaders/Private/SensorOwnedRayTracing.usf")));
    // Before the move the project folder is used; after it, only the plugin copy may be mapped.
    TestEqual(TEXT("plugin shaders win only when the plugin carries the source"),FPaths::IsSamePath(Dir,FPaths::Combine(FPaths::ProjectDir(),TEXT("Shaders"))),!PluginOwnsShader);
    return true;
}
#endif

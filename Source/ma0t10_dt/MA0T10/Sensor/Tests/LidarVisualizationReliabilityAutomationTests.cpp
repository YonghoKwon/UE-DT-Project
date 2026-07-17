#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLidarPointCloudRendererDecisionTest,
    "MA0T10.SensorV2.Visualization.RendererDecision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLidarPointCloudRendererDecisionTest::RunTest(const FString& Parameters)
{
    using State = ELidarPointCloudRendererState;
    using Policy = ELidarPointCloudRenderPolicy;

    TestEqual(TEXT("disabled world preview stays disabled"),
        UVirtualLidarVisualizationComponent::ResolveRendererState(Policy::AutoPreferNiagara, false, 100, true, true), State::Disabled);
    TestEqual(TEXT("measurement misses are not visible points"),
        UVirtualLidarVisualizationComponent::ResolveRendererState(Policy::AutoPreferNiagara, true, 0, true, true), State::Starting);
    TestEqual(TEXT("auto uses Niagara only after readiness"),
        UVirtualLidarVisualizationComponent::ResolveRendererState(Policy::AutoPreferNiagara, true, 100, true, true), State::NiagaraActive);
    TestEqual(TEXT("auto falls back when Niagara is not ready"),
        UVirtualLidarVisualizationComponent::ResolveRendererState(Policy::AutoPreferNiagara, true, 100, true, false), State::CpuFallback);
    TestEqual(TEXT("forced CPU never selects Niagara"),
        UVirtualLidarVisualizationComponent::ResolveRendererState(Policy::ForceCpu, true, 100, true, true), State::CpuFallback);
    TestEqual(TEXT("forced Niagara exposes readiness failure"),
        UVirtualLidarVisualizationComponent::ResolveRendererState(Policy::ForceNiagara, true, 100, true, false), State::Error);
    return true;
}

#endif

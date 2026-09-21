#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Slab/SlabVisualizationComponent.h"
#include "Engine/Font.h"
#include "Components/TextRenderComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabReadabilityTest,"MA0T10.SlabAnalysis.ScreenSizingAndReadableAssets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabReadabilityTest::RunTest(const FString&)
{
	using V=USlabVisualizationComponent;
	const float Near=V::CalculateScreenWorldSize(1000,90,720,16./9.,4);
	TestTrue(TEXT("perspective doubles stroke at twice distance"),FMath::IsNearlyEqual(V::CalculateScreenWorldSize(2000,90,720,16./9.,4),Near*2));
	TestTrue(TEXT("pixel control scales linearly"),FMath::IsNearlyEqual(V::CalculateScreenWorldSize(1000,90,720,16./9.,12),Near*3));
	TestEqual(TEXT("orthographic independent of distance"),V::CalculateScreenWorldSize(1000,90,720,16./9.,4,true,1600),V::CalculateScreenWorldSize(30000,90,720,16./9.,4,true,1600));
	auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Slab=World->SpawnActor<ASlabActor>();auto S=Slab->GetAnalysisDisplaySettings();S.bYaw=false;S.LineWidthPixels=999;S.TextHeightPixels=1;Slab->SetAnalysisDisplaySettings(S);
	TestEqual(TEXT("width clamped"),Slab->GetAnalysisDisplaySettings().LineWidthPixels,12.f);TestEqual(TEXT("text clamped"),Slab->GetAnalysisDisplaySettings().TextHeightPixels,18.f);
	Slab->SetDiagnosticHelpersVisible(false);Slab->SetDiagnosticHelpersVisible(true);TestFalse(TEXT("master preserves individual options"),Slab->GetAnalysisDisplaySettings().bYaw);TestEqual(TEXT("master preserves width"),Slab->GetAnalysisDisplaySettings().LineWidthPixels,12.f);
	auto* Font=LoadObject<UFont>(nullptr,TEXT("/Game/MA0T10/Slab/Materials/F_SlabDiagnostics.F_SlabDiagnostics"));TestNotNull(TEXT("portable diagnostic font asset"),Font);
	if(Font){TestTrue(TEXT("Korean intrusion characters have real atlas mappings"),Font->CharRemap.Contains(uint16(TEXT('침')))&&Font->CharRemap.Contains(uint16(TEXT('범'))));float W=0,H=0;Font->GetCharSize(TEXT('침'),W,H);TestTrue(TEXT("Korean glyph has positive extent"),W>0&&H>0);}
	for(const TCHAR* Name:{TEXT("M_SlabAnalysisReadable"),TEXT("M_SlabTextReadable")})
	{auto* Mat=LoadObject<UMaterial>(nullptr,*FString::Printf(TEXT("/Game/MA0T10/Slab/Materials/%s.%s"),Name,Name));TestNotNull(TEXT("owned readable material exists"),Mat);if(Mat)TestTrue(TEXT("diagnostics are unlit"),Mat->GetShadingModels().HasShadingModel(MSM_Unlit));}
	Slab->Destroy();return true;
}
#endif

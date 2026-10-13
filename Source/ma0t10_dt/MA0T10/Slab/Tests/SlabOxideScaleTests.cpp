#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "ma0t10_dt/MA0T10/Slab/SlabActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSurfaceResponseComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlabOxideScaleTest,"MA0T10.UXR08.OxideAssetSwitchRestoreAndOptIn",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSlabOxideScaleTest::RunTest(const FString&)
{
    const FString OxidePath=TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurfaceOxideScale.M_SlabSurfaceOxideScale");
    auto* Oxide=LoadObject<UMaterial>(nullptr,*OxidePath);
    if(!TestNotNull(TEXT("new oxide package loads"),Oxide))return false;
    TArray<FMaterialParameterInfo> Params;TArray<FGuid> Ids;Oxide->GetAllScalarParameterInfo(Params,Ids);
    for(const FName Name:{FName(TEXT("Oxidation")),FName(TEXT("Hotness")),FName(TEXT("Roughness")),FName(TEXT("EmissiveStrength"))})
        TestTrue(TEXT("MID contract parameters exist"),Params.ContainsByPredicate([Name](const auto& I){return I.Name==Name;}));
    auto* World=FAutomationEditorCommonUtils::CreateNewMap();auto* Slab=World->SpawnActor<ASlabActor>();
    auto* Original=Cast<UMaterialInstanceDynamic>(Slab->SlabMesh->GetMaterial(0));
    if(!TestNotNull(TEXT("original MID exists"),Original)){Slab->Destroy();return false;}
    const auto Property=Slab->SurfaceMaterial.ToSoftObjectPath();const bool Hot=Slab->bHotAppearance;
    const float Ox=Slab->Oxidation,Rough=Slab->Roughness,Emissive=Slab->EmissiveStrength;
    Original->SetScalarParameterValue(TEXT("Metallic"),.37f);
    FString Reason;UMaterialInstanceDynamic* OxideMid=nullptr;
    for(int32 Cycle=0;Cycle<3;++Cycle)
    {
        TestTrue(TEXT("oxide application accepted"),Slab->ApplySurfacePreset(ESlabSurfacePreset::OxidationScale,false,Reason));
        auto* Mid=Cast<UMaterialInstanceDynamic>(Slab->SlabMesh->GetMaterial(0));
        if(!TestNotNull(TEXT("oxide MID created"),Mid))break;
        TestTrue(TEXT("oxide MID uses new parent"),Mid->Parent==Oxide);
        if(OxideMid)TestTrue(TEXT("repeated switching reuses oxide MID"),Mid==OxideMid);
        OxideMid=Mid;
        TestEqual(TEXT("effective path reports displayed oxide"),Slab->GetEffectiveSurfaceMaterialPath(),OxidePath);
        TestEqual(TEXT("configured SurfaceMaterial never changes"),Slab->SurfaceMaterial.ToSoftObjectPath(),Property);
        TestEqual(TEXT("UXR09 opaque oxide coverage parameter applied"),Mid->K2_GetScalarParameterValue(TEXT("Oxidation")),1.f);
        TestNull(TEXT("visual-only oxide creates no optical component"),Slab->FindComponentByClass<UVirtualLidarSurfaceResponseComponent>());
        TestTrue(TEXT("same selection is accepted"),Slab->ApplySurfacePreset(ESlabSurfacePreset::OxidationScale,false,Reason));
        TestTrue(TEXT("same selection retains MID"),Slab->SlabMesh->GetMaterial(0)==Mid);
        for(const auto Preset:{ESlabSurfacePreset::HotRed,ESlabSurfacePreset::CooledSteel,ESlabSurfacePreset::AnalysisMatte,ESlabSurfacePreset::Default})
        {
            TestTrue(TEXT("other presets retain original parent"),Slab->ApplySurfacePreset(Preset,false,Reason));
            TestTrue(TEXT("original MID restored exactly"),Slab->SlabMesh->GetMaterial(0)==Original);
        }
    }
    TestEqual(TEXT("original hot state restored"),Slab->bHotAppearance,Hot);
    TestEqual(TEXT("original oxidation restored"),Slab->Oxidation,Ox);
    TestEqual(TEXT("original roughness restored"),Slab->Roughness,Rough);
    TestEqual(TEXT("original emissive restored"),Slab->EmissiveStrength,Emissive);
    TestEqual(TEXT("unrelated original MID override survives"),Original->K2_GetScalarParameterValue(TEXT("Metallic")),.37f);
    auto* Response=NewObject<UVirtualLidarSurfaceResponseComponent>(Slab);Response->RegisterComponent();
    Response->Reflectivity940Nm=.31f;Response->IntensityGain=2;
    TestTrue(TEXT("explicit oxide linkage accepted"),Slab->ApplySurfacePreset(ESlabSurfacePreset::OxidationScale,true,Reason));
    TestEqual(TEXT("oxide optical assumption applied"),Response->Reflectivity940Nm,.12f);
    TestEqual(TEXT("gain preserved"),Response->IntensityGain,2.f);
    Slab->ApplySurfacePreset(ESlabSurfacePreset::CooledSteel,true,Reason);
    TestTrue(TEXT("oxide reflectivity is lower than cooled steel"),.12f<Response->Reflectivity940Nm);
    Slab->ApplySurfacePreset(ESlabSurfacePreset::Default,false,Reason);
    TestEqual(TEXT("opt-out restores response"),Response->Reflectivity940Nm,.31f);
    TestFalse(TEXT("opt-out restores GPU override"),Response->bUseGpuReflectivityOverride);
    Slab->SlabMesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
    auto* CustomSlot=Slab->SlabMesh->GetMaterial(0);
    TestFalse(TEXT("explicit user slot still rejects oxide"),Slab->ApplySurfacePreset(ESlabSurfacePreset::OxidationScale,false,Reason));
    TestTrue(TEXT("user slot unchanged"),Slab->SlabMesh->GetMaterial(0)==CustomSlot);
    Slab->Destroy();return true;
}
#endif

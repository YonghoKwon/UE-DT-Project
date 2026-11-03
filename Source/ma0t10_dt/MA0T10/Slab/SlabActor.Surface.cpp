#include "SlabActor.h"
#include "SlabUiRegistry.h"
#include "ma0t10_dt/MA0T10/Slab/Scenario/SlabRunResultsSubsystem.h"
#include "Device/Sensor/VirtualLidarSurfaceResponseComp.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Device/Sensor/VirtualLidarSensorActor.h"
#include "Device/Sensor/VirtualLidarScanComp.h"

FString ASlabActor::SurfacePresetId(ESlabSurfacePreset P)
{
    switch(P){case ESlabSurfacePreset::HotRed:return TEXT("hot-red");case ESlabSurfacePreset::OxidationScale:return TEXT("oxidation-scale");case ESlabSurfacePreset::CooledSteel:return TEXT("cooled-steel");case ESlabSurfacePreset::AnalysisMatte:return TEXT("analysis-matte");default:return TEXT("default");}
}
void ASlabActor::Click()
{ Super::Click();SlabUiRegistry::Activate(this); }
FSoftObjectPath ASlabActor::GetPresetSurfaceMaterialPath(ESlabSurfacePreset Preset) const
{
    if(Preset==ESlabSurfacePreset::OxidationScale)
        return FSoftObjectPath(TEXT("/Game/MA0T10/Slab/Materials/M_SlabSurfaceOxideScale.M_SlabSurfaceOxideScale"));
    return ResolveSurfaceMaterialPath(SurfaceMaterial.ToSoftObjectPath());
}
bool ASlabActor::CanApplySurfacePreset(ESlabSurfacePreset P,FString& Reason) const
{
    Reason.Empty();if(uint8(P)>uint8(ESlabSurfacePreset::AnalysisMatte)){Reason=TEXT("표면 프리셋을 확인하세요");return false;}
    if(P==ESlabSurfacePreset::Default)return true;
    if(GetExplicitSlotSurfaceMaterial()){Reason=TEXT("사용자 슬롯 재질을 유지합니다 · 이 Slab의 프리셋 적용을 지원하지 않습니다");return false;}
    auto* M=Cast<UMaterialInterface>(GetPresetSurfaceMaterialPath(P).TryLoad());
    if(!M){Reason=TEXT("현재 표면 재질을 읽을 수 없습니다");return false;}
    TArray<FMaterialParameterInfo> Params;TArray<FGuid> Ids;M->GetAllScalarParameterInfo(Params,Ids);
    for(const FName Name:{FName(TEXT("Hotness")),FName(TEXT("Roughness")),FName(TEXT("EmissiveStrength"))})
        if(!Params.ContainsByPredicate([Name](const auto& I){return I.Name==Name;})){Reason=TEXT("현재 재질에 필요한 표면 파라미터가 없습니다");return false;}
    if(P==ESlabSurfacePreset::OxidationScale&&!Params.ContainsByPredicate([](const auto& I){return I.Name==TEXT("Oxidation");}))
    {Reason=TEXT("산화 스케일 재질에 Oxidation 파라미터가 없습니다");return false;}
    return true;
}
bool ASlabActor::ApplySurfacePreset(ESlabSurfacePreset P,bool Link,FString& Reason)
{
    check(IsInGameThread());if(bEnding||IsActorBeingDestroyed()||!CanApplySurfacePreset(P,Reason))return false;
    if(Link&&GetWorld())for(TActorIterator<AVirtualLidarSensorActor> It(GetWorld());It;++It)if(It->ScanComp)
    {
        FString BackendReason;const auto* Scan=It->ScanComp.Get();
        if(Scan->GetResolvedAcquisitionBackend(BackendReason)==EVirtualLidarAcquisitionBackend::GpuDepthProjection&&!Scan->IsSemanticCaptureRequested())
        {Reason=TEXT("현재 GPU 측정에서는 반사율 연동을 적용할 수 없습니다 · 형상 분류 보기 또는 기존 물체 필터를 먼저 선택하세요");return false;}
    }
    if(P==SurfacePreset&&Link==bSurfaceLidarLinked)return true;
    if(!bSurfaceBaselineCaptured)
    {
        bSurfaceBaselineCaptured=true;BaselineHot=bHotAppearance;BaselineOxidation=Oxidation;BaselineRoughness=Roughness;BaselineEmissive=EmissiveStrength;
        if(SurfaceInstance){bHasBaselineColdColor=SurfaceInstance->GetVectorParameterValue(FMaterialParameterInfo(TEXT("ColdColor")),BaselineColdColor);bHasBaselineMetallic=SurfaceInstance->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Metallic")),BaselineSurfaceMetallic);}
    }
    if(Link&&!bSurfaceLidarLinked)
    {
        BaselineSurfaceResponse=FindComponentByClass<UVirtualLidarSurfaceResponseComp>();
        if(BaselineSurfaceResponse.IsValid()){BaselineReflectivity=BaselineSurfaceResponse->Reflectivity940Nm;BaselineGpuOpticalOverride=BaselineSurfaceResponse->bUseGpuReflectivityOverride;}
    }
    bool Hot=BaselineHot;float Ox=BaselineOxidation,Rough=BaselineRoughness,Emissive=BaselineEmissive,Reflect=BaselineSurfaceResponse.IsValid()?BaselineReflectivity:.2f;
    switch(P)
    {
    case ESlabSurfacePreset::HotRed:Hot=true;Rough=.55f;Emissive=4;Reflect=.08f;break;
    case ESlabSurfacePreset::OxidationScale:Hot=false;Ox=1.f;Rough=.72f;Emissive=0;Reflect=.12f;break;
    case ESlabSurfacePreset::CooledSteel:Hot=false;Rough=.22f;Emissive=0;Reflect=.45f;break;
    case ESlabSurfacePreset::AnalysisMatte:Hot=false;Rough=.98f;Emissive=0;Reflect=.18f;break;
    default:break;
    }
    const bool AppearanceChanged=bHotAppearance!=Hot||Oxidation!=Ox||Roughness!=Rough||EmissiveStrength!=Emissive||
        P!=SurfacePreset;
    SurfacePreset=P; // Select the parent before the one-shot appearance update.
    bHotAppearance=Hot;Oxidation=Ox;Roughness=Rough;EmissiveStrength=Emissive;
    if(AppearanceChanged)UpdateAppearance(); // Reuse owned MIDs; one application per explicit change.
    if(Link)
    {
        auto* Response=FindComponentByClass<UVirtualLidarSurfaceResponseComp>();
        if(!Response){OwnedPresetSurfaceResponseComp=NewObject<UVirtualLidarSurfaceResponseComp>(this,MakeUniqueObjectName(this,UVirtualLidarSurfaceResponseComp::StaticClass(),TEXT("SlabPresetOpticalResponse")),RF_Transient);OwnedPresetSurfaceResponseComp->RegisterComponent();Response=OwnedPresetSurfaceResponseComp;}
        Response->Reflectivity940Nm=Reflect;Response->bUseGpuReflectivityOverride=true; // Keep gain/detection/two-sided/saturation settings unchanged.
    }
    else if(bSurfaceLidarLinked)
    {
        if(BaselineSurfaceResponse.IsValid()){BaselineSurfaceResponse->Reflectivity940Nm=BaselineReflectivity;BaselineSurfaceResponse->bUseGpuReflectivityOverride=BaselineGpuOpticalOverride;}
        if(OwnedPresetSurfaceResponseComp){OwnedPresetSurfaceResponseComp->DestroyComponent();OwnedPresetSurfaceResponseComp=nullptr;}
    }
    if(Link||bSurfaceLidarLinked)for(TActorIterator<AVirtualLidarSensorActor> It(GetWorld());It;++It)if(It->ScanComp)It->ScanComp->InvalidateSurfaceResponseCache(); // One-shot adapter; no measurement or restart.
    SurfacePreset=P;bSurfaceLidarLinked=Link;
    RecordSurfaceUsage(Status.RunUUID);OnSlabStateChanged.Broadcast();Reason.Empty();return true;
}
void ASlabActor::RecordSurfaceUsage(const FString& Run)
{
    if(!GetWorld()||Run.IsEmpty())return;
    FSlabSurfaceAppearanceRecord A;A.Preset=SurfacePresetId(SurfacePreset);A.bLidarLinked=bSurfaceLidarLinked;A.bHot=bHotAppearance;A.Oxidation=Oxidation;A.Roughness=Roughness;A.EmissiveStrength=EmissiveStrength;
    if(auto* R=FindComponentByClass<UVirtualLidarSurfaceResponseComp>()){A.bHasOpticalResponse=true;A.Reflectivity940Nm=R->Reflectivity940Nm;A.IntensityGain=R->IntensityGain;}
    if(Run==Status.RunUUID){A.SlabFrame=Status.FrameNo;A.ElapsedSec=Status.ElapsedSec;}
    if(auto* Results=GetWorld()->GetSubsystem<USlabRunResultsSubsystem>())Results->RecordSurfaceAppearance(Run,A);
}

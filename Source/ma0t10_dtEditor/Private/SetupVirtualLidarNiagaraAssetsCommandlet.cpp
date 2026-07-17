#include "SetupVirtualLidarNiagaraAssetsCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EdGraphSchema_Niagara.h"
#include "Factories/MaterialFactoryNew.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "NiagaraConstants.h"
#include "NiagaraDataInterfaceArrayFloat.h"
#include "NiagaraEmitter.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
#include "UObject/SavePackage.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

namespace
{
constexpr TCHAR AssetFolder[] = TEXT("/Game/MA0T10/Sensor/VFX");

bool SaveAsset(UObject* Asset)
{
    if (!Asset) return false;
    UPackage* Package = Asset->GetOutermost();
    Package->MarkPackageDirty();
    const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(Package, Asset, *FileName, Args);
}

UEdGraphPin& GetOrCreateOverridePin(UNiagaraNodeFunctionCall& Function, const FName InputName, const FNiagaraTypeDefinition& Type)
{
    const FNiagaraParameterHandle ModuleHandle = FNiagaraParameterHandle::CreateModuleParameterHandle(InputName);
    const FNiagaraParameterHandle AliasedHandle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(ModuleHandle, &Function);
    return FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(Function, AliasedHandle, Type, FGuid(), FGuid());
}

void LinkFunctionInput(
    UNiagaraNodeFunctionCall& Function,
    const FName InputName,
    const FNiagaraTypeDefinition& InputType,
    const FNiagaraVariable& LinkedVariable)
{
    UEdGraphPin& Pin = GetOrCreateOverridePin(Function, InputName, InputType);
    if (Pin.LinkedTo.Num() > 0) return;
    TSet<FNiagaraVariable> KnownParameters;
    KnownParameters.Add(LinkedVariable);
    FNiagaraStackGraphUtilities::SetLinkedValueHandleForFunctionInput(
        Pin,
        FNiagaraParameterHandle(LinkedVariable.GetName()),
        KnownParameters,
        ENiagaraDefaultMode::Value);
}

void ConfigureArrayDynamicInput(
    UNiagaraNodeAssignment& Assignment,
    const FNiagaraVariable& Target,
    const TCHAR* DynamicInputPath,
    const FName ArrayInputName,
    const FNiagaraTypeDefinition& ArrayType,
    const FName UserArrayName)
{
    UEdGraphPin& TargetPin = GetOrCreateOverridePin(Assignment, Target.GetName(), Target.GetType());
    if (TargetPin.LinkedTo.Num() > 0) return;

    UNiagaraScript* DynamicInputScript = LoadObject<UNiagaraScript>(nullptr, DynamicInputPath);
    if (!DynamicInputScript)
    {
        UE_LOG(LogTemp, Error, TEXT("Missing Niagara dynamic input: %s"), DynamicInputPath);
        return;
    }

    UNiagaraNodeFunctionCall* DynamicInput = nullptr;
    FNiagaraStackGraphUtilities::SetDynamicInputForFunctionInput(TargetPin, DynamicInputScript, DynamicInput);
    if (!DynamicInput) return;

    LinkFunctionInput(*DynamicInput, ArrayInputName, ArrayType, FNiagaraVariable(ArrayType, UserArrayName));
    LinkFunctionInput(*DynamicInput, TEXT("Direct Array Index"), FNiagaraTypeDefinition::GetIntDef(),
        FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Engine.ExecutionIndex")));

    UEnum* SamplingModeEnum = LoadObject<UEnum>(nullptr, TEXT("/Niagara/Enums/ENiagaraArraySamplingMode.ENiagaraArraySamplingMode"));
    if (SamplingModeEnum)
    {
        UEdGraphPin& SamplingPin = GetOrCreateOverridePin(
            *DynamicInput,
            TEXT("Array Sampling Mode"),
            FNiagaraTypeDefinition(SamplingModeEnum));
        for (int32 Index = 0; Index < SamplingModeEnum->NumEnums(); ++Index)
        {
            const FString Name = SamplingModeEnum->GetNameStringByIndex(Index);
            const FString DisplayName = SamplingModeEnum->GetDisplayNameTextByIndex(Index).ToString();
            if (Name.Contains(TEXT("Direct"), ESearchCase::IgnoreCase) || DisplayName.Contains(TEXT("Direct"), ESearchCase::IgnoreCase))
            {
                GetDefault<UEdGraphSchema_Niagara>()->TrySetDefaultValue(SamplingPin, Name);
                break;
            }
        }
    }
}

bool ConfigurePointArrayEmitter(UNiagaraSystem& System)
{
    bool bConfigured = false;
    for (FNiagaraEmitterHandle& Handle : System.GetEmitterHandles())
    {
        FVersionedNiagaraEmitterData* EmitterData = Handle.GetInstance().GetEmitterData();
        UNiagaraScriptSource* Source = EmitterData ? Cast<UNiagaraScriptSource>(EmitterData->GraphSource) : nullptr;
        if (!EmitterData || !Source || !Source->NodeGraph) continue;

        UNiagaraNodeOutput* ParticleSpawnOutput = Source->NodeGraph->FindEquivalentOutputNode(ENiagaraScriptUsage::ParticleSpawnScript);
        if (!ParticleSpawnOutput)
        {
            ParticleSpawnOutput = Source->NodeGraph->FindEquivalentOutputNode(ENiagaraScriptUsage::ParticleSpawnScriptInterpolated);
        }
        if (!ParticleSpawnOutput) continue;

        TArray<UNiagaraNodeFunctionCall*> SpawnFunctions;
        for (UEdGraphNode* Node : Source->NodeGraph->Nodes)
        {
            UNiagaraNodeFunctionCall* Function = Cast<UNiagaraNodeFunctionCall>(Node);
            if (Function && Function->FunctionScript && Function->FunctionScript->GetPathName().Contains(TEXT("SpawnBurst_Instantaneous")))
            {
                SpawnFunctions.Add(Function);
            }
        }
        for (UNiagaraNodeFunctionCall* Function : SpawnFunctions)
        {
            LinkFunctionInput(*Function, TEXT("Spawn Count"), FNiagaraTypeDefinition::GetIntDef(),
                FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("User.PointCount")));
        }

        UNiagaraNodeAssignment* PointAssignment = nullptr;
        for (UEdGraphNode* Node : Source->NodeGraph->Nodes)
        {
            UNiagaraNodeAssignment* Existing = Cast<UNiagaraNodeAssignment>(Node);
            if (Existing && Existing->FindAssignmentTarget(SYS_PARAM_PARTICLES_POSITION.GetName()) != INDEX_NONE)
            {
                PointAssignment = Existing;
                break;
            }
        }

        if (!PointAssignment)
        {
            const TArray<FNiagaraVariable> Targets = {
                SYS_PARAM_PARTICLES_POSITION,
                SYS_PARAM_PARTICLES_COLOR,
                SYS_PARAM_PARTICLES_SPRITE_SIZE
            };
            const TArray<FString> Defaults = {
                FNiagaraConstants::GetAttributeDefaultValue(SYS_PARAM_PARTICLES_POSITION),
                FNiagaraConstants::GetAttributeDefaultValue(SYS_PARAM_PARTICLES_COLOR),
                FNiagaraConstants::GetAttributeDefaultValue(SYS_PARAM_PARTICLES_SPRITE_SIZE)
            };
            PointAssignment = FNiagaraStackGraphUtilities::AddParameterModuleToStack(Targets, *ParticleSpawnOutput, INDEX_NONE, Defaults);
        }
        if (!PointAssignment) continue;

        ConfigureArrayDynamicInput(*PointAssignment, SYS_PARAM_PARTICLES_POSITION,
            TEXT("/Niagara/DynamicInputs/Arrays/SelectPositionFromArray.SelectPositionFromArray"),
            TEXT("Position Array"), FNiagaraTypeDefinition(UNiagaraDataInterfaceArrayPosition::StaticClass()), TEXT("User.PointPositions"));
        ConfigureArrayDynamicInput(*PointAssignment, SYS_PARAM_PARTICLES_COLOR,
            TEXT("/Niagara/DynamicInputs/Arrays/SelectLinearColorFromArray.SelectLinearColorFromArray"),
            TEXT("Color Selection Array"), FNiagaraTypeDefinition(UNiagaraDataInterfaceArrayColor::StaticClass()), TEXT("User.PointColors"));
        LinkFunctionInput(*PointAssignment, SYS_PARAM_PARTICLES_SPRITE_SIZE.GetName(), FNiagaraTypeDefinition::GetVec2Def(),
            FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("User.PointSpriteSize")));

        EmitterData->SimTarget = ENiagaraSimTarget::GPUComputeSim;
        bConfigured = true;
    }
    return bConfigured;
}
}

USetupVirtualLidarNiagaraAssetsCommandlet::USetupVirtualLidarNiagaraAssetsCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 USetupVirtualLidarNiagaraAssetsCommandlet::Main(const FString& Params)
{
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

    UMaterial* Material = LoadObject<UMaterial>(nullptr, TEXT("/Game/MA0T10/Sensor/VFX/M_VirtualLidarPointSprite.M_VirtualLidarPointSprite"));
    if (!Material)
    {
        UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
        Material = Cast<UMaterial>(AssetTools.CreateAsset(TEXT("M_VirtualLidarPointSprite"), AssetFolder, UMaterial::StaticClass(), MaterialFactory));
        if (Material)
        {
            Material->BlendMode = BLEND_Additive;
            Material->SetShadingModel(MSM_Unlit);
            Material->TwoSided = true;
        }
    }
    if (Material)
    {
        Material->BlendMode = BLEND_Additive;
        Material->SetShadingModel(MSM_Unlit);
        Material->TwoSided = true;
        if (UMaterialEditingLibrary::GetNumMaterialExpressions(Material) == 0)
        {
            UClass* ParticleColorClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.MaterialExpressionParticleColor"));
            UMaterialExpression* ParticleColor = UMaterialEditingLibrary::CreateMaterialExpression(
                Material, ParticleColorClass, -180, 0);
            UMaterialEditingLibrary::ConnectMaterialProperty(ParticleColor, TEXT("RGB"), MP_EmissiveColor);
            UMaterialEditingLibrary::ConnectMaterialProperty(ParticleColor, TEXT("A"), MP_Opacity);
            bool bNeedsRecompile = false;
            UMaterialEditingLibrary::SetMaterialUsage(Material, MATUSAGE_NiagaraSprites, bNeedsRecompile);
            UMaterialEditingLibrary::RecompileMaterial(Material);
        }
    }
    if (!SaveAsset(Material))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create Niagara point material"));
        return 1;
    }

    UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/MA0T10/Sensor/VFX/NS_VirtualLidarPointCloud.NS_VirtualLidarPointCloud"));
    if (!System)
    {
        UNiagaraEmitter* TemplateEmitter = LoadObject<UNiagaraEmitter>(nullptr, TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst"));
        if (!TemplateEmitter)
        {
            UE_LOG(LogTemp, Error, TEXT("SimpleSpriteBurst Niagara template was not found"));
            return 2;
        }
        UPackage* Package = CreatePackage(TEXT("/Game/MA0T10/Sensor/VFX/NS_VirtualLidarPointCloud"));
        System = NewObject<UNiagaraSystem>(Package, TEXT("NS_VirtualLidarPointCloud"), RF_Public | RF_Standalone | RF_Transactional);
        UNiagaraSystemFactoryNew::InitializeSystem(System, true);
        System->AddEmitterHandle(*TemplateEmitter, TEXT("VirtualLidarGpuPoints"), TemplateEmitter->GetExposedVersion().VersionGuid);
        FAssetRegistryModule::AssetCreated(System);
    }
    if (!System)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create Niagara point cloud system"));
        return 3;
    }

    FNiagaraUserRedirectionParameterStore& Parameters = System->GetExposedParameters();
    Parameters.AddParameter(FNiagaraVariable(FNiagaraTypeDefinition(UNiagaraDataInterfaceArrayPosition::StaticClass()), TEXT("User.PointPositions")), false);
    Parameters.AddParameter(FNiagaraVariable(FNiagaraTypeDefinition(UNiagaraDataInterfaceArrayColor::StaticClass()), TEXT("User.PointColors")), false);
    Parameters.AddParameter(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("User.PointCount")), false);
    Parameters.AddParameter(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.PointSize")), false);
    Parameters.AddParameter(FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("User.PointSpriteSize")), false);

    for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
    {
        FVersionedNiagaraEmitterData* EmitterData = Handle.GetInstance().GetEmitterData();
        if (!EmitterData) continue;
        EmitterData->SimTarget = ENiagaraSimTarget::GPUComputeSim;
        for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
        {
            if (UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
            {
                Sprite->Material = Material;
                Sprite->SortMode = ENiagaraSortMode::None;
            }
        }
    }
    if (!ConfigurePointArrayEmitter(*System))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to configure Niagara point-array emitter graph"));
        return 4;
    }
    System->RequestCompile(false);
    if (!SaveAsset(System))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to save Niagara point cloud system"));
        return 5;
    }
    FAssetRegistryModule::AssetCreated(Material);
    FAssetRegistryModule::AssetCreated(System);
    UE_LOG(LogTemp, Display, TEXT("Virtual LiDAR Niagara assets are ready: %s"), *System->GetPathName());
    return 0;
}

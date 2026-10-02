#include "DTCoreContractListener.h"
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Crane/CraneDataSyncComp.h"
#include "UI/DxWidget.h"
#include "UI/DxWidgetDataType.h"
#include "UObject/UnrealType.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Core/DxApiSubsystem.h"
#include "Core/DTCoreSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Misc/ScopeExit.h"

bool UDTCoreContractListener::CompileBlueprintForValidation(UBlueprint* Blueprint)
{
    if (!Blueprint) return false;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
    return Blueprint->Status != BS_Error && Blueprint->GeneratedClass != nullptr;
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreHttpContractTest, "MA0T10.DTCoreIntegration.HttpBodyAndFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDTCoreHttpContractTest::RunTest(const FString&)
{
    auto* Api = NewObject<UDxApiSubsystem>();
    Api->HttpModule = &FHttpModule::Get();
    Api->ApiDataTable = NewObject<UDataTable>();
    Api->ApiDataTable->RowStruct = FApiStruct::StaticStruct();
    FApiStruct Row;
    Row.ApiType = EApiType::Local;
    Row.ApiMethod = EApiMethod::Post;
    Row.ApiUrl = TEXT("/capture");
    Api->ApiDataTable->AddRow(TEXT("capture"), Row);
    auto* Settings = GetMutableDefault<UDTCoreSettings>();
    const FString OldLocal = Settings->LocalApiUrl, OldBase = Settings->BaseApiUrl;
    ON_SCOPE_EXIT { Settings->LocalApiUrl = OldLocal; Settings->BaseApiUrl = OldBase; };
    Settings->LocalApiUrl.Reset(); Settings->BaseApiUrl = TEXT("http://settings.invalid");
    TMap<FString,FString> Overrides;
    Overrides.Add(TEXT("BaseApiUrl"), TEXT("http://override.invalid"));
    Api->RuntimeOverrideReaderForTests = [&Overrides](const TCHAR* Key, FString& Out)
    { if (const auto* Found=Overrides.Find(Key)) { Out=*Found; return true; } return false; };
    TestEqual(TEXT("Base override is used when environment settings absent"), Api->GetServerUrl(EApiType::Local), TEXT("http://override.invalid"));
    Settings->LocalApiUrl = TEXT("http://environment.invalid");
    TestEqual(TEXT("environment setting precedes Base override"), Api->GetServerUrl(EApiType::Local), Settings->LocalApiUrl);
    Overrides.Add(TEXT("LocalApiUrl"), TEXT("http://environment-override.invalid"));
    TestEqual(TEXT("environment override wins"), Api->GetServerUrl(EApiType::Local), TEXT("http://environment-override.invalid"));
    Overrides.Reset(); Settings->LocalApiUrl.Reset();
    TestEqual(TEXT("Base setting is final configured fallback"), Api->GetServerUrl(EApiType::Local), Settings->BaseApiUrl);
    FString CapturedBody, CapturedUrl;
    int32 Requests = 0;
    Api->RequestProbeForTests = [&](const TSharedRef<IHttpRequest,ESPMode::ThreadSafe>& Request)
    {
        ++Requests; CapturedUrl=Request->GetURL();
        const auto& Bytes=Request->GetContent();
        CapturedBody=Bytes.IsEmpty()?FString():FString(FUTF8ToTCHAR(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()),Bytes.Num()).Get());
        return false; // 실제 요청 객체를 검사하되 외부 네트워크로 보내지 않는다.
    };
    const FString Body = TEXT("{\"소재\":\"SQ83521 047\",\"frame_no\":580}");
    Api->DxRequestApiWithBody(TEXT("capture"), FDxApiCallback(), Body);
    TestEqual(TEXT("UTF-8 Body reaches actual request"), CapturedBody, Body);
    Api->DxRequestApiWithParameterAndBody(TEXT("capture"), FDxApiCallback(), {TEXT("sensor"),TEXT("42")}, Body);
    TestEqual(TEXT("parameter+Body preserves payload"), CapturedBody, Body);
    TestTrue(TEXT("parameters remain in request URL"), CapturedUrl.EndsWith(TEXT("/capture/sensor/42")));
    Api->DxRequestApiWithBody(TEXT("capture"), FDxApiCallback(), FString());
    TestTrue(TEXT("empty body remains empty"), CapturedBody.IsEmpty());
    TestEqual(TEXT("exactly one request constructed per invocation"), Requests, 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreCraneContractTest, "MA0T10.DTCoreIntegration.CraneDispatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDTCoreCraneContractTest::RunTest(const FString&)
{
    auto* Sync = NewObject<UCraneDataSyncComp>();
    auto* Listener = NewObject<UDTCoreContractListener>();
    Sync->OnCranePositionChanged.AddDynamic(Listener, &UDTCoreContractListener::ReceivePosition);
    auto Data = MakeShared<FCraneStateData>();
    Data->CraneId = TEXT("compatibility-fixture");
    Data->Position.AnchorPosition = 12.5f;
    Data->Position.HookRopeHeight = 33.0f;
    Sync->OnReceiveData(Data);
    TestEqual(TEXT("new int32 type dispatch emits one event"), Listener->ReceivedCount, 1);
    TestEqual(TEXT("position is preserved"), Listener->LastPosition.AnchorPosition, 12.5f);
    TestEqual(TEXT("old CraneState numerical meaning retained"), Data->GetType(), 1);
    struct FOtherData : FDxDataBase { int32 GetType() const override { return 1001; } };
    Sync->OnReceiveData(MakeShared<FOtherData>());
    Sync->OnReceiveData(nullptr);
    TestEqual(TEXT("other/null messages cannot dispatch as Crane"), Listener->ReceivedCount, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreWidgetContractTest, "MA0T10.DTCoreIntegration.WidgetIdentifiers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDTCoreWidgetContractTest::RunTest(const FString&)
{
    const auto* Enum = StaticEnum<EDxWidgetFlag>();
    const TCHAR* Names[] = {TEXT("None"),TEXT("ShipFreeView"),TEXT("ShipTopView"),TEXT("CraneFreeView"),TEXT("CraneTopView"),TEXT("UpdateWidget"),TEXT("CctvWidget")};
    for (int32 Index=0; Index<7; ++Index)
        TestEqual(Names[Index], Enum->GetValueByNameString(Names[Index]), static_cast<int64>(Index));
    TestNotNull(TEXT("new widget identifier is a byte property"), FindFProperty<FByteProperty>(UDxWidget::StaticClass(), TEXT("WidgetFlag")));
    return true;
}
#endif

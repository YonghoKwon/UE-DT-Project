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
#include "Engine/GameInstance.h"
#include "Core/DxWebSocketSubsystem.h"
#include "IStompClient.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "Async/TaskGraphInterfaces.h"
#include "Core/DxDataSubsystem.h"
#include "Core/DxLogSubsystem.h"
#include "Core/DxObjectSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Player/DxPlayerControllerBase.h"
#include "InteractableActor/InteractableActor.h"
#include "Core/DxWidgetSubsystem.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableSet.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"

class FDTCoreFakeStomp : public IStompClient
{
public:
    bool Connected=true;
    TArray<FStompRequestCompleted> Completions;
    FStompClientConnectedEvent ConnectedEvent;
    FStompClientConnectionErrorEvent ConnectionErrorEvent;
    FStompClientErrorEvent ErrorEvent;
    FStompClientClosedEvent ClosedEvent;
    void Connect(const FStompHeader&) override { Connected=true; }
    void Disconnect(const FStompHeader&) override { Connected=false; }
    bool IsConnected() const override { return Connected; }
    FString Subscribe(const FString&,const FStompSubscriptionEvent&,const FStompRequestCompleted& C) override
    { Completions.Add(C); return FString::FromInt(Completions.Num()); }
    void Unsubscribe(FString,const FStompRequestCompleted& C) override { C.ExecuteIfBound(true,FString()); }
    void Send(const FString&,const FStompBuffer&,const FStompHeader&,const FStompRequestCompleted&) override {}
    FStompClientConnectedEvent& OnConnected() override { return ConnectedEvent; }
    FStompClientConnectionErrorEvent& OnConnectionError() override { return ConnectionErrorEvent; }
    FStompClientErrorEvent& OnError() override { return ErrorEvent; }
    FStompClientClosedEvent& OnClosed() override { return ClosedEvent; }
};

class FDTCoreCallbackCommand : public IAutomationLatentCommand
{
    TFunction<bool()> Callback;
public:
    explicit FDTCoreCallbackCommand(TFunction<bool()> In) : Callback(MoveTemp(In)) {}
    bool Update() override { return Callback(); }
};

bool UDTCoreContractListener::CompileBlueprintForValidation(UBlueprint* Blueprint)
{
    if (!Blueprint) return false;
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
    return Blueprint->Status != BS_Error && Blueprint->GeneratedClass != nullptr;
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSensorCameraCoherenceUnitTest,"MA0T10.SensorFiles.CameraAcquisitionSnapshot",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSensorCameraCoherenceUnitTest::RunTest(const FString&)
{
    auto Camera=MakeShared<TStrongObjectPtr<UVirtualCameraCaptureComponent>>(NewObject<UVirtualCameraCaptureComponent>());
    auto* C=Camera->Get(); C->CaptureMode=EVirtualCameraCaptureMode::Payload; C->OutputMode=EVirtualCameraOutputMode::None;
    FVirtualCameraPayloadSnapshot Snapshot; Snapshot.FrameId=17;Snapshot.SensorId=TEXT("snapshot-fixture");
    Snapshot.Width=8;Snapshot.Height=8;Snapshot.Location=FVector(10,20,30);Snapshot.TimestampUtc=FDateTime(2026,10,2,12,0,0);
    C->ScheduledAcquisitionSnapshots.Add(17,Snapshot);C->EncodeOrder.Add(17);
    C->SetWorldLocation(FVector(900,800,700));
    TArray<FColor> Pixels;Pixels.Init(FColor::Blue,64);
    TestTrue(TEXT("delayed encode accepted"),C->StartScheduledEncode(MoveTemp(Pixels),8,8,17,FPlatformTime::Seconds()));
    const double Start=FPlatformTime::Seconds();
    ADD_LATENT_AUTOMATION_COMMAND(FDTCoreCallbackCommand([this,Camera,Snapshot,Start]()
    {
        if(FPlatformTime::Seconds()-Start>5){AddError(TEXT("JPEG encode did not finish"));return true;}
        if(!Camera->Get()->GetLastJpegSnapshot().IsValid())return false;
        const auto& Result=Camera->Get()->GetLastCompletedAcquisition();
        TestEqual(TEXT("encode retains acquisition pose rather than current pose"),Result.Location,Snapshot.Location);
        TestEqual(TEXT("acquisition UTC survives encode"),Camera->Get()->GetRuntimeStatus().LastUpdateUtc,Snapshot.TimestampUtc);
        TestEqual(TEXT("acquisition frame id survives encode"),Camera->Get()->GetRuntimeStatus().FrameId,int64(17));
        UVirtualCameraCaptureComponent::FPendingReadbackRequest Missing;Missing.FrameId=18;
        Camera->Get()->PendingReadbackRequests.Add(Missing);Camera->Get()->EncodeOrder.Add(18);
        Camera->Get()->LastFileAcquisition.FrameId=19;
        Camera->Get()->QueuePendingGpuReadbacks();
        TestTrue(TEXT("old metadata request cannot wait for a newer target"),Camera->Get()->PendingReadbackRequests.IsEmpty());
        TestFalse(TEXT("failed old frame is removed from ordered completion queue"),Camera->Get()->EncodeOrder.Contains(18));
        return true;
    }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreBlueprintCloseTest,"MA0T10.DTCoreIntegration.BlueprintCloseDispatch",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDTCoreBlueprintCloseTest::RunTest(const FString&)
{
    auto* BP=Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(UDxWidget::StaticClass(),GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(),UWidgetBlueprint::StaticClass(),TEXT("CloseDispatchFixture")),BPTYPE_Normal,
        UWidgetBlueprint::StaticClass(),UWidgetBlueprintGeneratedClass::StaticClass()));
    FEdGraphPinType Type; Type.PinCategory=UEdGraphSchema_K2::PC_Int;
    FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("CloseProbe"),Type,TEXT("0"));
    UEdGraph* Graph=FBlueprintEditorUtils::FindEventGraph(BP);
    auto* Event=NewObject<UK2Node_Event>(Graph);
    Event->EventReference.SetExternalMember(TEXT("CloseWidgetAddLogic"),UDxWidget::StaticClass());
    Event->bOverrideFunction=true; Graph->AddNode(Event,false,false); Event->CreateNewGuid(); Event->PostPlacedNewNode(); Event->AllocateDefaultPins();
    auto* Set=NewObject<UK2Node_VariableSet>(Graph);
    Set->VariableReference.SetSelfMember(TEXT("CloseProbe"));
    Graph->AddNode(Set,false,false); Set->CreateNewGuid(); Set->PostPlacedNewNode(); Set->AllocateDefaultPins();
    Set->FindPin(TEXT("CloseProbe"))->DefaultValue=TEXT("77");
    Event->FindPin(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(Set->FindPin(UEdGraphSchema_K2::PN_Execute));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    if (!UDTCoreContractListener::CompileBlueprintForValidation(BP)) { AddError(TEXT("Fixture BP failed to compile")); return false; }
    auto* Widget=NewObject<UDxWidget>(GetTransientPackage(),BP->GeneratedClass);
    auto* Property=FindFProperty<FIntProperty>(BP->GeneratedClass,TEXT("CloseProbe"));
    Widget->CloseWidget();
    TestEqual(TEXT("Blueprint override runs on close"),Property->GetPropertyValue_InContainer(Widget),77);
    Property->SetPropertyValue_InContainer(Widget,19);
    Widget->CloseWidget();
    TestEqual(TEXT("duplicate close does not run hook twice"),Property->GetPropertyValue_InContainer(Widget),19);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreInputPolicyTest,"MA0T10.DTCoreIntegration.ClickPolicy",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDTCoreInputPolicyTest::RunTest(const FString&)
{
    auto* Controller=NewObject<ADxPlayerControllerBase>();
    Controller->CurrentHoveredActor=NewObject<AInteractableActor>();
    int32 Activations=0; double Now=1.0;
    Controller->InputClockForTests=[&] { return Now; };
    Controller->ClickSinkForTests=[&](AActor*) { ++Activations; };
    Controller->ClickLeftMouseButton(FInputActionValue(true));
    Controller->ClickLeftMouseButton(FInputActionValue(true));
    Controller->ClickLeftMouseButton(FInputActionValue(false));
    TestEqual(TEXT("one/duplicate press does not activate default double policy"),Activations,0);
    Now=1.2; Controller->ClickLeftMouseButton(FInputActionValue(true));
    Controller->ClickLeftMouseButton(FInputActionValue(false));
    TestEqual(TEXT("same-actor second press activates once"),Activations,1);
    Controller->ClickActivationPolicy=EDxClickActivationPolicy::SingleRelease;
    Controller->ClickLeftMouseButton(FInputActionValue(true));
    Controller->ClickLeftMouseButton(FInputActionValue(false));
    Controller->ClickLeftMouseButton(FInputActionValue(false));
    TestEqual(TEXT("single policy ignores duplicate release"),Activations,2);
    Controller->bIsWidgetUnderMouse=true;
    Controller->ClickLeftMouseButton(FInputActionValue(true)); Controller->ClickLeftMouseButton(FInputActionValue(false));
    TestEqual(TEXT("UI input cannot activate 3D actor"),Activations,2);
    auto* GI=NewObject<UGameInstance>(); auto* Widgets=NewObject<UDxWidgetSubsystem>(GI);
    auto* External=NewObject<UDxWidget>();
    Widgets->RegisterExternalInputBlocker(External); Widgets->RegisterExternalInputBlocker(External);
    TestTrue(TEXT("input blocker registration does not take window ownership"),Widgets->GetOpenWidgets().IsEmpty());
    Widgets->UnregisterExternalInputBlocker(External);
    TestTrue(TEXT("input blocker removal leaves other ownership unchanged"),Widgets->GetOpenWidgets().IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreWorkerShutdownTest, "MA0T10.DTCoreIntegration.ParseShutdown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDTCoreWorkerShutdownTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>());
    TStrongObjectPtr<UDxDataSubsystem> Data(NewObject<UDxDataSubsystem>(GI.Get()));
    auto* Handler=NewObject<UDTCoreSlowTransaction>(Data.Get());
    auto Probe=MakeShared<FDTCoreParseProbe,ESPMode::ThreadSafe>(); Handler->Probe=Probe;
    Data->TransactionCodeMessageMap.Add(TEXT("slow"),Handler);
    Data->CachedHandlerTransactionCodeMessageMap=MakeShared<TMap<FString,UTransactionCodeMessage*>>();
    Data->CachedHandlerTransactionCodeMessageMap->Add(TEXT("slow"),Handler);
    Data->EnqueueWebSocketData(TEXT("{\"MESSAGE_ID\":\"slow\"}")); Data->Tick(0);
    const double Deadline=FPlatformTime::Seconds()+2.0;
    while (!Probe->Started.Load() && FPlatformTime::Seconds()<Deadline) FPlatformProcess::Sleep(0.001f);
    TestTrue(TEXT("slow worker actually starts"),Probe->Started.Load());
    Data->Deinitialize();
    TestTrue(TEXT("shutdown joined parser"),Data->WebSocketWorker.IsReady());
    FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
    TestEqual(TEXT("late result cannot apply after shutdown"),Probe->Applied.Load(),0);
    TestTrue(TEXT("handler ownership released after join"),Data->TransactionCodeMessageMap.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreLogFlushTest, "MA0T10.DTCoreIntegration.LogFlush",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDTCoreLogFlushTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>());
    TStrongObjectPtr<UDxLogSubsystem> Log(NewObject<UDxLogSubsystem>(GI.Get()));
    Log->CachedLogDirectory=FPaths::ProjectSavedDir()/TEXT("Reports/DTCoreSync/LogProbe");
    const FString Name=FGuid::NewGuid().ToString()+TEXT(".log");
    for (int32 I=0;I<40;++I) Log->WriteLog(FString::Printf(TEXT("sequence=%03d"),I),false,Name);
    TestTrue(TEXT("flush confirms IO completion"),Log->FlushLogs());
    FString Contents; FFileHelper::LoadFileToString(Contents,*(Log->CachedLogDirectory/Name));
    int32 Previous=-1;
    for (int32 I=0;I<40;++I)
    {
        const int32 Index=Contents.Find(FString::Printf(TEXT("sequence=%03d"),I));
        TestTrue(TEXT("logs written once and in order"),Index>Previous); Previous=Index;
    }
    Log->WriterSinkForTests=[](const FString&,const FString&) { FPlatformProcess::Sleep(0.05f); return false; };
    AddExpectedError(TEXT("DTCore log file write failed"),EAutomationExpectedErrorFlags::Contains,1);
    Log->WriteLog(TEXT("forced failure"),false,Name);
    TestFalse(TEXT("flush exposes slow writer failure"),Log->FlushLogs());
    Log->Deinitialize();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreRegistryGcTest, "MA0T10.DTCoreIntegration.RegistryGC",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDTCoreRegistryGcTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>());
    TStrongObjectPtr<UDxObjectSubsystem> Registry(NewObject<UDxObjectSubsystem>(GI.Get()));
    auto* Actor=NewObject<AActor>(); TWeakObjectPtr<AActor> Weak=Actor;
    Registry->RegisterObject(TEXT("test"),TEXT("a"),Actor);
    Actor=nullptr; CollectGarbage(RF_NoFlags);
    TestTrue(TEXT("registry references are explicitly tracked by GC"),Weak.IsValid());
    TestEqual(TEXT("legacy map accessor remains available"),Registry->GetCategoryMap(TEXT("test"))->Num(),1);
    Weak->MarkAsGarbage(); CollectGarbage(RF_NoFlags);
    TestNull(TEXT("destroyed/garbage actor cannot be found"),Registry->FindObject(TEXT("test"),TEXT("a")));
    Registry->CompactAllInvalidObjects();
    TestEqual(TEXT("invalid entries safely compact"),Registry->GetRegisteredObjectCount(TEXT("test")),0);
    Registry->ClearAllObjects(); Registry->Deinitialize();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreSubscriptionLifecycleTest, "MA0T10.DTCoreIntegration.SubscriptionLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDTCoreSubscriptionLifecycleTest::RunTest(const FString&)
{
    auto GI=MakeShared<TStrongObjectPtr<UGameInstance>>(NewObject<UGameInstance>());
    auto Ws=MakeShared<TStrongObjectPtr<UDxWebSocketSubsystem>>(NewObject<UDxWebSocketSubsystem>(GI->Get()));
    auto Listener=MakeShared<TStrongObjectPtr<UDTCoreContractListener>>(NewObject<UDTCoreContractListener>());
    auto Fake=MakeShared<FDTCoreFakeStomp>();
    auto* S=Ws->Get(); S->StompClient=Fake; S->bWantsConnection=true;
    S->OnConnected.AddDynamic(Listener->Get(),&UDTCoreContractListener::ReceiveReady);
    S->TopicRouteMap.Add(TEXT("a"),ETopicRouteType::WebSocket);
    S->TopicRouteMap.Add(TEXT("b"),ETopicRouteType::Api);
    S->HandleOnConnected(TEXT("1.2"),TEXT("test"),FString());
    Fake->Completions[0].Execute(true,FString()); Fake->Completions[1].Execute(false,TEXT("denied"));
    FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
    TestFalse(TEXT("failed subscription cannot announce ready"),S->AreConfiguredSubscriptionsReady());
    TestEqual(TEXT("failed batch ready notifications"),Listener->Get()->ReadyCount,0);
    S->CancelSubscriptionOperations(); ++S->ConnectionGeneration;
    S->HandleOnConnected(TEXT("1.2"),TEXT("next"),FString());
    Fake->Completions[0].Execute(true,FString()); // Old receipt must not decrement the new generation.
    FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
    TestEqual(TEXT("stale callback cannot consume new pending count"),S->PendingSubscribeCount,2);
    Fake->Completions[2].Execute(true,FString()); Fake->Completions[3].Execute(true,FString());
    FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
    TestTrue(TEXT("all subscriptions succeed"),S->AreConfiguredSubscriptionsReady());
    TestEqual(TEXT("one ready notification per generation"),Listener->Get()->ReadyCount,1);
    Fake->Completions[3].Execute(true,FString());
    FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
    TestEqual(TEXT("duplicate receipt not rebroadcast"),Listener->Get()->ReadyCount,1);
    S->CancelSubscriptionOperations(); ++S->ConnectionGeneration;
    S->TopicRouteMap.Reset(); S->TopicRouteMap.Add(TEXT("missing"),ETopicRouteType::WebSocket);
    S->HandleOnConnected(TEXT("1.2"),TEXT("timeout"),FString());
    auto Step=MakeShared<int32>(0);
    ADD_LATENT_AUTOMATION_COMMAND(FDTCoreCallbackCommand([this,GI,Ws,Listener,Fake,Step]()
    {
        if ((*Step)++<2) { GI->Get()->GetTimerManager().Tick(6.0f); return false; }
        FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
        auto* Current=Ws->Get();
        TestEqual(TEXT("missing receipt reaches plugin timeout"),Current->GetSubscriptionStatuses()[0].State,EDxSubscriptionState::TimedOut);
        TestFalse(TEXT("timeout does not become ready"),Current->AreConfiguredSubscriptionsReady());
        Current->DisconnectStompClient({}); Current->TryReconnect();
        TestFalse(TEXT("manual disconnect cancels connection intent"),Current->bWantsConnection);
        TestFalse(TEXT("manual disconnect has no retry timer"),GI->Get()->GetTimerManager().IsTimerActive(Current->ReconnectTimerHandle));
        Fake->Completions.Last().Execute(true,FString());
        FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
        TestEqual(TEXT("late receipt after disconnect not ready"),Listener->Get()->ReadyCount,1);
        Current->Deinitialize(); return true;
    }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreHttpContractTest, "MA0T10.DTCoreIntegration.HttpBodyAndFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDTCoreHttpContractTest::RunTest(const FString&)
{
    auto* Api = NewObject<UDxApiSubsystem>(NewObject<UGameInstance>());
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

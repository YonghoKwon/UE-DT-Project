#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Uxr15ToggleTestHelpers.h"
#include "ma0t10_dt/MA0T10/Sensor/Tests/FinalAcceptanceTestSession.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraSensorActor.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraCaptureComponent.h"
#include "ma0t10_dt/MA0T10/Camera/VirtualCameraGroundTruth.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorToolWorkspaceSubsystem.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorPlacementPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/VirtualSensorMonitorPanelWidget.h"
#include "ma0t10_dt/MA0T10/UI/SensorToolToolbarWidget.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Widgets/Images/SImage.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Base64.h"
#include "HAL/FileManager.h"
#include "RHIGlobals.h"

// Uses the existing scheduler, recorder, publisher and owned DTCore panels. No
// monitor readback/capture is added. JPEG decoding is a test-only output oracle.
class FDualCamera22Command final : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TWeakObjectPtr<AVirtualCameraSensorActor> Cameras[2];
    double Started = FPlatformTime::Seconds(), Next = 0;
    int32 Phase = 0, Case = 0;
    bool Manual = FParse::Param(FCommandLine::Get(), TEXT("DualCamera22Manual"));
    FString Report;

    bool Configure(int32 Index, FIntPoint Size, bool Truth = false)
    {
        FVirtualSensorEditableState State;
        Cameras[Index]->ReadEditableState(State);
        State.SensorId = Index ? TEXT("DUAL22-B") : TEXT("DUAL22-A");
        State.CameraResolution = Size;
        State.CameraCaptureInterval = .1f;
        State.CameraCaptureMode = EVirtualCameraCaptureMode::Payload;
        State.bCameraGroundTruth = Truth;
        FString Error;
        return Test->TestTrue(TEXT("fixture applies through Actor editing contract: ") + Error,
            Cameras[Index]->ApplyEditableState(State, Error));
    }

    void Observe(UWorld* World, UVirtualSensorMonitorPanelWidget* M, const FString& Label)
    {
        FString Text = Label + TEXT("\n");
        const auto View = World->GetGameViewport()->Viewport->GetSizeXY();
        auto Window = FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef());
        Text += FString::Printf(TEXT("client=%dx%d nativeDpi=%.3f font=%.2f split=%.2f\n"),
            View.X, View.Y, Window ? Window->GetDPIScaleFactor() : 0, M->GetSensorToolFontScale(), M->GetDualCameraSplitRatio());
        Text += FString::Printf(TEXT("primary=%s secondary=%s; rows below compare fixture A/B RT with primary/secondary image slot respectively\n"),
            M->CameraComp ? *M->CameraComp->SensorId : TEXT("none"), M->SecondaryCameraComp ? *M->SecondaryCameraComp->SensorId : TEXT("none"));
        for (int32 I = 0; I < 2; ++I)
        {
            auto* C = Cameras[I]->CaptureComponent.Get();
            auto* RT = C->GetCameraRenderTarget();
            const auto Image = I ? M->NativeSecondaryCameraImage : M->NativeViewImage;
            const auto& Brush = I ? M->NativeSecondaryCameraBrush : M->NativeViewBrush;
            Text += FString::Printf(TEXT("%s RT=%p outer=%s target=%p resolution=%dx%d rt=%dx%d available=%lld runtime=%lld brush=%p size=%s desired=%s geometry=%s visibility=%d running=%d\n"),
                *C->SensorId, RT, RT ? *RT->GetOuter()->GetPathName() : TEXT("null"), C->TextureTarget.Get(),
                C->CaptureResolution.X, C->CaptureResolution.Y, RT ? RT->SizeX : 0, RT ? RT->SizeY : 0,
                C->GetAvailableLocalFrameId(), C->GetRuntimeStatus().FrameId, Brush.GetResourceObject(),
                *Brush.ImageSize.ToString(), Image ? *Image->GetDesiredSize().ToString() : TEXT("null"),
                Image ? *Image->GetCachedGeometry().GetAbsoluteSize().ToString() : TEXT("null"), Image ? int32(Image->GetVisibility().IsVisible()) : -1, Cameras[I]->IsSensorRunning());
        }
        FFileHelper::SaveStringToFile(Text, *(Report / (Manual ? TEXT("manual-live.txt") : Label + TEXT(".txt"))));
        if (!Manual) Test->AddInfo(Text);
    }

    void CheckRgb(UVirtualSensorMonitorPanelWidget* M)
    {
        Test->TestTrue(TEXT("two cameras have independent render targets"), Cameras[0]->CaptureComponent->GetCameraRenderTarget() != Cameras[1]->CaptureComponent->GetCameraRenderTarget());
        for (int32 I = 0; I < 2; ++I)
        {
            auto* C = Cameras[I]->CaptureComponent.Get();
            auto* RT = C->GetCameraRenderTarget();
            const auto Image = I ? M->NativeSecondaryCameraImage : M->NativeViewImage;
            const auto& Brush = I ? M->NativeSecondaryCameraBrush : M->NativeViewBrush;
            Test->TestNotNull(TEXT("render target exists"), RT);
            if (!RT || !Image) { Test->AddError(TEXT("missing RGB target or SImage")); continue; }
            Test->TestEqual(TEXT("RT ownership"), RT->GetOuter(), static_cast<UObject*>(C));
            Test->TestEqual(TEXT("capture TextureTarget matches"), C->TextureTarget.Get(), RT);
            Test->TestEqual(TEXT("RT resolution matches settings"), FIntPoint(RT->SizeX, RT->SizeY), C->CaptureResolution);
            Test->TestEqual(TEXT("brush points at its own camera RT"), Brush.GetResourceObject(), static_cast<UObject*>(RT));
            Test->TestEqual(TEXT("brush resolution matches RT"), FVector2D(Brush.ImageSize), FVector2D(RT->SizeX, RT->SizeY));
            const auto Size = Image->GetCachedGeometry().GetAbsoluteSize();
            Test->TestTrue(TEXT("both rendered camera images exceed zero pixels"), Size.X > 1 && Size.Y > 1);
            Test->TestTrue(TEXT("RGB aspect matches its own RT"), Size.Y > 0 && FMath::IsNearlyEqual(float(Size.X / Size.Y), float(RT->SizeX) / RT->SizeY, .02f));
            auto Bytes = C->GetLastJpegSnapshot();
            auto Decoder = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper")).CreateImageWrapper(EImageFormat::JPEG);
            TArray64<uint8> Pixels;
            if (!Test->TestTrue(TEXT("each camera has a decodable nonempty JPEG"), Bytes.IsValid() && Bytes->Num() > 0 && Decoder->SetCompressed(Bytes->GetData(), Bytes->Num()) && Decoder->GetRaw(ERGBFormat::BGRA, 8, Pixels))) continue;
            Test->TestEqual(TEXT("JPEG width matches own RT"), Decoder->GetWidth(), int64(RT->SizeX));
            Test->TestEqual(TEXT("JPEG height matches own RT"), Decoder->GetHeight(), int64(RT->SizeY));
            bool NonBlack = false;
            for (int64 P = 0; P + 3 < Pixels.Num(); P += 4) if (Pixels[P] > 4 || Pixels[P + 1] > 4 || Pixels[P + 2] > 4) { NonBlack = true; break; }
            Test->TestTrue(TEXT("camera JPEG contains visible RGB pixels"), NonBlack);
            FFileHelper::SaveArrayToFile(*Bytes, *(Report / FString::Printf(TEXT("case-%02d-%s.jpg"), Case, *C->SensorId)));
        }
    }
public:
    explicit FDualCamera22Command(FAutomationTestBase* In) : Test(In)
    {
        if (!FParse::Value(FCommandLine::Get(), TEXT("DualCamera22Report="), Report)) Report = FPaths::ProjectSavedDir() / TEXT("Reports/DUAL-CAMERA-22/regression");
        IFileManager::Get().MakeDirectory(*Report, true);
    }
    ~FDualCamera22Command()
    {
        for (auto& Camera : Cameras) if (Camera.IsValid()) { Camera->StopSensor(); Camera->Destroy(); }
    }
    bool Update() override
    {
        const double Now = FPlatformTime::Seconds();
        if (Now - Started > (Manual ? 1800 : 120)) { Test->AddError(TEXT("DUAL-CAMERA-22 timeout")); return true; }
        UWorld* World = nullptr;
        for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::PIE) World = Context.World();
        if (!World || Now < Next) return false;
        if (Phase == 0 && !FinalAcceptanceViewportReady(World)) return false;
        auto* W = World->GetSubsystem<UVirtualSensorToolWorkspaceSubsystem>();
        auto* C = W->GetCoordinator();
        auto* M = Cast<UVirtualSensorMonitorPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Monitor));
        auto* P = Cast<UVirtualSensorPlacementPanelWidget>(W->GetOwnedPanel(ESensorToolPanelRole::Placement));
        auto* Bar = Cast<USensorToolToolbarWidget>(W->GetOwnedNavigation());
        if (!C || !M || !P || !Bar) return false;
        if (Phase == 0)
        {
            C->StopAllSensors();
            C->StreamPublisherComponent->StopAllStreams(TEXT(""));
            // Automatic matrix uses exactly two registered cameras. Existing
            // fixture actors remain stopped in the PIE world, without deletion,
            // transform edits or any map save. Manual sessions keep the inventory.
            if (!Manual)
                for (auto* Existing : C->GetSensorActors())
                    if (Cast<AVirtualCameraSensorActor>(Existing)) C->UnregisterSensorActor(Existing);
            W->ResetOwnedWorkspaceLayout(); W->SetSidebarMode(true);
            for (int32 I = 0; I < 2; ++I)
            {
                Cameras[I] = World->SpawnActor<AVirtualCameraSensorActor>(FVector(-200, I ? 100 : -100, 200), FRotator(-10, I ? -10 : 10, 0));
                Cameras[I]->Tags.Add(TEXT("SensorTestManaged")); Cameras[I]->StopSensor();
                Configure(I, I ? FIntPoint(640, 480) : FIntPoint(640, 360), I == 0 && !Manual);
                C->RegisterSensorActor(Cameras[I].Get());
            }
            if (!Manual) Test->TestEqual(TEXT("fixture registers exactly two cameras"), C->GetCameraCount(), 2);
            C->SharedRecorderComponent->ClearRecording();
            // Interactive placement controls correctly lock during recording.
            // Keep the manual fixture idle unless the operator starts outputs.
            if (!Manual) C->SharedRecorderComponent->StartRecording();
            int32 Port = 0;
            if (FParse::Value(FCommandLine::Get(), TEXT("DualCamera22Port="), Port) && Port > 0)
            {
                FVirtualSensorTransportProfile Profile;
                Profile.BrokerUrl = FString::Printf(TEXT("tcp://127.0.0.1:%d"), Port); Profile.UserName.Empty();
                C->SharedTransportComponent->ConfigureTransportProfile(Profile);
                C->SharedTransportComponent->SetSessionCredentials(FString(), FString());
                C->SharedTransportComponent->TransportMode = EVirtualSensorTransportMode::StompWebSocket;
                for (const auto& Camera : Cameras)
                {
                    FVirtualSensorStreamConfig Stream; Stream.SensorId = Camera->GetSensorId(); Stream.StreamKind = EVirtualSensorStreamKind::CameraImage;
                    Stream.bEnabled = true; Stream.ReceiptSampleInterval = 1; C->StreamPublisherComponent->ConfigureStream(Stream);
                }
            }
            P->SelectEquipment(Cameras[0].Get()); Bar->OpenOwnedDestination(ESensorToolDestination::Preview);
            M->SelectPrimaryCameraBySensorId(TEXT("DUAL22-A"));
            M->CameraTruthDisplayMode = Manual ? 0 : 1;
            M->SelectSecondaryCameraBySensorId(TEXT("DUAL22-B")); M->SetDualCameraModeEnabled(true);
            // Real stopped-primary GT state has no GT preview yet. B is already
            // acquiring RGB. The secondary must not depend on A becoming ready.
            M->CameraTruthDisplayMode = Manual ? 0 : 1;
            Cameras[1]->StartSensor(); if (Manual) Cameras[0]->StartSensor();
            Phase = 1; Next = Now + 1.5; return false;
        }
        if (Phase == 1)
        {
            Observe(World, M, TEXT("primary-not-ready"));
            if (!Manual)
            {
                Test->TestNull(TEXT("primary GT resource is genuinely unready"), Cameras[0]->CaptureComponent->GetGroundTruth() ? Cameras[0]->CaptureComponent->GetGroundTruth()->Preview(1) : nullptr);
                Test->TestTrue(TEXT("secondary RGB acquisition is ready"), Cameras[1]->CaptureComponent->GetAvailableLocalFrameId() >= 0);
                Test->TestEqual(TEXT("unready primary must not starve secondary brush"), M->NativeSecondaryCameraBrush.GetResourceObject(), static_cast<UObject*>(Cameras[1]->CaptureComponent->GetCameraRenderTarget()));
                Test->TestEqual(TEXT("secondary brush tracks actual resolution while primary GT is unready"), FVector2D(M->NativeSecondaryCameraBrush.ImageSize), FVector2D(640, 480));
                const auto Size = M->NativeSecondaryCameraImage->GetCachedGeometry().GetAbsoluteSize();
                Test->TestTrue(TEXT("secondary remains visible with unready primary GT"), Size.X > 1 && Size.Y > 1);
                Configure(0, FIntPoint(640, 360)); M->CameraTruthDisplayMode = 0; Cameras[0]->StartSensor();
            }
            Phase = 2; Next = Now + 1.5; return false;
        }
        if (Manual)
        {
            Observe(World, M, TEXT("manual")); Next = Now + .2;
            return IFileManager::Get().FileExists(*(Report / TEXT("finish.flag")));
        }
        if (Phase == 2)
        {
            Observe(World, M, FString::Printf(TEXT("layout-%02d"), Case)); CheckRgb(M);
            FScreenshotRequest::RequestScreenshot(Report / FString::Printf(TEXT("progress-%02d.png"), Case), true, false);
            if (++Case >= 10) { Phase = 3; Next = Now + .5; return false; }
            if (Case <= 3) M->SetDualCameraSplitRatio(Case == 1 ? .7f : Case == 2 ? .5f : .3f);
            if (Case == 4) { W->SetOwnedPanelFontScale(1.5f); W->SetDockedCardRect(M->GetCurrentPanelPosition(), W->GetDockedCardMinimum()); }
            if (Case == 5) { W->SetPanelOpen(ESensorToolPanelRole::Monitor, false); W->SetPanelOpen(ESensorToolPanelRole::Monitor, true); }
            if (Case == 6) { W->SetSidebarMode(false); W->SetSidebarMode(true); Test->TestTrue(TEXT("rehost keeps monitor instance"), W->GetOwnedPanel(ESensorToolPanelRole::Monitor) == M); }
            if (Case == 7) { Cameras[0]->StopSensor(); Configure(0, FIntPoint(800, 600)); Cameras[0]->StartSensor(); }
            if (Case == 8) { P->SelectEquipment(Cameras[1].Get()); P->SelectEquipment(Cameras[0].Get()); M->SelectSecondaryCameraBySensorId(TEXT("DUAL22-B")); }
            if (Case == 9)
            {
                if (auto Window = FSlateApplication::Get().FindWidgetWindow(World->GetGameViewport()->GetGameViewportWidget().ToSharedRef())) Window->Resize(FVector2D(1100, 700));
            }
            Next = Now + 1; return false;
        }
        if (Phase == 3)
        {
            bool Recorded[2] = {false, false};
            for (int32 I = 0; I < C->SharedRecorderComponent->GetRecordedFrameCount(); ++I)
            {
                FVirtualSensorRecordedFrame Frame;
                if (!C->SharedRecorderComponent->GetRecordedFrame(I, Frame)) continue;
                for (int32 J = 0; J < 2; ++J) if (Frame.SensorId == Cameras[J]->GetSensorId() && !Recorded[J])
                {
                    TSharedPtr<FJsonObject> Json; TArray<uint8> Bytes; TArray64<uint8> Pixels;
                    auto Decoder = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper")).CreateImageWrapper(EImageFormat::JPEG);
                    if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Frame.PayloadJson), Json) && Json.IsValid())
                        Recorded[J] = FBase64::Decode(Json->GetStringField(TEXT("image")), Bytes) && !Bytes.IsEmpty() && Decoder->SetCompressed(Bytes.GetData(), Bytes.Num()) && Decoder->GetRaw(ERGBFormat::BGRA, 8, Pixels) && Decoder->GetWidth() > 0 && Decoder->GetHeight() > 0;
                }
            }
            Test->TestTrue(TEXT("recorder contains image payloads from both camera IDs"), Recorded[0] && Recorded[1]);
            int32 Port = 0;
            if (FParse::Value(FCommandLine::Get(), TEXT("DualCamera22Port="), Port) && Port > 0)
                for (const auto& Camera : Cameras)
                {
                    bool Delivered = false;
                    for (const auto& S : C->StreamPublisherComponent->GetStreamStatuses()) if (S.SensorId == Camera->GetSensorId() && S.StreamKind == EVirtualSensorStreamKind::CameraImage)
                        Delivered = S.SubmittedFrameCount > 0 && S.ReceiptReceivedCount > 0 && S.EncodeFailureCount == 0;
                    Test->TestTrue(TEXT("each camera stream submitted JPEG and received local broker receipt"), Delivered);
                }
            C->SharedRecorderComponent->StopRecording(); C->StreamPublisherComponent->StopAllStreams(TEXT(""));
            return true;
        }
        return false;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDualCamera22Test, "MA0T10.SensorV2.UI.DualCameraPixelsRhi", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDualCamera22Test::RunTest(const FString&)
{
    if (GUsingNullRHI || !FApp::CanEverRender()) { AddInfo(TEXT("SKIP: dual camera pixels require real RHI")); return true; }
    if (!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/SlabScenarioValidationMap"), true)) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartFinalAcceptancePIE());
    ADD_LATENT_AUTOMATION_COMMAND(FDualCamera22Command(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif

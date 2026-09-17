#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Json.h"
#include "LidarGeometryValidationRig.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarSensorActor.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarVisualizationComponent.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorCoordinator.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorTransportComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorStreamPublisherComponent.h"
#include "ma0t10_dt/MA0T10/Core/VirtualSensorHighThroughputTransportSubsystem.h"

class FGeometryStreamCommand:public IAutomationLatentCommand
{
public:
    explicit FGeometryStreamCommand(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        const double Now=FPlatformTime::Seconds();if(Begin==0)Begin=Now;
        UWorld* W=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE){W=C.World();break;}
        // Automation may replace its initial PIE world while StartPIE settles.
        // Never configure a publisher belonging to that retiring world.
        if(ObservedWorld.Get()!=W){ObservedWorld=W;Begin=Now;Configured=false;return false;}
        if(Now-Begin<3)return false;
        ALidarGeometryValidationRig* Rig=nullptr;
        if(W)for(TActorIterator<ALidarGeometryValidationRig> It(W);It;++It){Rig=*It;break;}
        if(!Rig||!Rig->Monitor||!Rig->Coordinator){if(Now-Begin<20)return false;Test->AddError(TEXT("Geometry map not ready"));return true;}
        auto* P=Rig->Coordinator->StreamPublisherComponent.Get();
        auto* H=W->GetSubsystem<UVirtualSensorHighThroughputTransportSubsystem>();
        if(!Configured)
        {
            if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_GEOMETRY_BASELINE"))==TEXT("1"))
                Rig->Sensor->VisualizationComponent->SetColorMode(ELidarColorMode::RelativeHeight);
            auto* T=Rig->Coordinator->SharedTransportComponent.Get();auto Profile=T->GetTransportProfile();
            P->SetTransportComponent(T);
            Rig->Sensor->SetSharedOutputServices(T,nullptr,P);
            // PCD still contains every valid point; bound the unused compatibility JSON derivation.
            Rig->Sensor->ScanComponent->ServerPayloadStride=32;
            Rig->Sensor->ScanComponent->MaxServerPayloadPoints=1024;
            Rig->Sensor->ScanComponent->bIncludeMissPointsInServerPayload=false;
            Profile.BrokerUrl=TEXT("ws://127.0.0.1:61616");Profile.UserName=TEXT("artemis");
            T->ConfigureTransportProfile(Profile);T->SetSessionCredentials(TEXT("artemis"),FString());T->TransportMode=EVirtualSensorTransportMode::StompWebSocket;
            FVirtualSensorStreamConfig C;C.StreamKind=EVirtualSensorStreamKind::PointCloud;C.bEnabled=true;C.FrameStride=1;C.ReceiptSampleInterval=1;
            C.PointCloudFormat=EVirtualPointCloudStreamFormat::PCD;C.PcdDataMode=EVirtualPcdDataMode::Binary;C.DeliveryMode=EVirtualPointCloudDeliveryMode::ConnectedNoLoss;
            C.TransportBackend=EVirtualSensorStreamTransportBackend::TcpStompHighThroughput;P->ConfigureStream(C);
            Configured=true;Start=Now;Last=Now;return false;
        }
        const double Age=Now-Start;
        if(Age>8 && !Logged){Logged=true;for(const auto& S:P->GetStreamStatuses())Test->AddInfo(FString::Printf(TEXT("publisher input=%lld submitted=%lld enabled=%d message=%s"),S.InputFrameCount,S.SubmittedFrameCount,S.bEnabled,*S.Message));}
        if(Age<10){Last=Now;return false;}
        if(StartFrame<0)StartFrame=Rig->Sensor->ScanComponent->GetRuntimeStatus().FrameId;
        if(Age<70)
        {
            Times.Add(FApp::GetDeltaTime()*1000);Wall.Add((Now-Last)*1000);Last=Now;
            Analysis.Add(Rig->Sensor->VisualizationComponent->GetGeometryDiagnostics().ProcessingMs);return false;
        }
        if(!Stopped){EndFrame=Rig->Sensor->ScanComponent->GetRuntimeStatus().FrameId;Rig->Sensor->StopSensor();Stopped=true;}
        FVirtualSensorStreamTelemetry Telemetry;
        for(const auto& S:H->GetStreamTelemetry())if(S.StreamKind==EVirtualSensorStreamKind::PointCloud)Telemetry=S;
        if((Telemetry.ReceiptCount!=Telemetry.SubmittedCount||Telemetry.ConsumerReceivedCount!=Telemetry.SubmittedCount)&&Age<80)return false;
        P->StopAllStreams(FString());
        if(Times.IsEmpty()){Test->AddError(TEXT("No performance samples"));return true;}
        auto Percentile=[](TArray<double> A,double Pct){A.Sort();return A[FMath::Clamp(FMath::CeilToInt(A.Num()*Pct)-1,0,A.Num()-1)];};
        double Sum=0;for(double T:Times)Sum+=T;
        auto Sorted=Times;Sorted.Sort();double Worst=0;const int N=FMath::Max(1,FMath::CeilToInt(Sorted.Num()*.01));for(int I=Sorted.Num()-N;I<Sorted.Num();++I)Worst+=Sorted[I];
        const double Fps=1000/(Sum/Times.Num()),Low=1000/(Worst/N),Hz=(EndFrame-StartFrame)/60.0;
        Test->TestTrue(TEXT("geometry FPS >=55"),Fps>=55);Test->TestTrue(TEXT("geometry 1% low >=45"),Low>=45);Test->TestTrue(TEXT("LiDAR >=19Hz"),Hz>=19);
        Test->TestTrue(TEXT("consumer >=19Hz"),Telemetry.ConsumerHz>=19);Test->TestTrue(TEXT("no invalid/gap/overflow"),Telemetry.ValidationFailureCount==0&&Telemetry.FrameGapCount==0&&Telemetry.OverloadCount==0);
        Test->TestTrue(TEXT("receipt and consumer match submission"),Telemetry.SubmittedCount>0&&Telemetry.SubmittedCount==Telemetry.ReceiptCount&&Telemetry.SubmittedCount==Telemetry.ConsumerReceivedCount);
        auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("average_fps"),Fps);J->SetNumberField(TEXT("one_percent_low"),Low);J->SetNumberField(TEXT("engine_p95_ms"),Percentile(Times,.95));
        J->SetBoolField(TEXT("geometry_enabled"),FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_GEOMETRY_BASELINE"))!=TEXT("1"));
        J->SetNumberField(TEXT("wall_p95_ms"),Percentile(Wall,.95));J->SetNumberField(TEXT("analysis_p95_ms"),Percentile(Analysis,.95));J->SetNumberField(TEXT("lidar_hz"),Hz);J->SetNumberField(TEXT("consumer_hz"),Telemetry.ConsumerHz);
        J->SetNumberField(TEXT("submitted"),Telemetry.SubmittedCount);J->SetNumberField(TEXT("receipts"),Telemetry.ReceiptCount);J->SetNumberField(TEXT("consumed"),Telemetry.ConsumerReceivedCount);
        J->SetNumberField(TEXT("invalid"),Telemetry.ValidationFailureCount);J->SetNumberField(TEXT("gap"),Telemetry.FrameGapCount);J->SetNumberField(TEXT("overflow"),Telemetry.OverloadCount);
        FString Json;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Json));
        IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir()/TEXT("Reports")),true);FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectSavedDir()/TEXT("Reports/geometry_performance.json")));
        Test->AddInfo(Json);return true;
    }
private:
    FAutomationTestBase* Test;double Begin=0,Start=0,Last=0;bool Configured=false,Stopped=false,Logged=false;int64 StartFrame=-1,EndFrame=0;TArray<double>Times,Wall,Analysis;
    TWeakObjectPtr<UWorld> ObservedWorld;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGeometryStreamTest,"MA0T10.LidarGeometry.StreamPerformance",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGeometryStreamTest::RunTest(const FString&)
{
    if(FPlatformMisc::GetEnvironmentVariable(TEXT("MA0T10_GEOMETRY_PERF"))!=TEXT("1")){AddWarning(TEXT("SKIP: set MA0T10_GEOMETRY_PERF=1 with local Artemis"));return true;}
    if(!AutomationOpenMap(TEXT("/Game/MA0T10/Maps/Tests/LidarSemanticValidationMap"),true))return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FGeometryStreamCommand(this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif

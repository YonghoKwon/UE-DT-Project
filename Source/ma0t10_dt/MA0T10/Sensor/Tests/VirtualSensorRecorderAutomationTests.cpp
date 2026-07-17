#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualSensorRecorderComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVirtualSensorRecorderSaveLoadTest, "MA0T10.SensorRecorder.SaveLoadSession", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVirtualSensorRecorderSaveLoadTest::RunTest(const FString& Parameters)
{
    UVirtualSensorRecorderComponent* RecorderComp = NewObject<UVirtualSensorRecorderComponent>();
    UVirtualSensorRecorderComponent* LoadedRecorderComp = NewObject<UVirtualSensorRecorderComponent>();
    TestNotNull(TEXT("recorder component"), RecorderComp);
    TestNotNull(TEXT("loaded recorder component"), LoadedRecorderComp);
    if (!RecorderComp || !LoadedRecorderComp)
    {
        return false;
    }

    const FString SaveSubDirectory = TEXT("Automation/SensorRecorder");
    const FString SaveDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), SaveSubDirectory);
    IFileManager::Get().DeleteDirectory(*SaveDirectory, false, true);

    RecorderComp->SaveSubDirectory = SaveSubDirectory;
    RecorderComp->RecordJsonFrame(TEXT("TEST-SENSOR"), TEXT("virtual_lidar"), 1, TEXT("{\"ignored\":true}"));
    TestEqual(TEXT("record is ignored before StartRecording"), RecorderComp->GetRecordedFrameCount(), 0);

    const FString PayloadA = TEXT("{\"schemaVersion\":\"virtual-lidar.v1\",\"sensorId\":\"TEST-SENSOR\",\"payloadPointCount\":2}");
    const FString PayloadB = TEXT("{\"schemaVersion\":\"virtual-camera.v1\",\"sensorId\":\"TEST-CAMERA\",\"imageBytes\":12}");

    RecorderComp->StartRecording();
    TestTrue(TEXT("recorder is recording"), RecorderComp->IsRecording());
    RecorderComp->RecordJsonFrame(TEXT("TEST-SENSOR"), TEXT("virtual_lidar"), 101, PayloadA);
    RecorderComp->RecordJsonFrame(TEXT("TEST-CAMERA"), TEXT("virtual_camera"), 7, PayloadB);
    RecorderComp->StopRecording();
    TestFalse(TEXT("recorder stopped"), RecorderComp->IsRecording());
    TestEqual(TEXT("recorded frame count"), RecorderComp->GetRecordedFrameCount(), 2);

    FVirtualSensorRecordedFrame FirstFrame;
    TestTrue(TEXT("first recorded frame is readable"), RecorderComp->GetRecordedFrame(0, FirstFrame));
    TestEqual(TEXT("first frame sensor id"), FirstFrame.SensorId, FString(TEXT("TEST-SENSOR")));
    TestEqual(TEXT("first frame type"), FirstFrame.SensorType, FString(TEXT("virtual_lidar")));
    TestEqual(TEXT("first frame id"), FirstFrame.FrameId, static_cast<int64>(101));
    TestEqual(TEXT("first frame payload"), FirstFrame.PayloadJson, PayloadA);

    TestTrue(TEXT("session save succeeds"), RecorderComp->SaveSession(TEXT("automation_recorder")));
    const FString SavedPath = RecorderComp->GetLastSavedSessionPath();
    TestTrue(TEXT("last saved path is set"), !SavedPath.IsEmpty());
    TestTrue(TEXT("session file exists"), IFileManager::Get().FileExists(*SavedPath));

    FString SavedJson;
    TestTrue(TEXT("session JSON can be loaded"), FFileHelper::LoadFileToString(SavedJson, *SavedPath));
    TestTrue(TEXT("session JSON contains session type"), SavedJson.Contains(TEXT("virtual_sensor_recording")));
    TestTrue(TEXT("session JSON contains frame count"), SavedJson.Contains(TEXT("\"frameCount\": 2")) || SavedJson.Contains(TEXT("\"frameCount\":2")));

    TestTrue(TEXT("session load succeeds"), LoadedRecorderComp->LoadSessionFromFile(SavedPath));
    TestEqual(TEXT("loaded frame count"), LoadedRecorderComp->GetRecordedFrameCount(), 2);

    FVirtualSensorRecordedFrame LoadedFirstFrame;
    FVirtualSensorRecordedFrame LoadedSecondFrame;
    TestTrue(TEXT("loaded first frame is readable"), LoadedRecorderComp->GetRecordedFrame(0, LoadedFirstFrame));
    TestTrue(TEXT("loaded second frame is readable"), LoadedRecorderComp->GetRecordedFrame(1, LoadedSecondFrame));
    TestEqual(TEXT("loaded first payload"), LoadedFirstFrame.PayloadJson, PayloadA);
    TestEqual(TEXT("loaded second sensor id"), LoadedSecondFrame.SensorId, FString(TEXT("TEST-CAMERA")));
    TestEqual(TEXT("loaded second payload"), LoadedSecondFrame.PayloadJson, PayloadB);

    RecorderComp->ClearRecording();
    TestEqual(TEXT("clear recording resets frames"), RecorderComp->GetRecordedFrameCount(), 0);
    TestFalse(TEXT("out of range frame read fails"), RecorderComp->GetRecordedFrame(0, FirstFrame));
    return true;
}

#endif

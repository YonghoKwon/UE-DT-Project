#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "m7at10_dt/M7AT10/Sensor/LidarCsvReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/LidarJsonLinesReplaySourceComp.h"
#include "m7at10_dt/M7AT10/Sensor/RealSensorAdapterStubs.h"
#include "m7at10_dt/M7AT10/Sensor/RealSensorSourceComp.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourceBaseStateTest, "M7AT10.RealSensorSource.BaseState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRealSensorSourcePlaceholderStateTest, "M7AT10.RealSensorSource.PlaceholderState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRealSensorSourceBaseStateTest::RunTest(const FString& Parameters)
{
    ULidarCsvReplaySourceComp* CsvReplay = NewObject<ULidarCsvReplaySourceComp>();
    ULidarJsonLinesReplaySourceComp* JsonReplay = NewObject<ULidarJsonLinesReplaySourceComp>();
    TestNotNull(TEXT("CSV replay source"), CsvReplay);
    TestNotNull(TEXT("JSONL replay source"), JsonReplay);

    URealSensorSourceComp* CsvAsSource = Cast<URealSensorSourceComp>(CsvReplay);
    URealSensorSourceComp* JsonAsSource = Cast<URealSensorSourceComp>(JsonReplay);
    TestNotNull(TEXT("CSV replay inherits real sensor source"), CsvAsSource);
    TestNotNull(TEXT("JSONL replay inherits real sensor source"), JsonAsSource);
    TestEqual(TEXT("CSV replay source kind"), CsvAsSource->SourceKind, ERealSensorSourceKind::FileReplay);
    TestEqual(TEXT("JSONL replay source kind"), JsonAsSource->SourceKind, ERealSensorSourceKind::FileReplay);
    TestEqual(TEXT("CSV replay starts stopped"), CsvAsSource->GetConnectionState(), ERealSensorSourceConnectionState::Stopped);
    TestEqual(TEXT("JSONL replay starts stopped"), JsonAsSource->GetConnectionState(), ERealSensorSourceConnectionState::Stopped);
    return true;
}

bool FRealSensorSourcePlaceholderStateTest::RunTest(const FString& Parameters)
{
    TArray<URealSensorSourceComp*> Sources;
    Sources.Add(NewObject<URos2SensorBridgeSourceComp>());
    Sources.Add(NewObject<ULivoxLidarSourceComp>());
    Sources.Add(NewObject<URealSenseCameraSourceComp>());

    for (URealSensorSourceComp* Source : Sources)
    {
        TestNotNull(TEXT("placeholder source"), Source);
        TestFalse(FString::Printf(TEXT("%s StartSource fails until implemented"), *Source->SourceId), Source->StartSource());
        TestEqual(FString::Printf(TEXT("%s reports error state after StartSource"), *Source->SourceId), Source->GetConnectionState(), ERealSensorSourceConnectionState::Error);
        TestFalse(FString::Printf(TEXT("%s PushFrameOnce fails until implemented"), *Source->SourceId), Source->PushFrameOnce(false));
        TestEqual(FString::Printf(TEXT("%s remains error state after PushFrameOnce"), *Source->SourceId), Source->GetConnectionState(), ERealSensorSourceConnectionState::Error);
        TestFalse(FString::Printf(TEXT("%s is not running"), *Source->SourceId), Source->IsSourceRunning());
        TestFalse(FString::Printf(TEXT("%s has status message"), *Source->SourceId), Source->GetLastSourceMessage().IsEmpty());
    }

    return true;
}

#endif

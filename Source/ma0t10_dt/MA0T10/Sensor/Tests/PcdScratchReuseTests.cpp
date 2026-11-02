#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualPcdCodec.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPcdScratchReuseTest,"MA0T10.PcdBandwidth.ScratchWireStability",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPcdScratchReuseTest::RunTest(const FString&)
{
    FVirtualLidarPoint A,B;A.SensorLocalPositionMeters=FVector(1.25,-2.5,3.75);A.Confidence=.75f;
    B.SensorLocalPositionMeters=FVector(-7,8,9);B.Confidence=1;B.RawIntensity=65535;
    TArray<const FVirtualLidarPoint*> Target{&A,&B,&A};
    FVirtualPcdSendOptions Full;Full.bCompressed=true;
    TArray<uint8> Expected,Actual;
    TestTrue(TEXT("full compressed target encodes"),FVirtualPcdCodec::Encode(Target,TEXT("{}"),Full,Expected));
    for(int32 Count:{4096,1,513,0,17})
    {
        TArray<const FVirtualLidarPoint*> Other;for(int32 I=0;I<Count;++I)Other.Add(I%2?&A:&B);
        FVirtualPcdSendOptions Options;Options.bCompressed=true;Options.Profile=EVirtualPcdFieldProfile::Compact;
        TArray<uint8> Bytes;TestTrue(TEXT("different size/profile encodes"),FVirtualPcdCodec::Encode(Other,TEXT("{\"other\":true}"),Options,Bytes));
        FVirtualPcdDocument Decoded;FString Error;TestTrue(TEXT("intermediate payload remains independently readable"),FVirtualPcdCodec::Decode(Bytes,Decoded,Error));
        TestTrue(TEXT("target encodes after scratch reuse"),FVirtualPcdCodec::Encode(Target,TEXT("{}"),Full,Actual));
        TestTrue(TEXT("all header and compressed payload bytes remain identical across pool size/profile reuse"),Actual==Expected);
    }
    return true;
}
#endif

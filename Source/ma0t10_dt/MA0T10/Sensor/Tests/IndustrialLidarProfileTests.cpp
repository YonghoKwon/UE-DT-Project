#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ma0t10_dt/MA0T10/Sensor/IndustrialLidarProfiles.h"
#include "ma0t10_dt/MA0T10/Sensor/VirtualLidarScanComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIndustrialSpecs,"MA0T10.Hatch.IndustrialSpecs",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIndustrialSpecs::RunTest(const FString&)
{
 const int32 Points[]={20000,24000,65536,32000,14400},Echoes[]={1,3,1,2,2};
 const float HFov[]={360,70.4f,360,360,360},VFov[]={59,77.2f,45,31,30},Range[]={4000,19000,4500,8000,10000},Hz[]={10,10,10,20,20};
 int I=0;for(const auto& S:IndustrialLidar::Specs())
 {
  const auto P=UVirtualLidarScanComponent::ResolveProfilePreset(S.Profile,EVirtualSensorSimulationQuality::FullSpec);
  TestEqual(TEXT("manufacturer point budget at selected mode"),P.HorizontalSamples*P.VerticalChannels,Points[I]);
  TestEqual(TEXT("published horizontal FOV"),P.HorizontalFovDegrees,HFov[I]);TestEqual(TEXT("published vertical FOV"),P.MaxVerticalAngleDegrees-P.MinVerticalAngleDegrees,VFov[I]);
  TestEqual(TEXT("documented conditional range limit"),P.MaxDistanceCm,Range[I]);TestEqual(TEXT("selected frame interval"),P.ScanIntervalSeconds,1.f/Hz[I]);TestEqual(TEXT("published echo capability"),P.DeviceSpec.MaxEchoesPerPixel,Echoes[I]);
  TestTrue(TEXT("official traceable source and dated noncalibrated status"),FString(S.Source).StartsWith(TEXT("https://"))&&P.DeviceSpec.Notes.Contains(TEXT("2026-10-21"))&&!P.DeviceSpec.bProtocolVerifiedAgainstHardware);
  const auto A=IndustrialLidar::Beam(S.Profile,0,P.HorizontalSamples,P.VerticalChannels,1,P.MinVerticalAngleDegrees,-P.HorizontalFovDegrees*.5f),B=IndustrialLidar::Beam(S.Profile,0,P.HorizontalSamples,P.VerticalChannels,2,P.MinVerticalAngleDegrees,-P.HorizontalFovDegrees*.5f);
  TestEqual(TEXT("only nonrepeating envelopes change frame phase"),!A.Equals(B,.001),I<2);
  for(int R=0;R<Points[I];R+=37){const auto D=IndustrialLidar::Beam(S.Profile,R,P.HorizontalSamples,P.VerticalChannels,3,FMath::Lerp(P.MinVerticalAngleDegrees,P.MaxVerticalAngleDegrees,float(R/P.HorizontalSamples)/(P.VerticalChannels-1)),-180.f+360.f*(R%P.HorizontalSamples)/P.HorizontalSamples);TestTrue(TEXT("every sampled physical beam is inside declared envelope"),D.Pitch>=P.MinVerticalAngleDegrees-1.e-4&&D.Pitch<=P.MaxVerticalAngleDegrees+1.e-4&&FMath::Abs(D.Yaw)<=P.HorizontalFovDegrees*.5+1.e-4);}
  ++I;
 }
 TestEqual(TEXT("five added device choices"),I,5);
 const auto Old=UVirtualLidarScanComponent::ResolveProfilePreset(EVirtualLidarDeviceProfile::LivoxMid360S,EVirtualSensorSimulationQuality::FullSpec);TestEqual(TEXT("legacy default ray budget unchanged"),Old.HorizontalSamples*Old.VerticalChannels,21600);return true;
}
#endif

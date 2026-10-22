#include "IndustrialLidarProfiles.h"

namespace IndustrialLidar
{
static const FSpec Table[]={
 {EVirtualLidarDeviceProfile::LivoxMid360,TEXT("Livox"),TEXT("Mid-360"),TEXT("https://www.livoxtech.com/mid-360/specs"),TEXT("Nonrepeating envelope"),TEXT("40m @10%,100klx; precision 1sigma @10m,80%; beam count unpublished"),0,400,50,200000,1,360,-7,52,.1,40,10,20,905},
 {EVirtualLidarDeviceProfile::LivoxAvia,TEXT("Livox"),TEXT("Avia"),TEXT("https://www.livoxtech.com/avia/specs"),TEXT("Nonrepeating envelope"),TEXT("190m @10%,100klx; precision 1sigma @20m,80%; 100ms integration chosen; beam count unpublished"),0,400,60,240000,3,70.4f,-38.6f,38.6f,1,190,10,20,905},
 {EVirtualLidarDeviceProfile::OusterOS1_64_Rev05,TEXT("Ouster"),TEXT("OS1-64 Rev05 (1024x10Hz)"),TEXT("https://data.ouster.io/downloads/datasheets/datasheet-rev05-v2p1-os1.pdf"),TEXT("Rotating uniform 64"),TEXT("45m @10%,>90% detection,100klx; 1sigma20mm @20-50m; accuracy +/-30mm; 655360pts/s chosen mode,1310720 maximum"),64,1024,64,655360,1,360,-22.5f,22.5f,.3,45,10,20,865},
 {EVirtualLidarDeviceProfile::HesaiXT32,TEXT("Hesai"),TEXT("XT32 (20Hz)"),TEXT("https://www.hesaitech.com/wp-content/uploads/2026/07/PandarXT_User_Manual_X01-en-260710.pdf"),TEXT("Rotating 32"),TEXT("80m @10% channels9-24;50m outer channels; typical accuracy +/-10mm; 1sigma precision5mm typical @0.5-70m,50%,channels9-24;standard20mm"),32,1000,32,640000,2,360,-16,15,.05,80,20,5,905},
 {EVirtualLidarDeviceProfile::VelodyneVLP16,TEXT("Velodyne"),TEXT("VLP-16 (20Hz)"),TEXT("https://ouster.com/downloads/velodyne-downloads"),TEXT("Rotating 16"),TEXT("100m nominal; reflectivity condition unpublished; accuracy +/-30mm, not 1sigma;20Hz/0.4deg=288000pts/s,~300000 nominal"),16,900,16,288000,2,360,-15,15,1,100,20,30,903},
 {EVirtualLidarDeviceProfile::OusterOS1_64_Rev7,TEXT("Ouster"),TEXT("OS1-64 Rev7 (1024x20Hz)"),TEXT("https://data.ouster.io/downloads/datasheets/datasheet-rev7-v3p0-os1.pdf"),TEXT("Rotating uniform 64; uncalibrated beam spacing"),TEXT("90m simulation envelope from 10%,>90% detection,100klx,1024x10Hz datasheet; 20Hz mode supported but same detection range at20Hz unverified; conservative1sigma30mm; single-return1310720pts/s; checked2026-10-22"),64,1024,64,1310720,2,360,-21.2f,21.2f,.5,90,20,30,865}
};
TConstArrayView<FSpec> Specs(){return MakeArrayView(Table);}
const FSpec* Find(EVirtualLidarDeviceProfile P){for(const auto& S:Table)if(S.Profile==P)return &S;return nullptr;}
bool Resolve(EVirtualLidarDeviceProfile P,EVirtualSensorSimulationQuality Q,FVirtualLidarProfilePreset& R)
{
 const auto* S=Find(P);if(!S)return false;
 R.ProfileClass=EVirtualLidarProfileClass::PublicSpecNative;R.FidelityMode=EVirtualSensorFidelityMode::PublicSpecBased;
 R.RecommendedBackend=EVirtualLidarAcquisitionBackend::AccurateCpuTrace;
 R.ProfileKey=FString::Printf(TEXT("industrial-%d"),int32(P));R.CalibrationId=TEXT("public-envelope-2026-10-21-not-calibrated");
 auto& D=R.DeviceSpec;D.Manufacturer=S->Maker;D.Model=S->Model;D.HorizontalFovDegrees=S->HFov;D.VerticalFovDegrees=S->Top-S->Bottom;
 D.MinRangeCm=S->MinMeters*100;D.TypicalRangeCm=D.MaxRangeCm=S->RangeMeters*100;D.FrameRateHz=S->Hz;
 D.Width=S->Columns;D.Height=S->Rows;D.PointRate=S->PointRate;D.MaxEchoesPerPixel=S->Echoes;
 D.MaxDistanceErrorMillimeters=S->PrecisionMM;D.WavelengthNanometers=S->Wavelength;D.ReferenceReflectivityPercent=P==EVirtualLidarDeviceProfile::VelodyneVLP16?0:10;D.ReferenceAmbientLux=100000;
 D.HorizontalAngularResolutionDegrees=S->Beams?360.f/S->Columns:0;D.VerticalAngularResolutionDegrees=S->Beams?(S->Top-S->Bottom)/(S->Beams-1):0;
 D.OutputFields=TEXT("XYZ + intensity");D.Notes=FString(S->Pattern)+TEXT("; ")+S->Conditions+TEXT("; checked2026-10-21; ")+S->Source;
 R.ScanIntervalSeconds=1.f/S->Hz;R.MaxDistanceCm=S->RangeMeters*100;R.HorizontalFovDegrees=S->HFov;R.MinVerticalAngleDegrees=S->Bottom;R.MaxVerticalAngleDegrees=S->Top;
 const int32 Div=Q==EVirtualSensorSimulationQuality::Debug?4:Q==EVirtualSensorSimulationQuality::RealTimePreview?2:1;
 R.HorizontalSamples=FMath::Max(1,S->Columns/Div);R.VerticalChannels=FMath::Max(1,S->Rows/Div);
 R.PreviewPointStride=FMath::Max(1,R.HorizontalSamples*R.VerticalChannels/12000);R.MaxPreviewPoints=12000;return true;
}
FRotator Beam(EVirtualLidarDeviceProfile P,int32 Ray,int32 W,int32 H,int64 Frame,float Pitch,float Yaw)
{
 if(P==EVirtualLidarDeviceProfile::LivoxMid360||P==EVirtualLidarDeviceProfile::LivoxAvia)
 {
  const auto* S=Find(P);const double T=double(Frame)*W*H+Ray;
  // Equal-area nonrepeating coverage approximation; not a proprietary optical trajectory.
  const double U=FMath::Frac((T+.5)*.6180339887498949),V=FMath::Frac((T+.5)*.7548776662466927);
  return FRotator(FMath::Lerp(double(S->Bottom),double(S->Top),V),(U-.5)*S->HFov,0);
 }
 if(P==EVirtualLidarDeviceProfile::OusterOS1_64_Rev05||P==EVirtualLidarDeviceProfile::OusterOS1_64_Rev7||P==EVirtualLidarDeviceProfile::HesaiXT32||P==EVirtualLidarDeviceProfile::VelodyneVLP16)
  Yaw=-180.f+360.f*(Ray%W)/W; // spinning scans have no duplicated seam
 return FRotator(Pitch,Yaw,0);
}
}

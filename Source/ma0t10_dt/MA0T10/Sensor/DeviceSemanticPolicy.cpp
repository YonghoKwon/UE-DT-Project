#include "DeviceSemanticPolicy.h"
#include "VirtualLidarScanComponent.h"
namespace { TArray<FVirtualLidarSemanticClassRule> HostRules; TMap<FName,float> HostReflectances; FName HostDiagnosticLabel,HostDiagnosticTag; }
void DeviceSemanticPolicy::Configure(TArray<FVirtualLidarSemanticClassRule> Rules,TMap<FName,float> Reflectances,FName DiagnosticLabel,FName DiagnosticTag)
{ check(IsInGameThread()); HostRules=MoveTemp(Rules); HostReflectances=MoveTemp(Reflectances);HostDiagnosticLabel=DiagnosticLabel;HostDiagnosticTag=DiagnosticTag; }
const TArray<FVirtualLidarSemanticClassRule>& DeviceSemanticPolicy::Rules() { return HostRules; }
float DeviceSemanticPolicy::Reflectance(FName Label) { const float* Value=HostReflectances.Find(Label);return Value?*Value:.20f; }
FName DeviceSemanticPolicy::DiagnosticLabel() { return HostDiagnosticLabel; }
FName DeviceSemanticPolicy::DiagnosticTag() { return HostDiagnosticTag; }

#pragma once
#include "CoreMinimal.h"

/** Owned workspace vocabulary. Does not replace Designer or legacy toolbar text. */
namespace SensorToolUiText
{
#define LOCTEXT_NAMESPACE "SensorToolUiText"
    inline FText Equipment() { return LOCTEXT("Equipment", "장비"); }
    inline FText NoEquipment() { return LOCTEXT("NoEquipment", "장비 없음"); }
    inline FText Placement() { return LOCTEXT("Placement", "배치"); }
    inline FText Preview() { return LOCTEXT("Preview", "미리보기"); }
    inline FText Scenario() { return LOCTEXT("Scenario", "시나리오"); }
    inline FText PcdTransfer() { return LOCTEXT("PcdTransfer", "PCD 전송"); }
    inline FText Results() { return LOCTEXT("Results", "결과"); }
    inline FText Tools() { return LOCTEXT("Tools", "도구"); }
    inline FText SensorSettings() { return LOCTEXT("SensorSettings", "센서 상세 설정"); }
    inline FText FileSave() { return LOCTEXT("FileSave", "파일 저장"); }
    inline FText ServerSettings() { return LOCTEXT("ServerSettings", "서버 연결 설정"); }
    inline FText Diagnostics() { return LOCTEXT("Diagnostics", "진단"); }
    inline FText Compatibility() { return LOCTEXT("Compatibility", "고급·호환"); }
    inline FText ScenarioCharts() { return LOCTEXT("ScenarioCharts", "시나리오 차트"); }
    inline FText Appearance() { return LOCTEXT("Appearance", "UI 표시"); }
    inline FText Measuring() { return LOCTEXT("Measuring", "측정 중"); }
    inline FText Stopped() { return LOCTEXT("Stopped", "정지"); }
    inline FText Starved() { return LOCTEXT("Starved", "정체"); }
    inline FText Error() { return LOCTEXT("Error", "오류"); }
    inline FText Paused() { return LOCTEXT("Paused", "일시정지"); }
    inline FText EquipmentCount(int32 Total, int32 Running) { return FText::Format(LOCTEXT("EquipmentCount", "장비 {0}대(측정 {1})"), Total, Running); }
    inline FText PcdState(bool Sending) { return Sending ? LOCTEXT("PcdSending", "PCD 전송 중") : LOCTEXT("PcdStopped", "PCD 전송 중지"); }
    inline FText ServerState(bool Connected) { return Connected ? LOCTEXT("ServerConnected", "서버 연결됨") : LOCTEXT("ServerDisconnected", "서버 연결 안 됨"); }
    inline FText ScenarioWaiting() { return LOCTEXT("ScenarioWaiting", "시나리오 대기"); }
    inline FText ScenarioPreparing() { return LOCTEXT("ScenarioPreparing", "시나리오 준비 중"); }
    inline FText ScenarioPaused() { return LOCTEXT("ScenarioPaused", "시나리오 일시정지"); }
    inline FText ScenarioDraining() { return LOCTEXT("ScenarioDraining", "시나리오 정리 중"); }
    inline FText ScenarioRunning(float Progress) { return FText::Format(LOCTEXT("ScenarioRunning", "시나리오 실행 {0}%"), FMath::RoundToInt(Progress * 100)); }
    inline FText ConnectionRequired() { return LOCTEXT("ConnectionRequired", "연결 설정 필요 · 서버 연결 설정을 확인하세요"); }
    inline FText EquipmentRequired() { return LOCTEXT("EquipmentRequired", "장비 없음 · 배치에서 장비를 추가하세요"); }
    inline FText EquipmentCheck() { return LOCTEXT("EquipmentCheck", "장비 설정 확인 필요 · PCD 전송에서 확인하세요"); }
    inline FText TransferCheck() { return LOCTEXT("TransferCheck", "전송 상태 확인 필요 · 진단에서 확인하세요"); }
    inline FText Warning(const FText& Reason) { return FText::Format(LOCTEXT("Warning", "⚠ {0}"), Reason); }
    inline FText FontPercent(float Scale) { return FText::Format(LOCTEXT("FontPercent", "글자 {0}%"), FMath::RoundToInt(Scale * 100)); }
    inline FText ResetUi() { return LOCTEXT("ResetUi", "센서 도구 UI 초기화"); }
#undef LOCTEXT_NAMESPACE
}

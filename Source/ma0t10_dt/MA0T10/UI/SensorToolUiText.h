#pragma once
#include "CoreMinimal.h"

/** Owned workspace vocabulary. Does not replace Designer or legacy toolbar text. */
namespace SensorToolUiText
{
#define LOCTEXT_NAMESPACE "SensorToolUiText"
    inline FText Equipment() { return LOCTEXT("Equipment", "장비"); }
    inline FText NoEquipment() { return LOCTEXT("NoEquipment", "장비 없음"); }
    inline FText AddEquipment() { return LOCTEXT("AddEquipment", "+ 장비 추가 (배치)"); }
    inline FText Placement() { return LOCTEXT("Placement", "배치"); }
    inline FText RailPreview() { return LOCTEXT("RailPreview", "보기"); }
    inline FText RailTransfer() { return LOCTEXT("RailTransfer", "전송"); }
    inline FText BackPlacement() { return LOCTEXT("BackPlacement", "← 배치"); }
    inline FText BackProgress() { return LOCTEXT("BackProgress", "← 시나리오 진행"); }
    inline FText BackSettings() { return LOCTEXT("BackSettings", "← 센서 상세 설정"); }
    inline FText BackTools() { return LOCTEXT("BackTools", "← 도구"); }
    inline FText BackTransfer() { return LOCTEXT("BackTransfer", "← PCD 전송"); }
    inline FText BackResults() { return LOCTEXT("BackResults", "← 결과"); }
    inline FText BackFile() { return LOCTEXT("BackFile", "← 파일 저장"); }
    inline FText BackServer() { return LOCTEXT("BackServer", "← 서버 연결 설정"); }
    inline FText BackDiagnostics() { return LOCTEXT("BackDiagnostics", "← 진단"); }
    inline FText BackHint() { return LOCTEXT("BackHint", "상위 화면으로 돌아갑니다. 조작 중에는 조작을 종료한 뒤 돌아가세요."); }
    inline FText DefaultWidth() { return LOCTEXT("DefaultWidth", "기본 폭"); }
    inline FText WideWidth() { return LOCTEXT("WideWidth", "넓게"); }
    inline FText WidthHint() { return LOCTEXT("WidthHint", "사이드바 폭을 기본 / 넓게 전환합니다."); }
    inline FText ResizeHint() { return LOCTEXT("ResizeHint", "모서리를 드래그해 창 크기를 조절합니다."); }
    inline FText Preview() { return LOCTEXT("Preview", "미리보기"); }
    inline FText Scenario() { return LOCTEXT("Scenario", "시나리오"); }
    inline FText ScenarioProgress() { return LOCTEXT("ScenarioProgress", "시나리오 · 진행"); }
    inline FText ArchiveList() { return LOCTEXT("ArchiveList", "보관 목록"); }
    inline FText DisplaySettings() { return LOCTEXT("DisplaySettings", "표시 설정"); }
    inline FText SlabInformation() { return LOCTEXT("SlabInformation", "Slab 정보"); }
    inline FText SurfaceSupportReason(const FString& Reason) { return Reason.Contains(TEXT("Oxidation"))?LOCTEXT("UnsupportedOxidationSurface","현재 재질은 산화 스케일 표현을 지원하지 않습니다"):FText::FromString(Reason); }
    inline FText SurfacePreset(int32 P)
    {
        switch(P){case 1:return LOCTEXT("HotRed","고온 적열");case 2:return LOCTEXT("OxidationScale","산화 스케일");case 3:return LOCTEXT("CooledSteel","냉각 강판");case 4:return LOCTEXT("AnalysisMatte","분석용 무광");default:return LOCTEXT("CurrentSurface","기본 (현재 값)");}
    }
    inline FText LinkSlabReflectivity() { return LOCTEXT("LinkSlabReflectivity","LiDAR 반사율도 함께 적용"); }
    inline FText ObservationFallback() { return LOCTEXT("ObservationFallback", "연결 실패 시 관찰 전용"); }
    inline FText PcdTransfer() { return LOCTEXT("PcdTransfer", "PCD 전송"); }
    inline FText TransferTab() { return LOCTEXT("TransferTab", "전송"); }
    inline FText Data() { return LOCTEXT("Data", "데이터"); }
    inline FText ServerTab() { return LOCTEXT("ServerTab", "서버 연결"); }
    inline FText NewScenario() { return LOCTEXT("NewScenario", "신규 시나리오"); }
    inline FText ArchiveReplay() { return LOCTEXT("ArchiveReplay", "보관 목록 재생"); }
    inline FText PcdOutput() { return LOCTEXT("PcdOutput", "PCD"); }
    inline FText ExtraOutputs() { return LOCTEXT("ExtraOutputs", "추가 출력"); }
    inline FText CameraOutput() { return LOCTEXT("CameraOutput", "Camera 이미지"); }
    inline FText LidarOutput() { return LOCTEXT("LidarOutput", "LiDAR 정보"); }
    inline FText RequireConnection() { return LOCTEXT("RequireConnection", "연결 확인 후 시작"); }
    inline FText ObservationOnly() { return LOCTEXT("ObservationOnly", "관찰 전용"); }
    inline FText ChangeInTransfer() { return LOCTEXT("ChangeInTransfer", "PCD 전송에서 변경"); }
    inline FText DirectTransfer() { return LOCTEXT("DirectTransfer", "시나리오 없이 직접 전송"); }
    inline FText PreflightDetails() { return LOCTEXT("PreflightDetails", "사전 점검 상세"); }
    inline FText PreflightTarget() { return LOCTEXT("PreflightTarget", "점검 대상"); }
    inline FText DirectTarget() { return LOCTEXT("DirectTarget", "직접 전송"); }
    inline FText PcdServerRequired() { return LOCTEXT("PcdServerRequired", "PCD 전송에는 서버 연결 설정이 필요합니다"); }
    inline FString UserReason(FString Reason);
    inline FString CompactEquipmentTitle(const FString& Status)
    {
        const int32 StateStart=Status.Find(TEXT(" · "));
        return StateStart>28?Status.Left(27)+TEXT("…")+Status.Mid(StateStart):Status;
    }
    inline FString PreflightReason(const FString& Reason)
    {
        if(Reason.Contains(TEXT("STOMP")))return PcdServerRequired().ToString();
        if(Reason.Contains(TEXT("Broker URL"))||Reason.Contains(TEXT("Topic")))return LOCTEXT("OutputAddressRequired", "서버 연결과 출력 주소를 설정하세요").ToString();
        if(Reason.Contains(TEXT("Coordinator")))return LOCTEXT("EquipmentConnectionRequired", "장비와 전송 연결을 확인하세요").ToString();
        if(Reason.Contains(TEXT("중복 SensorId")))return LOCTEXT("DuplicateEquipmentId", "장비 ID가 겹칩니다 · 배치에서 확인하세요").ToString();
        if(Reason.Contains(TEXT("선택 센서 없음"))||Reason.Contains(TEXT("대상 SensorId 없음")))return LOCTEXT("SelectOutputEquipment", "전송할 장비를 배치나 상단 목록에서 선택하세요").ToString();
        if(Reason.Contains(TEXT("LiDAR가 없")))return LOCTEXT("LidarOutputRequired", "전송할 LiDAR가 없습니다 · 배치에서 추가하거나 대상을 확인하세요").ToString();
        if(Reason.Contains(TEXT("Camera가 없")))return LOCTEXT("CameraOutputRequired", "전송할 Camera가 없습니다 · 배치에서 추가하거나 대상을 확인하세요").ToString();
        if(Reason.Contains(TEXT("측정 주기")))return LOCTEXT("MeasurementPeriodRequired", "장비의 측정 주기를 확인하세요").ToString();
        if(Reason.Contains(TEXT("수집 설정")))return LOCTEXT("EquipmentSettingsRequired", "장비 설정을 읽을 수 없습니다 · 센서 상세 설정에서 확인하세요").ToString();
        return UserReason(Reason);
    }
    inline FText Advanced() { return LOCTEXT("Advanced", "고급"); }
    inline FText Ready() { return LOCTEXT("Ready", "준비됨"); }
    inline FString OutputSummary(bool Pcd,bool Camera,bool Lidar)
    {
        TArray<FString> Names;
        if(Pcd)Names.Add(PcdOutput().ToString());
        if(Camera)Names.Add(CameraOutput().ToString());
        if(Lidar)Names.Add(LidarOutput().ToString());
        return Names.IsEmpty()?ObservationOnly().ToString():FString::Join(Names,TEXT(" · "));
    }
    inline FString UserReason(FString Reason)
    {
        // Presentation only: original API results, logs, wire values and saved settings stay intact.
        Reason.ReplaceInline(TEXT("Server payload is uncapped (MaxServerPayloadPoints=0)"),TEXT("추가 출력 데이터의 점 수 제한이 없습니다"),ESearchCase::IgnoreCase);
        Reason.ReplaceInline(TEXT("Preview is uncapped (MaxPreviewPoints=0)"),TEXT("미리보기 점 수 제한이 없습니다"),ESearchCase::IgnoreCase);
        Reason.ReplaceInline(TEXT("LiDAR trace"),TEXT("LiDAR 측정"),ESearchCase::IgnoreCase);
        Reason.ReplaceInline(TEXT("STOMP 서버"),TEXT("서버"),ESearchCase::IgnoreCase);
        Reason.ReplaceInline(TEXT("연결·진단에서"),TEXT("서버 연결 설정에서"));
        const TCHAR* OldLabels[]={TEXT("센서 Actor"),TEXT("Capture/Export"),TEXT("캡처/내보내기"),TEXT("미리보기 시작"),TEXT("미리보기 중지"),TEXT("내보내기"),TEXT("송신"),TEXT("PIE"),TEXT("V2")};
        const TCHAR* CurrentLabels[]={TEXT("장비"),TEXT("데이터"),TEXT("데이터"),TEXT("측정 시작"),TEXT("측정 중지"),TEXT("파일 저장"),TEXT("전송"),TEXT("실행"),TEXT("장비")};
        for(int32 I=0;I<UE_ARRAY_COUNT(OldLabels);++I)Reason.ReplaceInline(OldLabels[I],CurrentLabels[I],ESearchCase::IgnoreCase);
        const TCHAR* From[]={TEXT("Payload"),TEXT("Topic"),TEXT("receipt"),TEXT("Broker"),TEXT("Raw TCP"),TEXT("coherent"),TEXT("revision"),TEXT("p95"),TEXT("Coordinator"),TEXT("SensorId"),TEXT("snapshot"),TEXT("generation"),TEXT("backend"),TEXT("socket"),TEXT("FIFO"),TEXT("STOMP")};
        const TCHAR* To[]={TEXT("데이터"),TEXT("출력 주소"),TEXT("서버 수락"),TEXT("서버"),TEXT("전송 연결"),TEXT("일치 상태"),TEXT("설정 버전"),TEXT("지연"),TEXT("장비 연결"),TEXT("장비 ID"),TEXT("기록"),TEXT("실행 세대"),TEXT("측정 방식"),TEXT("연결"),TEXT("순서대로 전송"),TEXT("서버 연결")};
        for(int32 I=0;I<UE_ARRAY_COUNT(From);++I)Reason.ReplaceInline(From[I],To[I],ESearchCase::IgnoreCase);
        Reason.ReplaceInline(TEXT("\n"),TEXT(" · "));
        Reason.ReplaceInline(TEXT("Slab Actor"),TEXT("시나리오 연결"));
        Reason.ReplaceInline(TEXT("Point Cloud"),TEXT("PCD"));
        Reason.ReplaceInline(TEXT("deadline"),TEXT("처리 시간 한도"),ESearchCase::IgnoreCase);
        Reason.ReplaceInline(TEXT("wildcard"),TEXT("전체 장비"),ESearchCase::IgnoreCase);
        Reason.ReplaceInline(TEXT("e2e"),TEXT("전체 경로"),ESearchCase::IgnoreCase);
        return Reason;
    }
    inline FText Results() { return LOCTEXT("Results", "결과"); }
    inline FText Tools() { return LOCTEXT("Tools", "도구"); }
    inline FText SensorSettings() { return LOCTEXT("SensorSettings", "센서 상세 설정"); }
    inline FText FileSave() { return LOCTEXT("FileSave", "파일 저장"); }
    inline FText FileSaveWaiting() { return LOCTEXT("FileSaveWaiting", "파일을 저장하면 처리 결과가 표시됩니다."); }
    inline FText ServerSettings() { return LOCTEXT("ServerSettings", "서버 연결 설정"); }
    inline FText Diagnostics() { return LOCTEXT("Diagnostics", "진단"); }
    inline FText Compatibility() { return LOCTEXT("Compatibility", "고급·호환"); }
    inline FText ScenarioCharts() { return LOCTEXT("ScenarioCharts", "시나리오 차트"); }
    inline FText Appearance() { return LOCTEXT("Appearance", "UI 표시"); }
    inline FText Measuring() { return LOCTEXT("Measuring", "측정 중"); }
    inline FText StartMeasurement() { return LOCTEXT("StartMeasurement", "측정 시작"); }
    inline FText StopMeasurement() { return LOCTEXT("StopMeasurement", "측정 중지"); }
    inline FText HidePanelHint() { return LOCTEXT("HidePanelHint", "패널만 숨깁니다. 측정·전송·재생은 계속됩니다."); }
    inline FText RealtimeConnection() { return LOCTEXT("RealtimeConnection", "방식: 실시간 전송"); }
    inline FText ServerAddress() { return LOCTEXT("ServerAddress", "서버 주소"); }
    inline FText PcdAddress() { return LOCTEXT("PcdAddress", "PCD 출력 주소"); }
    inline FText CameraAddress() { return LOCTEXT("CameraAddress", "Camera 이미지 출력 주소"); }
    inline FText LidarAddress() { return LOCTEXT("LidarAddress", "LiDAR 정보 출력 주소"); }
    inline FText ConsumerAckAddress() { return LOCTEXT("ConsumerAckAddress", "외부 프로그램 처리 확인 주소 · 선택 사항"); }
    inline FText Stopped() { return LOCTEXT("Stopped", "정지"); }
    inline FText Starved() { return LOCTEXT("Starved", "정체"); }
    inline FText Error() { return LOCTEXT("Error", "오류"); }
    inline FText Paused() { return LOCTEXT("Paused", "일시정지"); }
    inline FText EquipmentCount(int32 Total, int32 Running) { return FText::Format(LOCTEXT("EquipmentCount", "장비 {0}대(측정 {1})"), Total, Running); }
    inline FText PcdState(bool Sending) { return Sending ? LOCTEXT("PcdSending", "PCD 전송 중") : LOCTEXT("PcdStopped", "PCD 전송 중지"); }
    inline FText PcdOverloadSuspended() { return LOCTEXT("PcdOverloadSuspended", "PCD 과부하 · 잠시 후 자동 재개"); }
    inline FText PcdResumeDetails(float Seconds, int64 Count)
    { return FText::Format(LOCTEXT("PcdResumeDetails", "자동 재개까지 최소 {0}초 · 연결과 대기 전송 정리 후 재개 · 재개 {1}회"), FMath::CeilToInt(Seconds), FText::AsNumber(Count)); }
    inline FText ServerState(bool Connected) { return Connected ? LOCTEXT("ServerConnected", "서버 연결됨") : LOCTEXT("ServerDisconnected", "서버 연결 안 됨"); }
    inline FText ScenarioWaiting() { return LOCTEXT("ScenarioWaiting", "시나리오 대기"); }
    inline FText ScenarioPreparing() { return LOCTEXT("ScenarioPreparing", "시나리오 준비 중"); }
    inline FText ScenarioPaused() { return LOCTEXT("ScenarioPaused", "시나리오 일시정지"); }
    inline FText ScenarioDraining() { return LOCTEXT("ScenarioDraining", "시나리오 정리 중"); }
    inline FText ScenarioRunning(float Progress) { return FText::Format(LOCTEXT("ScenarioRunning", "시나리오 실행 {0}%"), FMath::RoundToInt(Progress * 100)); }
    inline FText ConnectionRequired() { return LOCTEXT("ConnectionRequired", "연결 설정 필요 · 서버 연결 설정을 확인하세요"); }
    inline FText ConnectionWarning(bool HasSettings,double DisconnectedSeconds)
    { return HasSettings&&DisconnectedSeconds<5 ? LOCTEXT("ServerReconnecting", "서버 재연결 중 · 잠시 기다려 주세요") : ConnectionRequired(); }
    inline FText SendDelayed() { return LOCTEXT("SendDelayed", "전송 지연 · 전송량과 서버 수신 상태를 확인하세요"); }
    inline FText EquipmentRequired() { return LOCTEXT("EquipmentRequired", "장비 없음 · 배치에서 장비를 추가하세요"); }
    inline FText EquipmentCheck() { return LOCTEXT("EquipmentCheck", "장비 설정 확인 필요 · PCD 전송에서 확인하세요"); }
    inline FText TransferCheck() { return LOCTEXT("TransferCheck", "전송 상태 확인 필요 · 진단에서 확인하세요"); }
    inline FText Warning(const FText& Reason) { return FText::Format(LOCTEXT("Warning", "⚠ {0}"), Reason); }
    inline FText FontPercent(float Scale) { return FText::Format(LOCTEXT("FontPercent", "글자 {0}%"), FMath::RoundToInt(Scale * 100)); }
    inline FText ResetUi() { return LOCTEXT("ResetUi", "센서 도구 UI 초기화"); }
#undef LOCTEXT_NAMESPACE
}

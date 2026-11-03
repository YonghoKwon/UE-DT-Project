"""NAMING-01 proposed file moves for Source/ma0t10_dt/MA0T10/{UI,Scenario}. Read-only; prints a Markdown table.

Usage: python -X utf8 -I movemap.py <host-root>
Rules are first-match; anything unmatched is reported as 확인 필요.
"""
import os, re, sys

root = os.path.join(sys.argv[1], "Source", "ma0t10_dt", "MA0T10")
RULES = {
    "UI": [
        (r"^Tests/", "UI/Tests/", "테스트는 위치 유지"),
        (r"^Main/", "UI/Main/", "기존 Main 유지(특정 맵 메인 위젯)"),
        (r"^(HatchMemory|LidarMapMemory|LidarMemoryAttribution|PcdTransportRuntime)", "Diagnostics/", "UI가 아닌 측정 probe"),
        (r"^(VirtualSensorTransformGizmoActor|VirtualSensorToolRig)", "Sensor/Tool/", "UI가 아닌 배치 도구 actor"),
        (r"^SensorToolToolbarWidget", "UI/Main/Sensor/", "센서 도구 기능의 메인 고정 패널(사이드바·툴바·상태바)"),
        (r"^ShipHatch", "UI/Main/Ship/", "ShipHatchTestMap 메인 고정 패널"),
        (r"^CoilLoading", "UI/Main/Coil/", "CoilLoadingTestMap 메인 고정 패널"),
        (r"^Slab(InformationCard|ScenarioChartWidget|ScenarioReplayUiHostActor|SimulationUiHostActor)", "UI/Main/Slab/", "Slab 검증맵 메인 고정 패널"),
        (r"^(VirtualSensorPanelWidgetBase|VirtualSensorPanelHostComponent|VirtualSensor(Monitor|Settings|Placement|CaptureExport)Panel|Slab(Charts|Progress|ScenarioReplay)PanelWidget|AgentTestSetWidget)", "UI/Freeview/", "떠 있는(이동 가능) 패널: UVirtualSensorPanelWidgetBase 계열"),
        (r"^(SensorMapCheckWidget|TrainBatchWidget)", "UI/Topview/", "닫을 수 있는 고정 패널(맵 점검·학습)"),
        (r"^(SensorToolAppearance|SensorToolUiText|SensorPlacementInteraction|VirtualSensorControlTypes|VirtualSensorCoordinatorMonitorBinding|VirtualSensorPlayerInputBinding|VirtualSensorToolWorkspaceSubsystem|VirtualSensorUiHostActor|VirtualSensorUiPreferences)", "UI/Common/", "여러 위젯이 공유하는 상태·문구·입력·호스트"),
    ],
    "Scenario": [
        (r"^Tests/", "(테스트 대상 폴더의 Tests/)", "대상 파일과 같은 분류로 분산"),
        (r"^Training/", "Training/", "대량 학습"),
        (r"^Automation/", "Manager/Automation/", "자동화 제어 서버·프로토콜"),
        (r"^ShipHatch", "Ship/", "해치 모델·시뮬레이션"),
        (r"^CoilLoading", "Coil/", "코일 적재 모델·시뮬레이션"),
        (r"^(Slab|VirtualSlab|VirtualSensorSlab)", "Slab/Scenario/", "기존 Slab 폴더에 합침"),
        (r"^VirtualDataset", "Dataset/", "가상 데이터셋"),
        (r"^AgentTestSet", "Agent/", "AI Agent 테스트 세트"),
        (r"^Project(Device|RunDelivery)", "Common/", "플러그인↔과제 연결 정책"),
    ],
}

rows = []
for top, rules in RULES.items():
    base = os.path.join(root, top)
    for dirpath, _, files in os.walk(base):
        for f in sorted(files):
            rel = os.path.relpath(os.path.join(dirpath, f), base).replace("\\", "/")
            for pat, dest, why in rules:
                if re.search(pat, rel):
                    rows.append((f"{top}/{rel}", dest, why)); break
            else:
                rows.append((f"{top}/{rel}", "확인 필요", ""))
print("| 현재 | 제안 위치 | 근거 |\n|---|---|---|")
for r in rows:
    print(f"| `{r[0]}` | `{r[1]}` | {r[2]} |")
from collections import Counter
print(f"\n합계 {len(rows)}개 파일:", ", ".join(f"{k} {v}" for k, v in Counter(r[1] for r in rows).most_common()))

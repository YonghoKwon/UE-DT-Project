"""Validate native hatch captures and assemble four topic sheets, without scene edits."""
import argparse
import hashlib
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

TITLES = ["부두·선박 전체 조감", "선박 + GTSU 측면", "ref4 참고 구도", "GTSU 포털·붐·기계실",
          "해측 열린 커버·개구부", "육측 열린 커버·개구부", "해측 레그 안 수신 호퍼", "클램셸 열림",
          "클램셸 닫힘", "철광석 표면 확대", "석탄 표면 확대", "GPU 점군 · 3센서 색 구분",
          "관측 커버리지·높이 히트맵", "홀드 단면", "2D 상면", "설정 패널 + 3분할",
          "Mid-360 · 붐 시점", "Avia · 붐 시점", "OS1-64 · 붐 시점", "XT32 · 붐 시점",
          "VLP-16 · 붐 시점", "실제 디깅 전", "실제 디깅 후", "퍼낸 바닥 노출 확대",
          "바닥 노출 후 GPU 점군", "만재 코너 확대", "전송 완료 표시 · 시연 UI"]
GROUPS = {"context": ([0, 1, 2, 3, 4, 5], 3),
          "close": ([6, 7, 8, 9, 10, 21, 22, 23, 25], 3),
          "analysis": ([11, 12, 13, 14, 15, 24, 26], 2),
          "lidar": ([16, 17, 18, 19, 20], 2)}


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    captures = {}
    for path in sorted(root.glob("[0-2][0-9]-*.png")):
        index = int(path.name[:2])
        if index in captures:
            raise ValueError(f"Duplicate selected capture index: {index}")
        meta = read(path.with_suffix(".json"))
        with Image.open(path) as picture:
            if picture.size != (1920, 1080):
                raise ValueError(f"Expected native1920x1080: {path}")
        if meta["rhi"] != "D3D12" or not meta["render_offscreen"]:
            raise ValueError(f"Expected actual offscreen D3D12: {path}")
        if index < 26 and (len(meta["sensors"]) != 3 or any(s["backend"] != "hardware_ray_tracing" for s in meta["sensors"])):
            raise ValueError(f"Three actual HWRT acquisitions required: {path}")
        if index == 26 and (meta["actual_network_send"] or meta["transport_events_delta"] != 0):
            raise ValueError("Send-completion capture must retain presentation-demo scope")
        captures[index] = {"path": path, "metadata": meta, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    if set(captures) != set(range(27)):
        raise ValueError(f"Missing selected captures: {set(range(27)) - set(captures)}")
    records = []
    for topic, (indices, columns) in GROUPS.items():
        width = 1920 // columns
        height = width * 9 // 16
        tile = height + 68
        sheet = Image.new("RGB", (1920, math.ceil(len(indices) / columns) * tile), (20, 25, 30))
        draw = ImageDraw.Draw(sheet)
        font = ImageFont.truetype("C:/Windows/Fonts/malgun.ttf", 22)
        small = ImageFont.truetype("C:/Windows/Fonts/malgun.ttf", 17)
        rows = []
        for order, index in enumerate(indices):
            capture = captures[index]
            x, y = (order % columns) * width, (order // columns) * tile
            with Image.open(capture["path"]) as picture:
                sheet.paste(picture.convert("RGB").resize((width, height), Image.Resampling.LANCZOS), (x, y))
            draw.text((x + 10, y + height + 5), f"{index:02d} {TITLES[index]}", fill="white", font=font)
            meta = capture["metadata"]
            condition = ("별도 테스트셋 화면 · 시연용 · 실제 네트워크 송신 없음" if index == 26 else
                         f"D3D12 / RT on · GPU 3센서 · 적재 {meta['fill_percent']:.2f}%")
            if 16 <= index <= 20:
                condition = f"붐 점군만 표시 · {meta['sensors'][0]['rays']}광선 원본 · 표시 6px"
            elif index in (22, 23, 24):
                condition += f" · 실제 디깅 {meta['production_digs']}회"
            draw.text((x + 10, y + height + 37), condition, fill=(180, 195, 205), font=small)
            rows.append({"index": index, "title": TITLES[index], "condition": condition,
                         "file": capture["path"].name, "sha256": capture["sha256"]})
        output = root / f"showcase-sheet-{topic}.png"
        sheet.save(output)
        record = {"topic": topic, "file": output.name, "rows": rows,
                  "scope": "unaltered native RHI pixels; resize and bottom labels on sheet only"}
        output.with_suffix(".json").write_text(json.dumps(record, indent=2, ensure_ascii=False), encoding="utf-8")
        records.append(record)
        print(output)
    (root / "manifest.json").write_text(json.dumps({"selected_images": 27, "sheets": records,
        "manual_windows_input": False, "rhi": "D3D12", "native_pixels": [1920, 1080],
        "sender_scope": "presentation demonstration; zero network transmission"}, indent=2, ensure_ascii=False), encoding="utf-8")


if __name__ == "__main__":
    main()

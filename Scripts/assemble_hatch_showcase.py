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
          "바닥 노출 후 GPU 점군", "만재 코너 확대", "전송 완료 표시 · 시연 UI", "퍼낸 탱크톱 · 단면 확인"]
GROUPS = {"context": ([0, 1, 2, 3, 4, 5], 3),
          "close": ([6, 7, 8, 9, 10, 21, 22, 23, 25], 3),
          "analysis": ([11, 12, 13, 14, 15, 24, 27, 26], 2),
          "lidar": ([16, 17, 18, 19, 20], 2)}


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def validate_metrics(cargo, floor):
    assert cargo["backend"] == floor["backend"] == "hardware_ray_tracing"
    assert cargo["model"] == floor["model"]
    for key in ("max_range_m", "horizontal_fov_deg", "vertical_min_deg", "vertical_max_deg"):
        assert abs(cargo[key]-floor[key]) < 1e-6, key
    for key in ("acquisition_origin_hold_m", "acquisition_pitch_yaw_roll"):
        assert all(abs(a-b) < 1e-6 for a, b in zip(cargo[key], floor[key])), key
    assert cargo["rays"] == floor["rays"] == cargo["frame_point_count"] == floor["frame_point_count"]
    assert abs(cargo["fill_percent"]-65) < 1e-4 and floor["fill_percent"] <= 5
    for record in (cargo, floor):
        for key in ("rays", "hit_points", "measured_hz", "max_range_m", "horizontal_fov_deg", "vertical_min_deg", "vertical_max_deg"):
            assert math.isfinite(record[key]), key
        assert record["hit_points"] >= record["cargo_hit_points"]+record["floor_hit_points"]
        for kind in ("cargo", "floor"):
            exact = 100*record[kind+"_observed_cells"]/record["total_hold_cells"]
            assert abs(exact-record[kind+"_observed_percent"]) < 1e-7


def make_lidar_table(root, captures):
    columns = [230, 145, 160, 205, 205, 230, 165, 520]
    width = sum(columns)+40
    table = Image.new("RGB", (width, 650), (20, 25, 30))
    draw = ImageDraw.Draw(table)
    title = ImageFont.truetype("C:/Windows/Fonts/malgun.ttf", 30)
    font = ImageFont.truetype("C:/Windows/Fonts/malgun.ttf", 21)
    draw.text((20, 18), "붐 LiDAR 5종 · 실제 HWRT 측정 비교", fill="white", font=title)
    draw.text((20, 65), "원료: 철광석65% / 바닥:181회 디깅 후 잔량4.99% · 각 조건 전체 홀드1m 목표 격자 · 단일 프레임 관측, 외삽 없음", fill=(180,195,205), font=font)
    headers = ["모델", "광선/회", "hit(65%)", "원료 관측%\n적재65%", "바닥 관측%\n잔량4.99%", "실제 Hz\n65% / 4.99%", "최대 설정 m", "설정 FOV H / V(min~max)°"]
    x = 20
    for header, cell in zip(headers, columns):
        draw.multiline_text((x+8, 110), header, fill="white", font=font, spacing=5)
        x += cell
    rows = []
    for row, index in enumerate(range(16, 21)):
        capture = captures[index]
        metric = capture["metadata"]["lidar_metrics"]
        floor_path = root/f"lidar-floor-{index:02d}.json"
        floor = read(floor_path)
        validate_metrics(metric, floor)
        y = 180+row*70
        draw.rectangle((20, y, width-20, y+65), fill=(28,35,42) if row%2==0 else (23,29,35))
        values = [TITLES[index].split(" · ")[0], f"{metric['rays']:,}", f"{metric['hit_points']:,}",
                  f"{metric['cargo_observed_percent']:.2f}", f"{floor['floor_observed_percent']:.2f}",
                  f"{metric['measured_hz']:.2f} / {floor['measured_hz']:.2f}", f"{metric['max_range_m']:g}",
                  f"{metric['horizontal_fov_deg']:g} / {metric['vertical_min_deg']:g}~{metric['vertical_max_deg']:g}"]
        x = 20
        for value, cell in zip(values, columns):
            draw.text((x+8, y+18), value, fill="white", font=font)
            x += cell
        rows.append({"image": capture['path'].name, "image_sha256": capture['sha256'], "cargo_metrics": metric,
                     "floor_metrics": floor, "floor_json": floor_path.name, "floor_json_sha256": hashlib.sha256(floor_path.read_bytes()).hexdigest()})
    draw.text((20, 555), "커버리지 = 해당 semantic 실제 hit가 든 셀 / 전체 홀드 셀. 바닥 hit는 HatchHold, |Z|≤2cm. 0%는 미관측이며 추정값으로 채우지 않음.", fill=(180,195,205), font=font)
    draw.text((20, 595), "각 프로필의 기본 장착 틸트 적용. 거리/FOV는 런타임 설정, XT32 등 각도별 유효 거리와 동일하다고 주장하지 않음.", fill=(180,195,205), font=font)
    table.save(root/"lidar-compare-table.png")
    (root/"lidar-compare-table.json").write_text(json.dumps({"scope": "actual immutable measured frames, separate65percent and post-dig conditions", "rows": rows}, ensure_ascii=False, indent=2), encoding="utf-8")


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
        if index != 26 and (len(meta["sensors"]) != 3 or any(s["backend"] != "hardware_ray_tracing" for s in meta["sensors"])):
            raise ValueError(f"Three actual HWRT acquisitions required: {path}")
        if index == 26 and (meta["actual_network_send"] or meta["transport_events_delta"] != 0):
            raise ValueError("Send-completion capture must retain presentation-demo scope")
        captures[index] = {"path": path, "metadata": meta, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    if set(captures) != set(range(28)):
        raise ValueError(f"Missing selected captures: {set(range(28)) - set(captures)}")
    records = []
    for topic, (indices, columns) in GROUPS.items():
        width = 1920 // columns
        height = width * 9 // 16
        tile = height + (154 if topic == "lidar" else 68)
        sheet = Image.new("RGB", (1920, math.ceil(len(indices) / columns) * tile), (20, 25, 30))
        draw = ImageDraw.Draw(sheet)
        font = ImageFont.truetype("C:/Windows/Fonts/malgun.ttf", 22)
        small = ImageFont.truetype("C:/Windows/Fonts/malgun.ttf", 17)
        rows = []
        for order, index in enumerate(indices):
            capture = captures[index]
            x, y = (order % columns) * width, (order // columns) * tile
            with Image.open(capture["path"]) as picture:
                panel = picture.convert("RGBA")
            if topic == "lidar":
                overlay = Image.new("RGBA", panel.size)
                outline = ImageDraw.Draw(overlay)
                for key, color in [("coaming_outline_screen_px", (220, 230, 235, 120)),
                                   ("hold_floor_outline_screen_px", (235, 180, 110, 90))]:
                    for segment in capture["metadata"][key]:
                        outline.line(tuple(segment), fill=color, width=3)
                panel = Image.alpha_composite(panel, overlay)
            sheet.paste(panel.convert("RGB").resize((width, height), Image.Resampling.LANCZOS), (x, y))
            draw.text((x + 10, y + height + 5), f"{index:02d} {TITLES[index]}", fill="white", font=font)
            meta = capture["metadata"]
            condition = ("별도 테스트셋 화면 · 시연용 · 실제 네트워크 송신 없음" if index == 26 else
                         f"D3D12 / RT on · GPU 3센서 · 적재 {meta['fill_percent']:.2f}%")
            if 16 <= index <= 20:
                metric = meta["lidar_metrics"]
                floor = read(root / f"lidar-floor-{index:02d}.json")
                validate_metrics(metric, floor)
                condition = f"광선 {metric['rays']:,} · hit {metric['hit_points']:,} · {metric['measured_hz']:.2f} Hz · 최대 설정 {metric['max_range_m']:g} m"
                draw.text((x+10, y+height+63), f"원료 관측 {metric['cargo_observed_percent']:.2f}% (65%) / 바닥 {floor['floor_observed_percent']:.2f}% (잔량 {floor['fill_percent']:.2f}%)", fill=(180,195,205), font=small)
                draw.text((x+10, y+height+89), f"FOV H {metric['horizontal_fov_deg']:g}° / V {metric['vertical_min_deg']:g}~{metric['vertical_max_deg']:g}° · 붐 점군 6px", fill=(180,195,205), font=small)
                draw.text((x+10, y+height+115), "참고 윤곽: 코밍(회색)/홀드 바닥(주황) · 전체 홀드 1m 격자", fill=(180,195,205), font=small)
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
    make_lidar_table(root, captures)
    (root / "manifest.json").write_text(json.dumps({"selected_images": 28, "sheets": records,
        "manual_windows_input": False, "rhi": "D3D12", "native_pixels": [1920, 1080],
        "sender_scope": "presentation demonstration; zero network transmission", "lidar_table": "lidar-compare-table.png", "outline_scope": "reference geometry projected from the exact native camera; composite only, not measured returns"}, indent=2, ensure_ascii=False), encoding="utf-8")


if __name__ == "__main__":
    main()

"""Assemble six real HATCH corner captures without generating scene pixels.

Run after run_hatch_validation.ps1 -Mode CornerCaptures -CornerCaptureDirectory <dir>.
Requires Pillow, also used by the existing HATCH evidence assembly pipeline.
The sixth image gets a labeled real GPU-cloud inset of the same unchanged surface.
Original RHI frames remain in session/r4/raw. Re-running uses those originals.
"""
import argparse
import hashlib
import json
import shutil
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

NAMES = ["01-iron-full-seaward-bow", "02-iron-full-seaward-stern",
         "03-iron-full-landward-bow", "04-iron-full-landward-stern",
         "05-coal-full-seaward-bow", "06-iron-low-residual-seaward-bow"]
CAPTIONS = ["01 철광석 만재 · 해측(+Y) / 선수(+X)", "02 철광석 만재 · 해측(+Y) / 선미(-X)",
            "03 철광석 만재 · 육측(-Y) / 선수(+X)", "04 철광석 만재 · 육측(-Y) / 선미(-X)",
            "05 석탄 만재 · 해측(+Y) / 선수(+X)", "06 철광석 퍼낸 뒤 코너 잔류 · 해측 / 선수"]


def load_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main(root):
    raw = root / "session/r4/raw"
    raw.mkdir(parents=True, exist_ok=True)
    records = []
    for index, name in enumerate(NAMES):
        path = root / (name + ".png")
        meta_path = path.with_suffix(".json")
        meta = load_json(meta_path)
        with Image.open(path) as image:
            if image.size != (1920, 1080):
                raise ValueError(f"Incorrect native RHI image size: {name}")
        if meta["rhi"] != "D3D12" or not meta["render_offscreen"]:
            raise ValueError(f"Not an actual offscreen D3D12 image: {name}")
        if not meta["aim_projects_inside_central_frame"]:
            raise ValueError(f"Corner target not centered: {name}")
        expected_side = "landward" if index in (2, 3) else "seaward"
        expected_end = "stern" if index in (1, 3) else "bow"
        if (meta["side"], meta["end"]) != (expected_side, expected_end):
            raise ValueError(f"Direction mismatch: {name}")
        original = raw / path.name
        if not original.exists() or load_json(original.with_suffix(".json"))["utc"] != meta["utc"]:
            shutil.copy2(path, original)
            shutil.copy2(meta_path, original.with_suffix(".json"))
        meta["raw_rhi_image"] = str(original.relative_to(root)).replace("\\", "/")
        meta["raw_rhi_sha256"] = sha(original)
        records.append(meta)

    cloud_path = raw / "06-gpu-point-cloud.png"
    cloud = load_json(cloud_path.with_suffix(".json"))
    residual = records[5]
    if cloud["surface_crc"] != residual["surface_crc"] or cloud["source_commit"] != residual["source_commit"]:
        raise ValueError("3D and GPU view do not describe the same unchanged scene")
    if not cloud["point_cloud_view"] or not all(
        s["backend"] == "hardware_ray_tracing" and s["acquired_after_last_dig"] for s in cloud["sensors"]
    ):
        raise ValueError("Inset requires actual post-excavation HWRT acquisitions")
    if residual["production_dig_count"] <= 0 or residual["center_height_m"] > .1:
        raise ValueError("Residual must follow real production digging to near the floor")

    font_path = "C:/Windows/Fonts/malgun.ttf"
    font = ImageFont.truetype(font_path, 26)
    small = ImageFont.truetype(font_path, 20)
    with Image.open(raw / (NAMES[5] + ".png")) as original:
        combined = original.convert("RGB")
    with Image.open(cloud_path) as image:
        inset = image.convert("RGB").resize((608, 342), Image.Resampling.LANCZOS)
    x, y = 1280, 674
    draw = ImageDraw.Draw(combined)
    draw.rectangle((x - 3, y - 34, x + 611, y + 345), fill=(20, 25, 30), outline=(220, 225, 230), width=2)
    draw.text((x + 10, y - 29), "GPU 전체 점군 · 같은 표면 revision", font=small, fill="white")
    combined.paste(inset, (x, y))
    combined.save(root / (NAMES[5] + ".png"))
    residual["gpu_inset"] = dict(source=str(cloud_path.relative_to(root)).replace("\\", "/"),
        width=608, height=342, position_px=[x, y], surface_crc=cloud["surface_crc"],
        utc=cloud["utc"], sensors=cloud["sensors"],
        timing="Separate rendered frames of the same unchanged geometry; not a claim of identical wall-clock timestamp")

    sheet = Image.new("RGB", (2880, 1280), (20, 25, 30))
    draw = ImageDraw.Draw(sheet)
    for index, (name, meta) in enumerate(zip(NAMES, records)):
        x, y = (index % 3) * 960, (index // 3) * 640
        draw.text((x + 15, y + 12), CAPTIONS[index], font=font, fill="white")
        with Image.open(root / (name + ".png")) as image:
            sheet.paste(image.convert("RGB").resize((944, 531), Image.Resampling.LANCZOS), (x + 8, y + 54))
        detail = (f"만재 설정95% · 실제 D3D12 / 1920×1080" if index < 5 else
                  f"실제 퍼내기 {meta['production_dig_count']}회 · 잔량 {meta['remaining_fill_percent']:.2f}% · GPU 병기")
        draw.text((x + 15, y + 598), detail, font=small, fill=(205, 215, 225))
        meta["png_sha256"] = sha(root / (name + ".png"))
        (root / (name + ".json")).write_text(json.dumps(meta, ensure_ascii=False, indent=2), encoding="utf-8")
    sheet.save(root / "corners-sheet.png")
    (root / "corners-sheet.json").write_text(json.dumps(dict(images=[n + ".png" for n in NAMES],
        source_commit=records[0]["source_commit"], sheet_sha256=sha(root / "corners-sheet.png"),
        composition="Only resize/layout/text and actual GPU screenshot inset; no generated scene pixels",
        model_limitation=records[0]["model_limitation"]), ensure_ascii=False, indent=2), encoding="utf-8")
    print(root / "corners-sheet.png")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1] / "Saved/Reports/GPU-LIDAR-01/captures/corners")
    main(parser.parse_args().root.resolve())

"""Assemble unchanged RHI pixels from six matching before/after cameras."""
import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, ImageStat


NAMES = ["01-iron-full-seaward-bow", "02-iron-full-seaward-stern",
         "03-iron-full-landward-bow", "04-iron-full-landward-stern",
         "05-coal-full-seaward-bow", "06-iron-low-residual-seaward-bow"]
TITLES = ["철광석 만재 · 해측/선수", "철광석 만재 · 해측/선미",
          "철광석 만재 · 육측/선수", "철광석 만재 · 육측/선미",
          "석탄 만재 · 해측/선수", "실제 퍼낸 뒤 바닥·코너 잔류"]


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    sheet = Image.new("RGB", (1920, 6 * 600), (20, 25, 30))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype("C:/Windows/Fonts/malgun.ttf", 23)
    small = ImageFont.truetype("C:/Windows/Fonts/malgun.ttf", 18)
    records = []
    for row, (name, title) in enumerate(zip(NAMES, TITLES)):
        before, after = [read(root / (name + ".json")) for root in (args.before, args.after)]
        for field in ("camera_hold_m", "aim_hold_m", "camera_rotation_pitch_yaw_roll"):
            if any(abs(a - b) > 1e-6 for a, b in zip(before[field], after[field])):
                raise ValueError(f"Different {field}: {name}")
        if abs(before["horizontal_fov_deg"] - after["horizontal_fov_deg"]) > 1e-6:
            raise ValueError(f"Different FOV: {name}")
        record = {"name": name, "camera_matches": True, "images": []}
        for col, (root, meta, label) in enumerate(((args.before, before, "전"), (args.after, after, "후"))):
            raw = root / "raw" / (name + ".png")
            path = raw if raw.exists() else root / (name + ".png")
            if meta["rhi"] != "D3D12" or not meta["render_offscreen"]:
                raise ValueError(f"Actual D3D12 capture required: {path}")
            with Image.open(path) as source:
                if source.size != (1920, 1080):
                    raise ValueError(f"Native1920x1080 required: {path}")
                picture = source.convert("RGB")
                sheet.paste(picture.resize((960, 540), Image.Resampling.LANCZOS), (col * 960, row * 600))
                median = ImageStat.Stat(picture.crop((480, 720, 1440, 1040)).convert("L")).median[0]
            x, y = col * 960 + 12, row * 600 + 544
            draw.text((x, y), f"{label} | {title}", font=font, fill="white")
            detail = f"적재 {meta['remaining_fill_percent']:.2f}% · 동일 카메라 · 실제 D3D12 / RT on"
            if meta["production_dig_count"]:
                detail += f" · 퍼내기 {meta['production_dig_count']}회"
            draw.text((x, y + 29), detail, font=small, fill=(180, 195, 205))
            record["images"].append({"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                                     "crop_display_luma_median": median, "metadata": meta})
        records.append(record)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)
    args.output.with_suffix(".json").write_text(json.dumps({"kind": "unaltered native RHI pixels; resized contact sheet only",
        "records": records}, ensure_ascii=False, indent=2), encoding="utf-8")
    print(args.output)


if __name__ == "__main__":
    main()

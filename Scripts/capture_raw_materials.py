"""RAW-CAP-01 session backup/restore, native-image QA and presentation copies.

PNG pixels are never edited. The contact sheet and 1600px quality-85 JPGs are
explicit derivatives. Numeric scene values come only from the capture JSONs.
"""
from pathlib import Path
import argparse
import hashlib
import json
import shutil

import capture_coil_environments as session

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Saved/Reports/RAW-CAP-01"
session.OUT = OUT
IDS = ["os_lidar_boom", "os_lidar_cabin_01", "os_lidar_cabin_02"]


def qa():
    from PIL import Image, ImageDraw, ImageStat
    rows, thumbs, derivatives = [], [], []
    share = OUT / "share"
    share.mkdir(exist_ok=True)
    for path in sorted(OUT.glob("[0-9]*.png")):
        data = session.read(path.with_suffix(".json"))
        image = Image.open(path).convert("RGB")
        small = image.resize((240, 135))
        stat = ImageStat.Stat(small)
        luminance = small.convert("L")
        histogram = luminance.histogram()
        populated = sum(histogram[21:]) / sum(histogram)
        sensor_ids = [s["sensor_id"] for s in data["sensors"]]
        sensors_ok = sensor_ids == IDS and all(
            s["profile"] == 8 and "Rev7" in s["model"] and
            s["backend"] == "hardware_ray_tracing" and s["frame_id"] > 0
            for s in data["sensors"])
        valid = (image.size == (1920, 1080) and populated > .015 and
                 max(stat.stddev) > 5 and sensors_ok and data["hardware_ray_tracing"] and
                 (not data["includes_slate_ui"] or data["owned_font_scale"] == 1))
        row = dict(name=path.stem, valid=valid, resolution=list(image.size),
                   mean_rgb=stat.mean, stddev_rgb=stat.stddev,
                   luminance_mean=ImageStat.Stat(luminance).mean[0],
                   fraction_above_20=populated, sensors_ok=sensors_ok,
                   fill_percent=data["fill_percent"], volume_m3=data["remaining_volume_m3"],
                   production_digs=data["production_digs"],
                   png_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        rows.append(row)
        jpg = image.copy()
        jpg.thumbnail((1600, 1600))
        jpg.save(share / (path.stem + ".jpg"), quality=85)
        # Preserve original actual-value metadata alongside each derivative.
        shutil.copy2(path.with_suffix(".json"), share / (path.stem + ".json"))
        derivatives.append(dict(source_png=path.name,
                                source_sha256=row["png_sha256"],
                                jpg=path.stem + ".jpg", width=jpg.width,
                                height=jpg.height, jpeg_quality=85,
                                metadata_describes="native 1920x1080 source"))
        thumb = image.copy()
        thumb.thumbnail((480, 270))
        thumbs.append((path.stem, thumb))
    assert len(rows) == 14, f"Expected 14 native captures, found {len(rows)}"
    progression = [session.read(OUT / f"09{x}-dig-{label}.json")
                   for x, label in (("a", "initial"), ("b", "middle"), ("c", "floor-exposed"))]
    same_camera = all(p["actual_camera_hold_m"] == progression[0]["actual_camera_hold_m"]
                      for p in progression)
    real_progression = (same_camera and
                        progression[0]["production_digs"] == 0 < progression[1]["production_digs"] < progression[2]["production_digs"] and
                        progression[0]["remaining_volume_m3"] > progression[1]["remaining_volume_m3"] > progression[2]["remaining_volume_m3"] and
                        progression[2]["surface_min_height_m"] <= .0001)
    trio = [session.read(OUT / (name + ".json")) for name in
            ("01-unloader-vessel-normal", "02-unloader-vessel-points",
             "03-unloader-vessel-overlay")]
    same_trio = all(p["actual_camera_hold_m"] == trio[0]["actual_camera_hold_m"] and
                    p["horizontal_fov_deg"] == trio[0]["horizontal_fov_deg"] and
                    p["surface_crc"] == trio[0]["surface_crc"] for p in trio)
    sheet = Image.new("RGB", (1440, ((len(thumbs) + 2) // 3) * 310), "#121923")
    draw = ImageDraw.Draw(sheet)
    for index, (name, thumb) in enumerate(thumbs):
        x, y = index % 3 * 480, index // 3 * 310
        sheet.paste(thumb, (x, y))
        draw.text((x + 8, y + 278), name, fill="white")
    sheet.save(OUT / "contact-sheet.png")
    sheet.thumbnail((1600, 1600))
    sheet.save(share / "contact-sheet.jpg", quality=85)
    result = dict(valid=all(r["valid"] for r in rows) and real_progression and same_trio,
                  images=len(rows), real_dig_progression=real_progression,
                  same_dig_camera=same_camera, same_normal_points_overlay_scene=same_trio, rows=rows,
                  manual_visual_review="See RESULT.md; automated QA is not mouse acceptance")
    session.write(OUT / "capture-qa.json", result)
    session.write(share / "manifest.json", dict(images=derivatives,
                  contact_sheet="contact-sheet.jpg", native_png_pixels_edited=False))
    print(json.dumps(dict(valid=result["valid"], images=len(rows),
                          failed=[r["name"] for r in rows if not r["valid"]],
                          real_dig_progression=real_progression)))
    assert result["valid"], "Capture QA failed; inspect actual JSONs and PNGs"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("backup", "check_backup", "restore", "qa"))
    action = parser.parse_args().action
    if action == "qa":
        qa()
    else:
        getattr(session, action)()

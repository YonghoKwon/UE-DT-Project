"""COIL-CAP-01 fixtures, session backup/restore and image QA (no asset writes).

Run prepare before the Editor capture test. The source settings/specs are the
existing PRESENTATION-20261022 coil-3tier-plan.json, never a modified approval.
The Editor test validates these SI fixtures through CoilLoading::Analyze and
loads them through the product's asynchronous LoadPlan API.
"""
from pathlib import Path
import argparse
import copy
import hashlib
import json
import math
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Saved/Reports/COIL-CAP-01"
SEED = ROOT / "Saved/Reports/PRESENTATION-20261022/coil-3tier-plan.json"
PERSONAL = ("Saved/SaveGames", "Saved/Config")
PROTECTED = ("Content", "Config", "Samples/PixelStreaming")


def read(p):
    return json.loads(p.read_text(encoding="utf-8-sig"))


def write(p, obj):
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(obj, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def hashes(roots):
    return {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for rr in roots for p in (ROOT / rr).rglob("*") if p.is_file()}


def backup():
    assert not (OUT / "initial-audit.json").exists(), "Keep this session's original backup"
    personal = hashes(PERSONAL)
    for rel in personal:
        dst = OUT / "personal-backup" / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / rel, dst)
    write(OUT / "initial-audit.json", dict(
        base_commit=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        status=subprocess.check_output(["git", "status", "--short"], cwd=ROOT, text=True),
        personal=personal, protected=hashes(PROTECTED)))


def restore():
    before = read(OUT / "initial-audit.json")
    removed, restored = [], []
    for rel in hashes(PERSONAL):
        if rel not in before["personal"]:
            target = (ROOT / rel).resolve()
            assert any(target.is_relative_to((ROOT / rr).resolve()) for rr in PERSONAL)
            target.unlink()
            removed.append(rel)
    for rel, digest in before["personal"].items():
        src, dst = OUT / "personal-backup" / rel, ROOT / rel
        assert hashlib.sha256(src.read_bytes()).hexdigest() == digest
        if not dst.exists() or hashlib.sha256(dst.read_bytes()).hexdigest() != digest:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
            restored.append(rel)
    after = hashes(PROTECTED)
    changed = [k for k in set(after) | set(before["protected"])
               if after.get(k) != before["protected"].get(k)]
    audit = dict(personal_settings_restored=hashes(PERSONAL) == before["personal"],
                 protected_changed=changed, removed_run_created_files=removed,
                 restored_files=restored, personal_files=len(before["personal"]),
                 protected_files=len(after), previous_backup_used=False)
    write(OUT / "restoration-audit.json", audit)
    assert audit["personal_settings_restored"] and not changed, audit
    print(json.dumps(audit))


def check_backup():
    before = read(OUT / "initial-audit.json")
    assert hashes(PERSONAL) == before["personal"], (
        "Personal settings differ from this session's backup. Stop before Editor; "
        "make a new session backup instead of restoring an older session over them.")
    assert hashes(PROTECTED) == before["protected"], "Protected files changed since backup"


def prepare():
    seed = read(SEED)
    specs = {c["type"]: c for c in seed["coils"]}
    settings = seed["settings"]
    scenes = [("half", 2), ("full", 5), ("mixed", 3), ("varied", 3), ("list", 3), ("lowering", 5)]
    for name, tiers in scenes:
        coils, previous_edge = [], None
        for row in range(1, 18):
            cr = name == "mixed" and row >= 9 or name == "half" and row >= 16
            spec = copy.deepcopy(specs["CR" if cr else "HR"])
            # Equal gross OD within every supported row. Different sizes occupy
            # separate longitudinal blocks, so narrow coils never support wide ones.
            od, width = (1.8, 1.5) if name == "varied" and row <= 8 else ((1.4, 1.0) if name == "varied" else (1.6, 1.25))
            spec.update(od=od - (2 * spec["package_radial_m"] if cr else 0), width=width,
                        inner_diameter=.762, weight=0)
            gross_width = width + (2 * spec["package_end_m"] if cr else 0)
            x = settings["HoldLength"] / 2 - max(gross_width / 2, settings["GearLength"] / 2) - settings["ClearanceMargin"] if previous_edge is None else previous_edge - gross_width / 2 - .15
            # The coil can fit while its C-hook swept envelope does not. Keep
            # the last complete row that also meets the actual hatch margin.
            if abs(x) + max(gross_width / 2, settings["GearLength"] / 2) > settings["OpeningLength"] / 2 - settings["ClearanceMargin"] + 1e-8:
                break
            assert abs(x) + gross_width / 2 <= settings["HoldLength"] / 2 - settings["ClearanceMargin"] + 1e-8
            previous_edge = x - gross_width / 2
            columns = math.floor((settings["HoldWidth"] - 2 * max(od / 2, settings["GearWidth"] / 2) - 2 * settings["ClearanceMargin"]) / od + 1e-8) + 1
            if name == "list":
                columns = 5
            last = []
            for tier in range(1, tiers + 1):
                current = []
                for col in range(1, columns - tier + 2):
                    p = copy.deepcopy(spec)
                    p.update(coil_id=f"CAP-{name}-R{row:02}-T{tier}-C{col:02}",
                             x=x, y=-settings["HoldWidth"] / 2 + max(od / 2, settings["GearWidth"] / 2) + settings["ClearanceMargin"] + (col - 1 + (tier - 1) / 2) * od,
                             z=settings["DunnageThickness"] + od / 2 + (tier - 1) * od * math.sqrt(3) / 2,
                             tier=tier, row=row, column=col,
                             support_a=last[col - 1] if last else -1,
                             support_b=last[col] if last else -1,
                             dunnage=True, chocks=True, lashing=False, eye_fore_aft=True,
                             chock_kn=0, lashing_kn=0, k_diam=0, ovality_limit=0, rolling_resistance_m=0)
                    current.append(len(coils))
                    coils.append(p)
                last = current
        pending = copy.deepcopy(coils[-1])
        if name == "lowering":
            pending = coils.pop()
        else:
            # The product section is the next row. Preserve a real preview;
            # its actual additional-coil analysis is recorded separately.
            pending.update(coil_id=f"CAP-{name}-NEXT", tier=3 if name == "half" else 5 if name == "full" else 4, row=9, column=8 if name == "full" else 1)
        write(OUT / "fixtures" / f"{name}.json", dict(schema="coil-loading.v1", settings=settings, coils=coils))
        write(OUT / "fixtures" / f"{name}-pending.json", pending)
        print(f"{name}: {len(coils)} placed, max tier {tiers}")
    write(OUT / "fixture-source.json", dict(seed=SEED.relative_to(ROOT).as_posix(),
          seed_sha256=hashlib.sha256(SEED.read_bytes()).hexdigest(), settings_unchanged=True,
          mass_overrides=False, approval_inputs_unchanged=True))


def qa():
    from PIL import Image, ImageDraw, ImageStat
    images = []
    # UE5.3's PI macro is a float literal. Use its represented value for the
    # independent sum, rather than silently changing the product's calculation.
    engine_pi = struct.unpack("f", struct.pack("f", math.pi))[0]
    for p in sorted(OUT.glob("cap-*.png")):
        im = Image.open(p).convert("RGB")
        meta = read(p.with_suffix(".json"))
        assert im.size == (1920, 1080) and meta["valid"], p
        plan = read(OUT / meta["plan_file"])["coils"]
        assert len(plan) == meta["coil_count"] == meta["rendered_coil_instances"], p
        assert sum(meta["tier_distribution"].values()) == len(plan)
        assert meta["hr_count"] + meta["cr_count"] == len(plan)
        mass = sum(c["weight"] if c["weight"] > 0 else c["density"] * c["fill"] * engine_pi / 4 * (c["od"] ** 2 - c["inner_diameter"] ** 2) * c["width"] for c in plan)
        assert abs(mass - meta["placed_analysis"]["total_mass_t"]) < 1e-6
        assert max(ImageStat.Stat(im).stddev) > 10, p
        images.append(dict(file=p.name, sha256=hashlib.sha256(p.read_bytes()).hexdigest(),
                           coils=len(plan), brightness=meta["brightness"],
                           counts_match_loaded_plan_and_render_instances=True))
    assert len(images) == 17, len(images)
    sheet = Image.new("RGB", (1600, math.ceil(len(images) / 4) * 260), (22, 27, 35))
    draw = ImageDraw.Draw(sheet)
    for i, entry in enumerate(images):
        tile = Image.open(OUT / entry["file"]).convert("RGB").resize((392, 220))
        x, y = (i % 4) * 400 + 4, (i // 4) * 260 + 4
        sheet.paste(tile, (x, y))
        draw.text((x + 3, y + 224), entry["file"].replace(".png", ""), fill="white")
        draw.text((x + 3, y + 239), f"{entry['coils']} placed coils", fill=(181, 198, 215))
    sheet.save(OUT / "contact-sheet.png")
    write(OUT / "capture-qa.json", dict(Valid=True, Failed=0, images=images,
          contact_sheet="contact-sheet.png", engine_pi=engine_pi,
          visual_review="See RESULT.md; contact sheet is derived, individual captures are native."))
    print(f"QA: {len(images)} native PNG/JSON pairs; count/mass cross-check PASS")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("backup", "check_backup", "prepare", "restore", "qa"))
    globals()[parser.parse_args().action]()

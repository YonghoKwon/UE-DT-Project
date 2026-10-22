"""Assemble unaltered RHI scene crops and supplied GTSU references."""
import argparse
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageOps


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--after', type=Path, required=True)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    reports = args.root / 'Saved/Reports/FOLLOWUP-26-28'
    before = reports / 'progress/29b/balanced/quality/captures'
    reference = args.root / 'Saved/Reports/HATCH-06/reference'
    pairs = [
        [(before/'ref1-v2.png', '#29b 측면'), (args.after/'32-0.png', '#32 측면 · 같은 고정 카메라'),
         (reference/'ref5-gtsu-portal-grab-hull.png', 'ref5 · 포털 / 그랩 / 선체')],
        [(before/'ref4-v2.png', '#29b 조감'), (args.after/'32-1.png', '#32 조감 · 같은 각도 / 자동 프레이밍'),
         (reference/'ref4-gtsu-ship-3dscan.png', 'ref4 · 상대 배치 최우선 참고')],
    ]
    sheet = Image.new('RGB', (2880, 1120), (20, 25, 30))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.truetype('C:/Windows/Fonts/malgun.ttf', 24)
    entries = []
    for row, frames in enumerate(pairs):
        for column, (path, title) in enumerate(frames):
            with Image.open(path) as raw:
                frame = raw.convert('RGB')
                crop = None
                if column < 2:
                    meta = json.loads(path.with_suffix('.json').read_text(encoding='utf-8-sig'))
                    assert raw.size == (1920, 1080), path
                    crop = tuple(round(meta[k]) for k in ('scene_left_px', 'scene_top_px', 'scene_right_px', 'scene_bottom_px'))
                    frame = frame.crop(crop)
                frame = ImageOps.contain(frame, (952, 500), Image.Resampling.LANCZOS)
                x, y = column*960+(960-frame.width)//2, row*560+(500-frame.height)//2
                sheet.paste(frame, (x, y))
                draw.text((column*960+12, row*560+510), title, font=font, fill='white')
                entries.append({'file': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'scene_crop_px': crop})
    output = reports/'corners-32-gtsu-check.png'
    sheet.save(output)
    output.with_suffix('.json').write_text(json.dumps({
        'scope': 'Native RHI pixels and supplied references; scene crop, resize and bottom captions only.',
        'camera_note': 'Side fixes the pre32 quay-relative camera in world space. Overview keeps preset3 angle and automatically fits changed geometry; distance can differ.',
        'entries': entries,
    }, ensure_ascii=False, indent=2), encoding='utf-8')
    print(output)


if __name__ == '__main__':
    main()

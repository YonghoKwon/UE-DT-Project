"""Check real cooker output for project assets loaded by literal runtime paths.

Run after BuildCookRun, for example:
  python Scripts/validate_runtime_cook_assets.py Saved/Cooked/Windows/ma0t10_dt/Content
This does not load or save Unreal assets. Tests/ references are excluded.
"""
import argparse
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cooked_content', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    if not args.cooked_content.is_dir():
        parser.error('Cooked project Content directory does not exist')
    references = {}
    pattern = re.compile(r'LoadObject<[^>]+>\([^;]*?TEXT\("(/Game/[^"\s]+)"\)', re.DOTALL)
    for source in (root / 'Source/ma0t10_dt').rglob('*.cpp'):
        if 'Tests' in source.parts:
            continue
        for object_path in pattern.findall(source.read_text(encoding='utf-8-sig')):
            package = object_path.split('.', 1)[0]
            references.setdefault(package, []).append(str(source.relative_to(root)))
    if not references:
        parser.error('No runtime project asset references found; scan did not execute')
    missing = []
    for package in sorted(references):
        relative = Path(package.removeprefix('/Game/')).with_suffix('.uasset')
        if not (root / 'Content' / relative).is_file():
            missing.append((package, 'source asset missing'))
        elif not (args.cooked_content / relative).is_file():
            missing.append((package, 'cooked asset missing'))
    for package, reason in missing:
        print(f'FAIL {package}: {reason}; runtime users: {", ".join(references[package])}')
    print(f'Runtime literal asset cook check: {len(references) - len(missing)} present, {len(missing)} missing')
    return 1 if missing else 0


if __name__ == '__main__':
    raise SystemExit(main())

"""Own exactly one disposable WSL2 distribution; never operate on other distros."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import urllib.request

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
LOCK = json.loads((HERE / 'environment-lock.json').read_text())
DISTRO = 'ma0t10-ros2'


def wsl(*args, check=True, **kwargs):
    p = subprocess.run(['wsl.exe', *args], capture_output=True, **kwargs)
    def decode(data):
        return data.decode('utf-16le' if b'\0' in data[:100] else 'utf-8', errors='replace').strip()
    out = decode(p.stdout)
    if check and p.returncode:
        # Commands contain no credentials. Do not propagate arbitrary Linux stderr.
        raise RuntimeError(f'WSL command failed ({p.returncode}): {out[:1200]}')
    return out


def names():
    return wsl('--list', '--quiet').splitlines()


def linux_path(path):
    return wsl('-d', DISTRO, '-u', 'root', '--', 'wslpath', '-a', str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['install', 'up', 'down', 'status', 'remove', 'run', 'stop'])
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--with-image-decoder', action='store_true')
    args = parser.parse_args()
    report = args.report.resolve()
    if not report.is_relative_to(ROOT / 'Saved' / 'Reports'):
        raise ValueError('Report/install directory must stay in this checkout Saved/Reports')
    report.mkdir(parents=True, exist_ok=True)
    statefile = report / 'ownership.json'
    state = json.loads(statefile.read_text()) if statefile.exists() else None
    current = names()
    action = {'run': 'up', 'stop': 'down'}.get(args.action, args.action)
    if action == 'status':
        print(wsl('-l', '-v'))
        print('owned=' + str(bool(state and DISTRO in current)))
        if state and DISTRO in current:
            print(wsl('-d', DISTRO, '-u', 'root', '--', 'bash', linux_path(HERE / 'run.sh'), 'status', linux_path(report)))
        return
    if action == 'install':
        if DISTRO in current or state:
            raise RuntimeError('Refusing to overwrite an existing distro or ownership record. Use its original report directory.')
        state = {'distribution': DISTRO, 'before': wsl('-l', '-v'), 'before_names': current,
                 'root': str(ROOT), 'download_bytes': 0, 'imported': False}
        statefile.write_text(json.dumps(state, indent=2))
        archive = report / 'ubuntu-24.04.wsl'
        try:
            print('Downloading official Ubuntu 24.04 rootfs (SHA256 pinned).', flush=True)
            digest = hashlib.sha256()
            with urllib.request.urlopen(LOCK['rootfs_url'], timeout=60) as response, archive.open('wb') as dest:
                while chunk := response.read(1024 * 1024):
                    dest.write(chunk)
                    digest.update(chunk)
                    state['download_bytes'] += len(chunk)
            state['rootfs_sha256'] = digest.hexdigest()
            statefile.write_text(json.dumps(state, indent=2))
            if digest.hexdigest() != LOCK['rootfs_sha256']:
                raise RuntimeError('Rootfs checksum mismatch')
            print('Rootfs bytes: ' + str(state['download_bytes']), flush=True)
            # Import bypasses Ubuntu OOBE, uses root, and specifies WSL2 without changing defaults.
            print(wsl('--import', DISTRO, str(report / 'distribution'), str(archive), '--version', '2'), flush=True)
            state['imported'] = True
            statefile.write_text(json.dumps(state, indent=2))
        finally:
            if archive.exists():
                state['download_bytes'] = archive.stat().st_size
                archive.unlink()  # exact owned installer only
            statefile.write_text(json.dumps(state, indent=2))
        script = linux_path(HERE / 'install.sh')
        print(wsl('-d', DISTRO, '-u', 'root', '--', 'bash', script, linux_path(report),
                  'yes' if args.with_image_decoder else 'no'), flush=True)
        return
    if not state or state['root'] != str(ROOT) or state['distribution'] != DISTRO:
        raise RuntimeError('No matching ownership record; refusing action')
    if action == 'remove':
        if DISTRO in current:
            if not state['imported']:
                raise RuntimeError('Distro ownership is unproven; refusing unregister')
            print(wsl('-d', DISTRO, '-u', 'root', '--', 'bash', linux_path(HERE / 'run.sh'), 'down', linux_path(report)))
            print(wsl('--unregister', DISTRO))
        after = wsl('-l', '-v')
        (report / 'wsl-after.txt').write_text(after, encoding='utf8')
        if names() != state['before_names']:
            raise RuntimeError('Distribution list differs from baseline; no other distro will be modified')
        remaining = [str(p.relative_to(report)) for pattern in ('*.wsl', '*.deb', '*.tar*', '*.whl', '*.vhdx') for p in report.rglob(pattern)]
        state['removed'] = True
        state['remaining_installers'] = remaining
        statefile.write_text(json.dumps(state, indent=2))
        print(after)
        print('remaining owned installers=' + str(remaining))
        return
    if DISTRO not in current or not state['imported']:
        raise RuntimeError('Owned distribution is not installed')
    # WSLENV is process-local; values are inherited without command line, files, or logs.
    env = os.environ.copy()
    passthrough = ['MA0T10_STOMP_USER', 'MA0T10_STOMP_PASSWORD', 'MA0T10_STOMP_HOST', 'MA0T10_STOMP_PORT']
    env['WSLENV'] = ':'.join(filter(None, [env.get('WSLENV', ''), *passthrough]))
    print(wsl('-d', DISTRO, '-u', 'root', '--', 'bash', linux_path(HERE / 'run.sh'), action, linux_path(report), env=env))


if __name__ == '__main__':
    try:
        main()
    except Exception as exc:
        print(f'ROS2-01 environment blocked: {exc}', file=sys.stderr)
        sys.exit(1)

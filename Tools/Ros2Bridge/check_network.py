"""Read-only connectivity probe. No authentication, payloads, or system changes."""
import socket
import subprocess
import json
host = subprocess.check_output(['ip','-4','route','show','default'], text=True).split()[2]
try:
    with socket.create_connection((host, 61616), timeout=5):
        print(json.dumps({'host':host, 'port':61616, 'tcp':'reachable'}))
except OSError as exc:
    print(json.dumps({'host':host, 'port':61616, 'tcp':'blocked', 'error_class':type(exc).__name__, 'errno':exc.errno}))
    raise SystemExit(1)

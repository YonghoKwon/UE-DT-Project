"""Optional temporary WSL-interface-only relay to a localhost-only Artemis.

Not needed for the current 0.0.0.0 Artemis acceptor. No firewall or portproxy edits.
Ctrl+C closes this process and all owned sockets. Never log packet contents.
"""
import argparse
import ipaddress
import select
import socket
import threading


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bind', required=True, help='Windows vEthernet (WSL) IPv4 address, never 0.0.0.0')
    p.add_argument('--wsl-subnet', required=True)
    p.add_argument('--port', type=int, default=61617)
    args = p.parse_args()
    ip = ipaddress.IPv4Address(args.bind)
    subnet = ipaddress.IPv4Network(args.wsl_subnet, strict=False)
    if not ip.is_private or ip.is_loopback or ip not in subnet or not 1024 <= args.port <= 65535:
        p.error('Use the actual private WSL vEthernet interface/subnet and a user port')
    stopping = threading.Event()
    slots = threading.BoundedSemaphore(8)
    threads = []

    def forward(client):
        try:
            with client, socket.create_connection(('127.0.0.1',61616),timeout=5) as upstream:
                client.settimeout(5)
                while not stopping.is_set():
                    readable, _, _ = select.select([client,upstream],[],[],.25)
                    for source in readable:
                        block = source.recv(65536)
                        if not block:
                            return
                        (upstream if source is client else client).sendall(block)
        except OSError:
            pass
        finally:
            slots.release()

    with socket.socket() as listener:
        listener.bind((str(ip), args.port))
        listener.listen(8)
        listener.settimeout(.5)
        print('Temporary WSL TCP relay running; Ctrl+C to close. No bodies logged.',flush=True)
        try:
            while True:
                try:
                    client, address = listener.accept()
                except socket.timeout:
                    continue
                if ipaddress.IPv4Address(address[0]) not in subnet or not slots.acquire(blocking=False):
                    client.close()
                    continue
                threads = [t for t in threads if t.is_alive()]
                t = threading.Thread(target=forward,args=(client,),daemon=True)
                threads.append(t)
                t.start()
        except KeyboardInterrupt:
            stopping.set()
    for t in threads:
        t.join(timeout=6)


if __name__ == '__main__':
    main()

"""Generate isolated Artemis configs without printing/persisting secrets in Git.

Does not invoke Docker, modify a service, or contact a server. The official image
copies etc-override/broker.xml during first instance creation (Artemis 2.42.0).
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import secrets
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
NODES = ("live-a", "live-b", "backup-a", "backup-b")
NS = "urn:activemq:core"


def read_env(path: Path) -> dict[str, str]:
    return dict(line.split("=", 1) for line in path.read_text(encoding="utf-8").splitlines()
                if line and not line.startswith("#") and "=" in line)


def broker_xml(node: str, cluster_password: str, threshold: int, limit: int) -> str:
    if node not in NODES or threshold < 1024 or limit < 1024:
        raise ValueError("invalid local broker parameters")
    # Artemis' bootstrap also looks up the literal core element name. Namespace
    # prefixes are XML-equivalent but must not become ns0:core here.
    root = ET.Element("configuration", {"xmlns": "urn:activemq"})
    core = ET.SubElement(root, "core", {"xmlns": NS})

    def add(parent: ET.Element, tag: str, value=None, **attrs):
        el = ET.SubElement(parent, tag, attrs)
        if value is not None:
            el.text = str(value)
        return el

    add(core, "name", node)
    add(core, "persistence-enabled", "true")
    add(core, "journal-type", "NIO")
    add(core, "bindings-directory", "./data/bindings")
    add(core, "journal-directory", "./data/journal")
    add(core, "large-messages-directory", "./data/large-messages")
    add(core, "paging-directory", "./data/paging")
    add(core, "journal-file-size", "10M")
    add(core, "global-max-size", "256M")
    add(core, "disk-scan-period", 5000)
    add(core, "max-disk-usage", 90)
    add(core, "cluster-user", "pcd_cluster")
    add(core, "cluster-password", cluster_password)
    connectors = add(core, "connectors")
    for peer in NODES:
        add(connectors, "connector", f"tcp://{peer}:61616", name=peer)
    acceptors = add(core, "acceptors")
    add(acceptors, "acceptor",
        f"tcp://0.0.0.0:61616?protocols=CORE,STOMP;stompMinLargeMessageSize={threshold}", name="stomp-core")
    clusters = add(core, "cluster-connections")
    cluster = add(clusters, "cluster-connection", name="sensor-cluster")
    add(cluster, "connector-ref", node)
    add(cluster, "retry-interval", 500)
    add(cluster, "use-duplicate-detection", "true")
    add(cluster, "message-load-balancing", "ON_DEMAND")
    add(cluster, "max-hops", 1)
    static = add(cluster, "static-connectors")
    for peer in NODES:
        if peer != node:
            add(static, "connector-ref", peer)
    ha = add(add(core, "ha-policy"), "replication")
    role = add(ha, "primary" if node.startswith("live-") else "backup")
    add(role, "group-name", "pair-" + node[-1])
    if node.startswith("live-"):
        add(role, "check-for-active-server", "true")
    else:
        add(role, "allow-failback", "false")
    security = add(core, "security-settings")
    setting = add(security, "security-setting", match="#")
    for permission in ("createNonDurableQueue", "deleteNonDurableQueue", "createDurableQueue",
                       "deleteDurableQueue", "createAddress", "deleteAddress", "consume", "browse", "send", "manage"):
        add(setting, "permission", type=permission, roles="amq")
    addresses = add(core, "address-settings")
    pcd = add(addresses, "address-setting", match="topic.virtual.sensor.export.#")
    add(pcd, "max-size-bytes", limit)
    add(pcd, "address-full-policy", "DROP")
    add(pcd, "redistribution-delay", 0)
    add(pcd, "auto-create-addresses", "true")
    add(pcd, "auto-create-queues", "true")
    other = add(addresses, "address-setting", match="#")
    add(other, "max-size-bytes", "64M")
    add(other, "address-full-policy", "BLOCK")
    add(other, "redistribution-delay", 0)
    add(other, "auto-create-addresses", "true")
    add(other, "auto-create-queues", "true")
    ET.indent(root, space="  ")
    return ET.tostring(root, encoding="unicode", xml_declaration=True) + "\n"


def haproxy_config(user: str, password: str) -> str:
    # Passive backup acceptors may answer TCP. Check actual STOMP readiness,
    # rather than accidentally routing normal clients to a passive server.
    connect = (f"CONNECT\naccept-version:1.2\nhost:localhost\nlogin:{user}\n"
               f"passcode:{password}\nheart-beat:0,0\n\n\0").encode().hex()
    return "\n".join([
        "global", "  maxconn 256", "defaults", "  mode tcp",
        "  timeout connect 5s", "  timeout client 120s", "  timeout server 120s",
        "resolvers docker", "  nameserver dns 127.0.0.11:53", "  resolve_retries 3",
        "  timeout resolve 1s", "  timeout retry 1s", "  hold valid 10s",
        "frontend sensor_vip", "  bind *:61616", "  default_backend active_brokers",
        "backend active_brokers", "  balance roundrobin", "  option tcp-check",
        "  tcp-check connect", f"  tcp-check send-binary {connect}",
        "  tcp-check expect string CONNECTED",
        *[f"  server {node} {node}:61616 check inter 2s fall 2 rise 2 resolvers docker init-addr last,libc,none" for node in NODES], ""])


def configure(threshold: int = 16 * 1024 * 1024, limit: int = 64 * 1024 * 1024) -> dict:
    local = ROOT / ".local"
    local.mkdir(exist_ok=True)
    env_path = local / "test.env"
    if env_path.exists():
        env = read_env(env_path)
    else:
        env = {"ARTEMIS_IMAGE": "apache/activemq-artemis:2.42.0-alpine",
               "ARTEMIS_USER": "pcd_test", "ARTEMIS_PASSWORD": secrets.token_hex(24),
               "CLUSTER_PASSWORD": secrets.token_hex(24)}
        env_path.write_text("".join(f"{key}={value}\n" for key, value in env.items()), encoding="utf-8")
    files = []
    for node in NODES:
        target = local / "config" / node / "broker.xml"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(broker_xml(node, env["CLUSTER_PASSWORD"], threshold, limit), encoding="utf-8")
        files.append({"path": str(target.relative_to(ROOT)), "sha256": hashlib.sha256(target.read_bytes()).hexdigest()})
    (local / "haproxy.cfg").write_text(haproxy_config(env["ARTEMIS_USER"], env["ARTEMIS_PASSWORD"]), encoding="utf-8")
    result = {"image": env["ARTEMIS_IMAGE"], "nodes": list(NODES), "replication_pairs": 2,
              "stomp_min_large_message_bytes": threshold, "pcd_max_size_bytes": limit,
              "address_full_policy": "DROP", "config_files": files, "docker_invoked": False}
    (local / "configuration-summary.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--large-message-bytes", type=int, default=16 * 1024 * 1024)
    parser.add_argument("--address-bytes", type=int, default=64 * 1024 * 1024)
    args = parser.parse_args()
    summary = configure(args.large_message_bytes, args.address_bytes)
    print(json.dumps({key: summary[key] for key in ("image", "nodes", "stomp_min_large_message_bytes", "pcd_max_size_bytes", "docker_invoked")}))

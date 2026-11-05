"""STRUCTURE-12 real-channel failover checks. No perf/memory verdict is made."""
from __future__ import annotations

import argparse
import re
from pathlib import Path

from configure import ROOT, configure, haproxy_config
from run_scenarios import Cluster, PROJECT, assert_unreal_slot, dump, native_case


def loss_verdict(result: dict, log: str) -> dict:
    sender = result["sender"]
    losses = sender.get("bulk_loss_by_sensor", {})
    checks = {
        "existing_delivery_checks": bool(result["pass"]),
        # AutomationController reprints captured warnings at test completion.
        # Count the originating logger, not that report echo.
        "one_recovery_warning": len(re.findall(r"\]LogTemp:\s+Warning:\s+\[SensorBulkRecovery\]", log)) == 1,
        "positive_known_loss": sender.get("bulk_outage_loss", 0) > 0,
        "per_sensor_loss": set(losses) == {"DATAIF-1", "DATAIF-2", "DATAIF-3"}
        and all(value > 0 for value in losses.values()),
        "channel_status_counts_loss": sender.get("bulk_channel_dropped", -1)
        >= sender.get("bulk_outage_loss", 0),
        "status_text_available": bool(sender.get("bulk_status_text")),
        "positive_disconnect_duration": sender.get("bulk_outage_seconds", 0) > 0,
    }
    return {"pass": all(checks.values()), "checks": checks,
            "ui_scope": "same status string as existing connection UI; no direct mouse acceptance",
            "loss_scope": "known local rejection/terminal failure, not consumer ACK"}


def shared_verdict(result: dict) -> dict:
    sender = result["sender"]
    connected = {row["name"] for row in sender["channels"] if row["state"] == 2}
    checks = {"existing_delivery_checks": bool(result["pass"]),
              "shared_mode": sender.get("json_mode") == "Default",
              "two_product_connections": connected == {"Default", "sensor-bulk"},
              "sensor_registered_idle": any(row["name"] == "sensor" and row["state"] == 0 for row in sender["channels"]),
              "shared_json_accepted": sender.get("json_accepted", 0) > 0,
              "external_json_received": result["consumer"]["json_messages"] > 0}
    totals = [sum(row["connections"] for row in snap["nodes"].values()
                  if row.get("active") and isinstance(row.get("connections"), (int, float)))
              for snap in result["snapshots"]]
    checks["broker_observed_connections"] = bool(totals) and max(totals) >= 3
    process_counts = [row["broker_established"] for row in result.get("process_socket_samples", [])
                      if isinstance(row.get("broker_established"), int)]
    checks["two_physical_process_connections"] = bool(process_counts) and max(process_counts) == 2
    return {"pass": all(checks.values()), "checks": checks, "connected_product_channels": sorted(connected),
            "broker_connection_samples": totals,
            "process_broker_connection_samples": process_counts,
            "broker_count_scope": "includes cluster/replication links, one external consumer and transient HAProxy health checks"}


def run(report: Path, mode: str = "loss") -> None:
    report = report.resolve()
    report.relative_to((PROJECT / "Saved/Reports").resolve())
    report.mkdir(parents=True, exist_ok=False)
    assert_unreal_slot()
    configure()
    cluster = Cluster(report)
    outcome = {"pass": False}
    try:
        cluster.call(["config", "--quiet"])
        cluster.call(["up", "-d"], timeout=600)
        cluster.wait_ready()
        if mode == "shared":
            result = native_case(cluster, "default-json", seconds=30, shared_json=True)
            outcome = shared_verdict(result)
            dump(report / "shared-verdict.json", outcome)
            if not outcome["pass"]:
                raise RuntimeError("STRUCTURE-12 shared connection check failed; see sanitized report")
            return
        # Both channels use pair A; pair B remains available for cluster quorum.
        config = haproxy_config(cluster.env["ARTEMIS_USER"], cluster.env["ARTEMIS_PASSWORD"])
        config = config.replace("server live-b live-b:61616 check", "server live-b live-b:61616 disabled check")
        config = config.replace("server backup-b backup-b:61616 check", "server backup-b backup-b:61616 disabled check")
        (ROOT / ".local/haproxy.cfg").write_text(config, encoding="utf-8")
        cluster.call(["up", "-d", "--no-deps", "--force-recreate", "vip"])
        result = native_case(cluster, "live-failover", seconds=150, kill_live=True)
        log = (report / "live-failover/automation.log").read_text(encoding="utf-8-sig", errors="replace")
        outcome = loss_verdict(result, log)
        dump(report / "loss-verdict.json", outcome)
    finally:
        cluster.call(["logs", "--no-color", "--tail", "200"], check=False)
        cluster.call(["down", "--volumes", "--remove-orphans"], check=False)
        dump(report / "summary.json", outcome)
    if not outcome["pass"]:
        raise RuntimeError("STRUCTURE-12 failover check failed; see sanitized report")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument("--mode", choices=("loss", "shared"), default="loss")
    args = parser.parse_args()
    run(args.report, args.mode)

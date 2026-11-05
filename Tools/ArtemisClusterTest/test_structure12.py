import unittest
from types import SimpleNamespace

from run_structure12 import loss_verdict, shared_verdict
from run_scenarios import process_broker_connections


class LossVerdictTests(unittest.TestCase):
    def test_process_counter_supports_existing_psutil_without_counting_unrelated_sockets(self):
        rows = [SimpleNamespace(status="ESTABLISHED", raddr=SimpleNamespace(ip="127.0.0.1", port=27616)),
                SimpleNamespace(status="ESTABLISHED", raddr=SimpleNamespace(ip="127.0.0.1", port=27616)),
                SimpleNamespace(status="ESTABLISHED", raddr=SimpleNamespace(ip="127.0.0.1", port=443)),
                SimpleNamespace(status="CLOSE_WAIT", raddr=SimpleNamespace(ip="127.0.0.1", port=27616))]
        for name in ("connections", "net_connections"):
            process = SimpleNamespace(**{name: lambda kind: rows})
            self.assertEqual(process_broker_connections(process, (27616, 27616)), 2)

    def test_warning_echo_does_not_count_as_second_emission(self):
        sender = dict(bulk_outage_loss=3, bulk_channel_dropped=3, bulk_status_text="available",
                      bulk_outage_seconds=1, bulk_loss_by_sensor={f"DATAIF-{i}": 1 for i in range(1, 4)})
        original = "[2026.11.05][0]LogTemp: Warning: [SensorBulkRecovery] recovery\n"
        echo = "[2026.11.05][1]LogAutomationController: Warning: LogTemp: [SensorBulkRecovery] recovery\n"
        self.assertTrue(loss_verdict({"sender": sender, "pass": True}, original + echo)["pass"])
        self.assertFalse(loss_verdict({"sender": sender, "pass": True}, original * 2)["pass"])
        sender["bulk_outage_loss"] = 0
        self.assertFalse(loss_verdict({"sender": sender, "pass": True}, original)["pass"])

    def test_shared_mode_requires_two_real_connected_channels_and_receiver(self):
        result = {"pass": True, "sender": {"json_mode": "Default", "json_accepted": 2,
                  "channels": [{"name": "Default", "state": 2}, {"name": "sensor-bulk", "state": 2}, {"name": "sensor", "state": 0}]},
                  "consumer": {"json_messages": 1}, "snapshots": [{"nodes": {"a": {"active": True, "connections": 3}}}],
                  "process_socket_samples": [{"broker_established": 0}, {"broker_established": 2}]}
        self.assertTrue(shared_verdict(result)["pass"])
        result["sender"]["channels"][2]["state"] = 2
        self.assertFalse(shared_verdict(result)["pass"])


if __name__ == "__main__":
    unittest.main()

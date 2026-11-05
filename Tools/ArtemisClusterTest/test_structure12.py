import unittest

from run_structure12 import loss_verdict


class LossVerdictTests(unittest.TestCase):
    def test_warning_echo_does_not_count_as_second_emission(self):
        sender = dict(bulk_outage_loss=3, bulk_channel_dropped=3, bulk_status_text="available",
                      bulk_outage_seconds=1, bulk_loss_by_sensor={f"DATAIF-{i}": 1 for i in range(1, 4)})
        original = "[2026.11.05][0]LogTemp: Warning: [SensorBulkRecovery] recovery\n"
        echo = "[2026.11.05][1]LogAutomationController: Warning: LogTemp: [SensorBulkRecovery] recovery\n"
        self.assertTrue(loss_verdict(dict(sender=sender, pass_=True, **{"pass": True}), original + echo)["pass"])
        self.assertFalse(loss_verdict({"sender": sender, "pass": True}, original * 2)["pass"])
        sender["bulk_outage_loss"] = 0
        self.assertFalse(loss_verdict({"sender": sender, "pass": True}, original)["pass"])


if __name__ == "__main__":
    unittest.main()

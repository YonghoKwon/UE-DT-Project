import unittest
import hashlib
import struct
import xml.etree.ElementTree as ET

from configure import NODES, NS, broker_xml, haproxy_config
from stomp_wire import Parser, encode, escape, unescape
from run_scenarios import check_pcd, Cluster, complete_suite


class LocalToolsTests(unittest.TestCase):
    def test_four_nodes_replication_and_drop_policy(self):
        for node in NODES:
            xml = broker_xml(node, "fixture-cluster-marker", 4096, 65536)
            self.assertIn('<core xmlns="urn:activemq:core">', xml)
            self.assertNotIn(':core', xml.replace('urn:activemq:core', ''))
            root = ET.fromstring(xml)
            core = root.find(f"{{{NS}}}core")
            self.assertIsNotNone(core)
            role = "primary" if node.startswith("live") else "backup"
            self.assertIsNotNone(core.find(f"{{{NS}}}ha-policy/{{{NS}}}replication/{{{NS}}}{role}"))
            values = core.findall(f".//{{{NS}}}message-load-balancing")
            self.assertEqual([v.text for v in values], ["ON_DEMAND"])
            pcd = next(x for x in core.findall(f"{{{NS}}}address-settings/{{{NS}}}address-setting") if x.get("match").endswith("export.#"))
            self.assertEqual(pcd.find(f"{{{NS}}}address-full-policy").text, "DROP")
            self.assertEqual(pcd.find(f"{{{NS}}}max-size-bytes").text, "65536")
            self.assertEqual(pcd.find(f"{{{NS}}}redistribution-delay").text, "0")
            acceptor = core.find(f"{{{NS}}}acceptors/{{{NS}}}acceptor").text
            self.assertIn("stompMinLargeMessageSize=4096", acceptor)

    def test_readiness_check_is_stomp_not_tcp_only(self):
        config = haproxy_config("fixture-user", "fixture-marker")
        self.assertIn("balance roundrobin", config)
        self.assertIn("tcp-check expect string CONNECTED", config)
        self.assertEqual(config.count(" check inter "), 4)
        self.assertEqual(config.count("init-addr last,libc,none"), 4)
        self.assertIn("nameserver dns 127.0.0.11:53", config)
        self.assertNotIn("fixture-marker", config)

    def test_binary_fragmented_and_coalesced(self):
        parser = Parser()
        message = encode("MESSAGE", {"destination": "topic.fixture", "x-test": "a:b\nc\\d"}, b"\0\xff\0xyz")
        wire = b"\n" + message + encode("RECEIPT", {"receipt-id": "second"})
        frames = []
        for i in range(0, len(wire), 3):
            parser.feed(wire[i:i + 3])
            while (frame := parser.pop()) is not None:
                frames.append(frame)
        self.assertEqual(len(frames), 2)
        self.assertEqual(frames[0][2], b"\0\xff\0xyz")
        self.assertEqual(frames[0][1]["x-test"], "a:b\nc\\d")
        self.assertEqual(frames[1][1]["receipt-id"], "second")

    def test_invalid_boundaries_and_escape_rejected(self):
        parser = Parser(20)
        with self.assertRaises(ValueError):
            parser.feed(b"x" * 21)
        parser = Parser()
        parser.feed(b"MESSAGE\ncontent-length:2\n\nabX")
        with self.assertRaises(ValueError):
            parser.pop()
        with self.assertRaises(ValueError):
            unescape("bad\\q")
        self.assertEqual(unescape(escape("a:b\nc\\d\r")), "a:b\nc\\d\r")

    def test_full_byte_point_oracle_and_checksum(self):
        header = b"VERSION .7\nFIELDS x y z rest\nSIZE 4 4 4 1\nTYPE F F F U\nCOUNT 1 1 1 21\nPOINTS 2\nDATA binary\n"
        body = header + (struct.pack("<fff", 1, 2, 3) + bytes(21)) * 2
        headers = {"schema": "virtual-pointcloud.pcd.v1", "point-count": "2",
                   "checksum": hashlib.sha1(body).hexdigest()}
        self.assertEqual(check_pcd(headers, body), 2)
        corrupt = body[:-1] + b"\1"
        with self.assertRaises(ValueError):
            check_pcd(headers, corrupt)
        headers["checksum"] = hashlib.sha1(corrupt).hexdigest()
        with self.assertRaises(ValueError):
            check_pcd(headers, corrupt)

    def test_test_credentials_are_redacted(self):
        cluster = Cluster.__new__(Cluster)
        cluster.env = {"ARTEMIS_USER": "fixture", "ARTEMIS_PASSWORD": "fixture-password",
                       "CLUSTER_PASSWORD": "fixture-cluster"}
        output = cluster.scrub("fixture-password fixture-cluster tcp-check send-binary 434f4e4e45435400")
        self.assertNotIn("fixture-password", output)
        self.assertNotIn("fixture-cluster", output)
        self.assertNotIn("434f4e4e45435400", output)

    def test_missing_or_failed_scenario_cannot_be_reported_pass(self):
        passed = [{"name": "a", "pass": True}, {"name": "b", "pass": True}]
        self.assertTrue(complete_suite(passed, ["a", "b"], None, []))
        self.assertFalse(complete_suite(passed[:1], ["a", "b"], None, []))
        self.assertFalse(complete_suite(passed, ["a", "b"], {"type": "fixture"}, []))
        self.assertFalse(complete_suite(passed, ["a", "b"], None, ["cleanup failed"]))


if __name__ == "__main__":
    unittest.main()

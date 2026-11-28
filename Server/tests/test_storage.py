"""Tests of smarthouse.storage on a temporary database.

Run from Server/:  python -m unittest discover -s tests -v
"""

import sqlite3
import tempfile
import threading
import unittest
from pathlib import Path

from smarthouse import contract as c
from smarthouse import storage as s

T0 = 1_795_000_000_000

class StorageTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.path = Path(self.tmp.name) / "data" / "smarthouse.db"
        self.db = s.Storage(self.path)

    def tearDown(self):
        self.db.close()
        self.tmp.cleanup()

    def test_schema(self):
        self.assertTrue(self.path.is_file())
        expected = {"connections", "states", "measurements", "durations", "events",
                    "commands", "images", "settings", "alerts", "access_log"}
        self.assertTrue(expected <= set(self.db.tables()))
        conn = sqlite3.connect(self.path)
        self.assertEqual(conn.execute("PRAGMA journal_mode").fetchone()[0], "wal")
        self.assertEqual(conn.execute("PRAGMA user_version").fetchone()[0], s.SCHEMA_VERSION)
        conn.close()

    def test_reopen_keeps_data(self):
        self.db.add_connection(True, T0)
        self.db.close()
        again = s.Storage(self.path)
        self.assertEqual(again.latest_connection()["online"], 1)
        again.close()

    def test_newer_schema_refused(self):
        self.db.close()
        conn = sqlite3.connect(self.path)
        conn.execute(f"PRAGMA user_version = {s.SCHEMA_VERSION + 1}")
        conn.close()
        with self.assertRaises(RuntimeError):
            s.Storage(self.path)

    def test_board_messages(self):
        self.db.add_state(c.StateMsg(c.Alarm.INTRUSION, False, False, True), T0)
        self.db.add_measurement(c.MeasurementMsg(21, 21.4, 45.2, 1013.2, 12.3), True, T0)
        self.db.add_durations(c.DurationsMsg(60_000, 0, 1200, 4500), T0)
        self.db.add_event(c.EventMsg(5, c.EventType.INTRUSION, 8.4), T0)

        state = self.db.latest_state()
        self.assertEqual((state["alarm"], state["streaming"]), ("intrusion", 1))
        meas = self.db.latest_measurement()
        self.assertEqual((meas["seq"], meas["gas_valid"]), (21, 1))
        self.assertEqual(len(self.db.durations_between(T0, T0)), 1)
        self.assertEqual(self.db.recent_events()[0]["distance_cm"], 8.4)

    def test_command_ids_never_reused(self):
        first = self.db.create_command("REARMER", "admin", T0)
        second = self.db.create_command("OBTENIR", "admin", T0)
        self.assertEqual(second, first + 1)

        self.db.purge(T0 + 31 * s.DAY_MS, retention_days=30)
        self.assertIsNone(self.db.get_command(second))
        third = self.db.create_command("ALERTER", "admin", T0 + 31 * s.DAY_MS)
        self.assertEqual(third, second + 1)

    def test_command_result_first_ack_only(self):
        cid = self.db.create_command("REARMER", "admin", T0)
        self.assertTrue(self.db.set_command_result(cid, "ok", T0 + 100))

        self.assertFalse(self.db.set_command_result(cid, "refuse", T0 + 200))
        row = self.db.get_command(cid)
        self.assertEqual((row["result"], row["ack_at"]), ("ok", T0 + 100))
        self.assertFalse(self.db.set_command_result(9999, "ok", T0))

    def test_settings_upsert(self):
        self.assertEqual(self.db.get_setting("temp_setpoint_c", "20.0"), "20.0")
        self.db.set_setting("temp_setpoint_c", "21.5", "admin", T0)
        self.db.set_setting("temp_setpoint_c", "19.0", "admin", T0 + 1)
        self.assertEqual(self.db.get_setting("temp_setpoint_c"), "19.0")

    def test_alerts_open_close(self):
        first = self.db.open_alert(s.ALERT_TEMPERATURE_HIGH, T0)
        self.assertEqual(self.db.open_alert(s.ALERT_TEMPERATURE_HIGH, T0 + 1), first)
        self.assertEqual(len(self.db.open_alerts()), 1)
        self.assertTrue(self.db.close_alert(s.ALERT_TEMPERATURE_HIGH, T0 + 2))
        self.assertFalse(self.db.close_alert(s.ALERT_TEMPERATURE_HIGH, T0 + 3))
        self.assertIsNone(self.db.current_alert(s.ALERT_TEMPERATURE_HIGH))

    def test_access_log(self):
        self.db.log_access("172.20.10.5", s.ACCESS_FAILURE, T0)
        self.db.log_access("172.20.10.5", s.ACCESS_FAILURE, T0 + 1000)
        self.db.log_access("172.20.10.5", s.ACCESS_SUCCESS, T0 + 2000)
        self.db.log_access("172.20.10.6", s.ACCESS_FAILURE, T0 + 2000)
        self.assertEqual(self.db.failures_since("172.20.10.5", T0), 2)
        self.assertEqual(self.db.failures_since("172.20.10.5", T0 + 500), 1)

    def test_images(self):
        self.db.add_image(3, 3192, 3150, "images/3.bin", T0, T0 + 7 * s.DAY_MS)
        self.assertEqual(self.db.expired_images(T0 + s.DAY_MS), [])
        expired = self.db.expired_images(T0 + 7 * s.DAY_MS)
        self.assertEqual(len(expired), 1)
        self.db.delete_image(expired[0]["id"])
        self.assertEqual(self.db.list_images(), [])

    def test_purge(self):
        old, recent = T0, T0 + 20 * s.DAY_MS
        now = T0 + 31 * s.DAY_MS
        for at in (old, recent):
            self.db.add_connection(True, at)
            self.db.log_access("x", s.ACCESS_SUCCESS, at)
        self.db.open_alert(s.ALERT_BOARD_OFFLINE, old)
        self.db.open_alert(s.ALERT_TEMPERATURE_LOW, old)
        self.db.close_alert(s.ALERT_TEMPERATURE_LOW, old + 1)
        deleted = self.db.purge(now, retention_days=30)
        self.assertEqual(deleted["connections"], 1)
        self.assertEqual(deleted["access_log"], 1)
        self.assertEqual(deleted["alerts"], 1)
        self.assertIsNotNone(self.db.current_alert(s.ALERT_BOARD_OFFLINE))

    def test_threads(self):
        """MQTT thread writes while another thread reads (one connection each)."""
        errors = []

        def writer():
            try:
                for i in range(200):
                    self.db.add_measurement(c.MeasurementMsg(i, 20.0, 40.0, 1000.0, 10.0),
                                            i >= 20, T0 + i)
            except Exception as exc:
                errors.append(exc)
            finally:
                self.db.close()

        def reader():
            try:
                for _ in range(200):
                    self.db.latest_measurement()
            except Exception as exc:
                errors.append(exc)
            finally:
                self.db.close()

        threads = [threading.Thread(target=writer), threading.Thread(target=reader)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        self.assertEqual(errors, [])
        self.assertEqual(len(self.db.measurements_between(T0, T0 + 1000)), 200)

if __name__ == "__main__":
    unittest.main()

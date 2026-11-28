"""Tests of smarthouse.config (values come from the environment, no real .env).

Run from Server/:  python -m unittest discover -s tests -v
"""

import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from smarthouse import config

VALID_ENV = {
    "MQTT_HOST": "127.0.0.1",
    "MQTT_PORT": "1883",
    "MQTT_USERNAME": "user-example",
    "MQTT_PASSWORD": "password-example",
    "IMAGE_KEY": "key-example",
    "DASHBOARD_USERNAME": "admin-example",
    "DASHBOARD_PASSWORD_HASH": "hash-example",
    "STORAGE_SECRET": "secret-example",
    "HTTPS_CERTFILE": "",
    "HTTPS_KEYFILE": "",
    "DATA_DIR": "data",
}

MISSING_ENV_FILE = Path("does-not-exist.env")

class LoadSettingsTest(unittest.TestCase):
    def load(self, **changes):
        env = {**VALID_ENV, **changes}
        with mock.patch.dict(os.environ, env, clear=True):
            return config.load_settings(MISSING_ENV_FILE)

    def test_valid(self):
        settings = self.load()
        self.assertEqual(settings.mqtt_port, 1883)
        self.assertFalse(settings.https_enabled)
        self.assertEqual(settings.data_dir, config.SERVER_DIR / "data")
        self.assertEqual(settings.database_path.name, "smarthouse.db")

    def test_repr_hides_secrets(self):
        text = repr(self.load())
        for name in ("MQTT_PASSWORD", "IMAGE_KEY", "DASHBOARD_PASSWORD_HASH", "STORAGE_SECRET"):
            self.assertNotIn(VALID_ENV[name], text)

    def test_missing_secret(self):
        for name in ("MQTT_PASSWORD", "IMAGE_KEY", "STORAGE_SECRET"):
            with self.subTest(name=name), self.assertRaises(config.ConfigError) as ctx:
                self.load(**{name: "  "})
            self.assertIn(name, str(ctx.exception))

    def test_bad_port(self):
        with self.assertRaises(config.ConfigError):
            self.load(MQTT_PORT="huit")

    def test_https_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            cert, key = Path(tmp, "cert.pem"), Path(tmp, "key.pem")
            cert.write_text("x")
            key.write_text("x")
            self.assertTrue(self.load(HTTPS_CERTFILE=str(cert), HTTPS_KEYFILE=str(key)).https_enabled)
            with self.assertRaises(config.ConfigError):
                self.load(HTTPS_CERTFILE=str(cert))
            with self.assertRaises(config.ConfigError):
                self.load(HTTPS_CERTFILE=str(cert), HTTPS_KEYFILE=str(Path(tmp, "nope.pem")))

    def test_constants(self):
        self.assertEqual(config.GAS_WARMUP_SEQ, 20)
        self.assertLess(config.TEMP_SETPOINT_MIN_C, config.TEMP_SETPOINT_DEFAULT_C)
        self.assertLess(config.TEMP_SETPOINT_DEFAULT_C, config.TEMP_SETPOINT_MAX_C)
        self.assertLess(config.TEMP_HYSTERESIS_C, config.TEMP_TOLERANCE_C)

if __name__ == "__main__":
    unittest.main()

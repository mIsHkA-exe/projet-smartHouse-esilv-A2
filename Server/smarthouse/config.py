"""Server configuration.

Two kinds of values:
- validated operating parameters: plain constants below, versioned in Git
  (they are not secrets, the team must be able to read them);
- secrets and machine-specific paths: read from ``Server/.env``, never
  committed (model: ``Server/.env.example``).
"""

from __future__ import annotations

import os
from dataclasses import dataclass
from pathlib import Path

SERVER_DIR = Path(__file__).resolve().parent.parent
ENV_FILE = SERVER_DIR / ".env"

TEMP_SETPOINT_DEFAULT_C = 20.0
TEMP_SETPOINT_MIN_C = 10.0
TEMP_SETPOINT_MAX_C = 30.0
TEMP_TOLERANCE_C = 2.0
TEMP_HYSTERESIS_C = 0.5

DATA_RETENTION_DAYS = 30
IMAGE_RETENTION_DAYS = 7

MEASUREMENT_PERIOD_S = 15
DURATIONS_PERIOD_S = 60

GAS_WARMUP_S = 300
GAS_WARMUP_SEQ = GAS_WARMUP_S // MEASUREMENT_PERIOD_S

STREAM_MAX_S = 60
IMAGE_ABANDON_S = 10

MQTT_CLIENT_ID = "smarthouse-serveur"
MQTT_KEEPALIVE_S = 60

DASHBOARD_PORT = 8443

class ConfigError(RuntimeError):
    """Missing or invalid configuration value."""

@dataclass(frozen=True)
class Settings:
    """Secrets and machine-specific values read from Server/.env."""

    mqtt_host: str
    mqtt_port: int
    mqtt_username: str
    mqtt_password: str
    image_key: str
    dashboard_username: str
    dashboard_password_hash: str
    storage_secret: str
    https_certfile: Path | None
    https_keyfile: Path | None
    data_dir: Path

    @property
    def database_path(self) -> Path:
        return self.data_dir / "smarthouse.db"

    @property
    def images_dir(self) -> Path:
        return self.data_dir / "images"

    @property
    def https_enabled(self) -> bool:
        return self.https_certfile is not None and self.https_keyfile is not None

    def __repr__(self) -> str:
        return (
            f"Settings(mqtt={self.mqtt_host}:{self.mqtt_port}, "
            f"user={self.mqtt_username}, https={self.https_enabled}, "
            f"data_dir={self.data_dir})"
        )

def _required(name: str) -> str:
    value = os.environ.get(name, "").strip()
    if not value:
        raise ConfigError(f"{name} is missing in {ENV_FILE}")
    return value

def _optional_path(name: str) -> Path | None:
    value = os.environ.get(name, "").strip()
    if not value:
        return None
    path = Path(value)
    if not path.is_file():
        raise ConfigError(f"{name}: file not found: {path}")
    return path

def load_settings(env_file: Path = ENV_FILE) -> Settings:
    """Read Server/.env (if present) and return the settings.

    Values already set in the process environment take precedence.
    """
    try:
        from dotenv import load_dotenv
    except ImportError as exc:
        raise ConfigError("python-dotenv is not installed") from exc
    load_dotenv(env_file, override=False)

    port_text = os.environ.get("MQTT_PORT", "1883").strip()
    try:
        mqtt_port = int(port_text)
    except ValueError as exc:
        raise ConfigError(f"MQTT_PORT is not a number: {port_text!r}") from exc

    certfile = _optional_path("HTTPS_CERTFILE")
    keyfile = _optional_path("HTTPS_KEYFILE")
    if (certfile is None) != (keyfile is None):
        raise ConfigError("HTTPS_CERTFILE and HTTPS_KEYFILE must be set together")

    data_dir = Path(os.environ.get("DATA_DIR", "data").strip() or "data")
    if not data_dir.is_absolute():
        data_dir = SERVER_DIR / data_dir

    return Settings(
        mqtt_host=os.environ.get("MQTT_HOST", "127.0.0.1").strip(),
        mqtt_port=mqtt_port,
        mqtt_username=_required("MQTT_USERNAME"),
        mqtt_password=_required("MQTT_PASSWORD"),
        image_key=_required("IMAGE_KEY"),
        dashboard_username=_required("DASHBOARD_USERNAME"),
        dashboard_password_hash=_required("DASHBOARD_PASSWORD_HASH"),
        storage_secret=_required("STORAGE_SECRET"),
        https_certfile=certfile,
        https_keyfile=keyfile,
        data_dir=data_dir,
    )

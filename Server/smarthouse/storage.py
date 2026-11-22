"""SQLite storage (one database file in DATA_DIR).

- One connection per thread (the sqlite3 module forbids sharing one between
  threads by default): the MQTT thread writes, the dashboard reads.
- WAL mode: readers are not blocked while a write is in progress.
- Times are integers: milliseconds UTC since 1970 (converted to Paris time
  only for display).
- commands.id uses AUTOINCREMENT: SQLite never reuses an id, even after the
  rows are purged, so the command counter survives restarts. It is reset
  only if the database file itself is deleted.
"""

from __future__ import annotations

import sqlite3
import threading
import time
from pathlib import Path
from typing import Iterator

from .contract import DurationsMsg, EventMsg, MeasurementMsg, StateMsg

SCHEMA_VERSION = 1
DAY_MS = 24 * 60 * 60 * 1000

_SCHEMA = """
CREATE TABLE IF NOT EXISTS connections (
    id          INTEGER PRIMARY KEY,
    received_at INTEGER NOT NULL,
    online      INTEGER NOT NULL CHECK (online IN (0, 1))
);
CREATE TABLE IF NOT EXISTS states (
    id          INTEGER PRIMARY KEY,
    received_at INTEGER NOT NULL,
    alarm       TEXT    NOT NULL,
    buzzer      INTEGER NOT NULL CHECK (buzzer IN (0, 1)),
    red_led     INTEGER NOT NULL CHECK (red_led IN (0, 1)),
    streaming   INTEGER NOT NULL CHECK (streaming IN (0, 1))
);
CREATE TABLE IF NOT EXISTS measurements (
    id          INTEGER PRIMARY KEY,
    received_at INTEGER NOT NULL,
    seq         INTEGER NOT NULL,
    temperature REAL    NOT NULL,
    humidity    REAL    NOT NULL,
    pressure    REAL    NOT NULL,
    gas         REAL    NOT NULL,
    gas_valid   INTEGER NOT NULL CHECK (gas_valid IN (0, 1))
);
CREATE INDEX IF NOT EXISTS measurements_received_at ON measurements (received_at);
CREATE TABLE IF NOT EXISTS durations (
    id          INTEGER PRIMARY KEY,
    received_at INTEGER NOT NULL,
    since_ms    INTEGER NOT NULL,
    buzzer_ms   INTEGER NOT NULL,
    red_led_ms  INTEGER NOT NULL,
    camera_ms   INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS durations_received_at ON durations (received_at);
CREATE TABLE IF NOT EXISTS events (
    id          INTEGER PRIMARY KEY,
    received_at INTEGER NOT NULL,
    seq         INTEGER NOT NULL,
    type        TEXT    NOT NULL,
    distance_cm REAL    NOT NULL
);
CREATE TABLE IF NOT EXISTS commands (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    word        TEXT    NOT NULL,
    sent_at     INTEGER NOT NULL,
    sent_by     TEXT    NOT NULL,
    result      TEXT,
    ack_at      INTEGER
);
CREATE TABLE IF NOT EXISTS images (
    id          INTEGER PRIMARY KEY,
    image_num   INTEGER NOT NULL,
    received_at INTEGER NOT NULL,
    sent_size   INTEGER NOT NULL,
    jpeg_size   INTEGER NOT NULL,
    file_path   TEXT    NOT NULL,
    expires_at  INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS images_expires_at ON images (expires_at);
CREATE TABLE IF NOT EXISTS settings (
    key         TEXT    PRIMARY KEY,
    value       TEXT    NOT NULL,
    updated_at  INTEGER NOT NULL,
    updated_by  TEXT    NOT NULL
);
CREATE TABLE IF NOT EXISTS alerts (
    id          INTEGER PRIMARY KEY,
    type        TEXT    NOT NULL,
    started_at  INTEGER NOT NULL,
    ended_at    INTEGER
);
CREATE TABLE IF NOT EXISTS access_log (
    id          INTEGER PRIMARY KEY,
    at          INTEGER NOT NULL,
    address     TEXT    NOT NULL,
    result      TEXT    NOT NULL
);
"""

# Alert types (alerts.type).
ALERT_TEMPERATURE_HIGH = "temperature_high"
ALERT_TEMPERATURE_LOW = "temperature_low"
ALERT_BOARD_OFFLINE = "board_offline"

# Access results (access_log.result).
ACCESS_SUCCESS = "success"
ACCESS_FAILURE = "failure"
ACCESS_BLOCKED = "blocked"

# Tables purged after DATA_RETENTION_DAYS, with their time column.
# Images are purged separately (their encrypted file must be deleted first).
_PURGEABLE = {
    "connections": "received_at",
    "states": "received_at",
    "measurements": "received_at",
    "durations": "received_at",
    "events": "received_at",
    "commands": "sent_at",
    "access_log": "at",
}


def now_ms() -> int:
    """Current time, milliseconds UTC since 1970."""
    return time.time_ns() // 1_000_000


class Storage:
    """Access to the database; safe to share between threads."""

    def __init__(self, database_path: Path) -> None:
        self.database_path = Path(database_path)
        self.database_path.parent.mkdir(parents=True, exist_ok=True)
        self._local = threading.local()
        self._init_schema()

    # --- connections ----------------------------------------------------
    def _connection(self) -> sqlite3.Connection:
        conn = getattr(self._local, "conn", None)
        if conn is None:
            conn = sqlite3.connect(self.database_path, timeout=5.0)
            conn.row_factory = sqlite3.Row
            conn.execute("PRAGMA journal_mode=WAL")
            conn.execute("PRAGMA foreign_keys=ON")
            self._local.conn = conn
        return conn

    def close(self) -> None:
        """Close this thread's connection."""
        conn = getattr(self._local, "conn", None)
        if conn is not None:
            conn.close()
            self._local.conn = None

    def _write(self, sql: str, params: tuple = ()) -> int:
        conn = self._connection()
        with conn:  # commit, or rollback on error
            cursor = conn.execute(sql, params)
        return cursor.lastrowid

    def _rows(self, sql: str, params: tuple = ()) -> list[sqlite3.Row]:
        return self._connection().execute(sql, params).fetchall()

    def _row(self, sql: str, params: tuple = ()) -> sqlite3.Row | None:
        return self._connection().execute(sql, params).fetchone()

    def _init_schema(self) -> None:
        conn = self._connection()
        version = conn.execute("PRAGMA user_version").fetchone()[0]
        if version > SCHEMA_VERSION:
            self.close()
            raise RuntimeError(
                f"database schema v{version} is newer than this server (v{SCHEMA_VERSION})"
            )
        with conn:
            conn.executescript(_SCHEMA)
            conn.execute(f"PRAGMA user_version = {SCHEMA_VERSION}")

    # --- board messages -------------------------------------------------
    def add_connection(self, online: bool, at: int) -> int:
        return self._write(
            "INSERT INTO connections (received_at, online) VALUES (?, ?)", (at, int(online))
        )

    def add_state(self, msg: StateMsg, at: int) -> int:
        return self._write(
            "INSERT INTO states (received_at, alarm, buzzer, red_led, streaming)"
            " VALUES (?, ?, ?, ?, ?)",
            (at, msg.alarm.value, int(msg.buzzer), int(msg.red_led), int(msg.streaming)),
        )

    def add_measurement(self, msg: MeasurementMsg, gas_valid: bool, at: int) -> int:
        return self._write(
            "INSERT INTO measurements"
            " (received_at, seq, temperature, humidity, pressure, gas, gas_valid)"
            " VALUES (?, ?, ?, ?, ?, ?, ?)",
            (at, msg.seq, msg.temperature, msg.humidity, msg.pressure, msg.gas, int(gas_valid)),
        )

    def add_durations(self, msg: DurationsMsg, at: int) -> int:
        return self._write(
            "INSERT INTO durations (received_at, since_ms, buzzer_ms, red_led_ms, camera_ms)"
            " VALUES (?, ?, ?, ?, ?)",
            (at, msg.since_ms, msg.buzzer_ms, msg.red_led_ms, msg.camera_ms),
        )

    def add_event(self, msg: EventMsg, at: int) -> int:
        return self._write(
            "INSERT INTO events (received_at, seq, type, distance_cm) VALUES (?, ?, ?, ?)",
            (at, msg.seq, msg.type.value, msg.distance_cm),
        )

    # --- commands -------------------------------------------------------
    def create_command(self, word: str, sent_by: str, at: int) -> int:
        """Record a command and return its id (the id sent to the board)."""
        return self._write(
            "INSERT INTO commands (word, sent_at, sent_by) VALUES (?, ?, ?)",
            (word, at, sent_by),
        )

    def set_command_result(self, command_id: int, result: str, at: int) -> bool:
        """Store the first acknowledgement of a command.

        Returns False if the id is unknown or already acknowledged
        (duplicate acknowledgements are expected with QoS 1).
        """
        conn = self._connection()
        with conn:
            cursor = conn.execute(
                "UPDATE commands SET result = ?, ack_at = ? WHERE id = ? AND result IS NULL",
                (result, at, command_id),
            )
        return cursor.rowcount == 1

    def get_command(self, command_id: int) -> sqlite3.Row | None:
        return self._row("SELECT * FROM commands WHERE id = ?", (command_id,))

    def recent_commands(self, limit: int = 50) -> list[sqlite3.Row]:
        return self._rows("SELECT * FROM commands ORDER BY id DESC LIMIT ?", (limit,))

    # --- images (kept images only) --------------------------------------
    def add_image(self, image_num: int, sent_size: int, jpeg_size: int,
                  file_path: str, at: int, expires_at: int) -> int:
        return self._write(
            "INSERT INTO images"
            " (image_num, received_at, sent_size, jpeg_size, file_path, expires_at)"
            " VALUES (?, ?, ?, ?, ?, ?)",
            (image_num, at, sent_size, jpeg_size, file_path, expires_at),
        )

    def list_images(self, limit: int = 100) -> list[sqlite3.Row]:
        return self._rows("SELECT * FROM images ORDER BY received_at DESC LIMIT ?", (limit,))

    def expired_images(self, now: int) -> list[sqlite3.Row]:
        return self._rows("SELECT * FROM images WHERE expires_at <= ?", (now,))

    def delete_image(self, image_id: int) -> None:
        """Delete the row; the caller deletes the encrypted file first."""
        self._write("DELETE FROM images WHERE id = ?", (image_id,))

    # --- settings -------------------------------------------------------
    def get_setting(self, key: str, default: str | None = None) -> str | None:
        row = self._row("SELECT value FROM settings WHERE key = ?", (key,))
        return default if row is None else row["value"]

    def set_setting(self, key: str, value: str, updated_by: str, at: int) -> None:
        self._write(
            "INSERT INTO settings (key, value, updated_at, updated_by) VALUES (?, ?, ?, ?)"
            " ON CONFLICT(key) DO UPDATE SET value = excluded.value,"
            " updated_at = excluded.updated_at, updated_by = excluded.updated_by",
            (key, value, at, updated_by),
        )

    # --- alerts ---------------------------------------------------------
    def open_alert(self, alert_type: str, at: int) -> int:
        """Open an alert unless one of the same type is already open."""
        current = self.current_alert(alert_type)
        if current is not None:
            return current["id"]
        return self._write(
            "INSERT INTO alerts (type, started_at) VALUES (?, ?)", (alert_type, at)
        )

    def close_alert(self, alert_type: str, at: int) -> bool:
        conn = self._connection()
        with conn:
            cursor = conn.execute(
                "UPDATE alerts SET ended_at = ? WHERE type = ? AND ended_at IS NULL",
                (at, alert_type),
            )
        return cursor.rowcount > 0

    def current_alert(self, alert_type: str) -> sqlite3.Row | None:
        return self._row(
            "SELECT * FROM alerts WHERE type = ? AND ended_at IS NULL"
            " ORDER BY started_at DESC LIMIT 1",
            (alert_type,),
        )

    def open_alerts(self) -> list[sqlite3.Row]:
        return self._rows("SELECT * FROM alerts WHERE ended_at IS NULL ORDER BY started_at")

    # --- dashboard access log -------------------------------------------
    def log_access(self, address: str, result: str, at: int) -> int:
        return self._write(
            "INSERT INTO access_log (at, address, result) VALUES (?, ?, ?)",
            (at, address, result),
        )

    def failures_since(self, address: str, since: int) -> int:
        row = self._row(
            "SELECT COUNT(*) FROM access_log WHERE address = ? AND result = ? AND at >= ?",
            (address, ACCESS_FAILURE, since),
        )
        return row[0]

    # --- queries for the dashboard ---------------------------------------
    def latest_state(self) -> sqlite3.Row | None:
        return self._row("SELECT * FROM states ORDER BY received_at DESC, id DESC LIMIT 1")

    def latest_connection(self) -> sqlite3.Row | None:
        return self._row("SELECT * FROM connections ORDER BY received_at DESC, id DESC LIMIT 1")

    def latest_measurement(self) -> sqlite3.Row | None:
        return self._row("SELECT * FROM measurements ORDER BY received_at DESC, id DESC LIMIT 1")

    def measurements_between(self, start: int, end: int) -> list[sqlite3.Row]:
        return self._rows(
            "SELECT * FROM measurements WHERE received_at BETWEEN ? AND ? ORDER BY received_at",
            (start, end),
        )

    def durations_between(self, start: int, end: int) -> list[sqlite3.Row]:
        return self._rows(
            "SELECT * FROM durations WHERE received_at BETWEEN ? AND ? ORDER BY received_at",
            (start, end),
        )

    def recent_events(self, limit: int = 50) -> list[sqlite3.Row]:
        return self._rows("SELECT * FROM events ORDER BY received_at DESC LIMIT ?", (limit,))

    # --- retention --------------------------------------------------------
    def purge(self, now: int, retention_days: int) -> dict[str, int]:
        """Delete rows older than retention_days (images excluded).

        Closed alerts are purged; open alerts are always kept.
        Returns the number of deleted rows per table.
        """
        limit = now - retention_days * DAY_MS
        deleted: dict[str, int] = {}
        conn = self._connection()
        with conn:
            for table, column in _PURGEABLE.items():
                cursor = conn.execute(f"DELETE FROM {table} WHERE {column} < ?", (limit,))
                deleted[table] = cursor.rowcount
            cursor = conn.execute(
                "DELETE FROM alerts WHERE ended_at IS NOT NULL AND ended_at < ?", (limit,)
            )
            deleted["alerts"] = cursor.rowcount
        return deleted

    def tables(self) -> Iterator[str]:
        for row in self._rows("SELECT name FROM sqlite_master WHERE type = 'table'"):
            yield row["name"]

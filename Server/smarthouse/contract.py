"""MQTT contract between the board and the server (docs/mqtt-topics.txt v0.8).

The contract itself stays in French (validated and tested on the board).
This module is the ONLY place where French topic names, JSON keys and
command words appear: everything else in the server uses the English
names defined here.

Every message from the board is validated before use: wrong type, missing
key or out-of-range value -> ContractError (the message is then ignored).
"""

from __future__ import annotations

import json
import math
import re
from dataclasses import dataclass
from enum import Enum
from typing import Union

CONTRACT_VERSION = "0.8"

# --- Topics (contract section 2) --------------------------------------
ROOT = "smarthouse"
TOPIC_CONNECTION = f"{ROOT}/carte/connexion"
TOPIC_STATE = f"{ROOT}/carte/etat"
TOPIC_MEASUREMENTS = f"{ROOT}/carte/mesures"
TOPIC_DURATIONS = f"{ROOT}/carte/durees"
TOPIC_EVENT = f"{ROOT}/carte/evenement"
TOPIC_ACK = f"{ROOT}/carte/ack"
TOPIC_IMAGE_ANNOUNCE = f"{ROOT}/carte/image/annonce"
TOPIC_COMMAND = f"{ROOT}/serveur/commande"

# The server subscribes to everything the board publishes.
SUBSCRIPTION = f"{ROOT}/carte/#"
SUBSCRIPTION_QOS = 1
COMMAND_QOS = 1            # commands are NEVER retained (contract section 4)

_IMAGE_CHUNK_RE = re.compile(rf"^{ROOT}/carte/image/(\d{{1,9}})/(\d{{1,4}})$")

# --- Limits (contract section 3.7) ------------------------------------
CHUNK_SIZE = 1024
MAX_IMAGE_BYTES = 59_999                      # board refuses 0 or >= 60 000 bytes
MAX_CHUNKS = math.ceil(MAX_IMAGE_BYTES / CHUNK_SIZE)
MAX_COMMAND_ID = 4_294_967_295                # unsigned long on the board


class ContractError(ValueError):
    """Message that does not follow the contract."""


# --- Enumerations: English names, French wire values -----------------
class Alarm(str, Enum):
    LATENT = "latent"
    INTRUSION = "intrusion"
    ALERT = "alerte"


class Command(str, Enum):
    GET_IMAGES = "OBTENIR"
    STOP_STREAM = "ARRETER_DIFFUSION"
    ALERT = "ALERTER"
    STOP_BUZZER = "STOP_BUZZER"
    REARM = "REARMER"


class AckResult(str, Enum):
    OK = "ok"
    REFUSED = "refuse"
    UNKNOWN = "inconnu"


class EventType(str, Enum):
    INTRUSION = "intrusion"


# --- Decoded messages -------------------------------------------------
@dataclass(frozen=True)
class ConnectionMsg:
    online: bool


@dataclass(frozen=True)
class StateMsg:
    alarm: Alarm
    buzzer: bool
    red_led: bool
    streaming: bool


@dataclass(frozen=True)
class MeasurementMsg:
    seq: int
    temperature: float
    humidity: float
    pressure: float
    gas: float


@dataclass(frozen=True)
class DurationsMsg:
    since_ms: int
    buzzer_ms: int
    red_led_ms: int
    camera_ms: int


@dataclass(frozen=True)
class EventMsg:
    seq: int
    type: EventType
    distance_cm: float


@dataclass(frozen=True)
class AckMsg:
    id: int              # 0 = command without a valid id
    command: str         # word as received by the board ("" if invalid)
    result: AckResult


@dataclass(frozen=True)
class ImageAnnounceMsg:
    image_num: int
    sent_size: int       # bytes sent, JPEG + possible padding (contract v0.6)
    chunks: int


@dataclass(frozen=True)
class ImageChunkMsg:
    image_num: int
    index: int
    data: bytes


BoardMessage = Union[
    ConnectionMsg, StateMsg, MeasurementMsg, DurationsMsg, EventMsg,
    AckMsg, ImageAnnounceMsg, ImageChunkMsg,
]


# --- Field helpers ----------------------------------------------------
def _json_object(payload: bytes) -> dict:
    try:
        obj = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ContractError(f"invalid JSON: {exc}") from exc
    if not isinstance(obj, dict):
        raise ContractError("JSON payload is not an object")
    return obj


def _get(obj: dict, key: str) -> object:
    if key not in obj:
        raise ContractError(f"missing field {key!r}")
    return obj[key]


def _bool(obj: dict, key: str) -> bool:
    value = _get(obj, key)
    if not isinstance(value, bool):
        raise ContractError(f"{key!r} must be true/false")
    return value


def _int(obj: dict, key: str, minimum: int = 0, maximum: int | None = None) -> int:
    value = _get(obj, key)
    if isinstance(value, bool) or not isinstance(value, int):
        raise ContractError(f"{key!r} must be an integer")
    if value < minimum or (maximum is not None and value > maximum):
        raise ContractError(f"{key!r} out of range: {value}")
    return value


def _float(obj: dict, key: str) -> float:
    value = _get(obj, key)
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ContractError(f"{key!r} must be a number")
    value = float(value)
    if not math.isfinite(value):
        raise ContractError(f"{key!r} is not finite")
    return value


def _enum(enum_cls: type[Enum], obj: dict, key: str) -> Enum:
    value = _get(obj, key)
    try:
        return enum_cls(value)
    except ValueError as exc:
        raise ContractError(f"{key!r}: unknown value {value!r}") from exc


# --- Parsers (French wire keys -> English fields) ---------------------
def _parse_connection(obj: dict) -> ConnectionMsg:
    return ConnectionMsg(online=_bool(obj, "en_ligne"))


def _parse_state(obj: dict) -> StateMsg:
    return StateMsg(
        alarm=_enum(Alarm, obj, "alarme"),
        buzzer=_bool(obj, "buzzer"),
        red_led=_bool(obj, "led_rouge"),
        streaming=_bool(obj, "diffusion"),
    )


def _parse_measurement(obj: dict) -> MeasurementMsg:
    return MeasurementMsg(
        seq=_int(obj, "seq"),
        temperature=_float(obj, "temperature"),
        humidity=_float(obj, "humidite"),
        pressure=_float(obj, "pression"),
        gas=_float(obj, "gaz"),
    )


def _parse_durations(obj: dict) -> DurationsMsg:
    return DurationsMsg(
        since_ms=_int(obj, "depuis"),
        buzzer_ms=_int(obj, "buzzer"),
        red_led_ms=_int(obj, "led_rouge"),
        camera_ms=_int(obj, "camera"),
    )


def _parse_event(obj: dict) -> EventMsg:
    return EventMsg(
        seq=_int(obj, "seq"),
        type=_enum(EventType, obj, "type"),
        distance_cm=_float(obj, "distance_cm"),
    )


def _parse_ack(obj: dict) -> AckMsg:
    command = _get(obj, "cmd")
    if not isinstance(command, str) or len(command) > 20:
        raise ContractError("'cmd' must be a short string")
    return AckMsg(
        id=_int(obj, "id", maximum=MAX_COMMAND_ID),
        command=command,
        result=_enum(AckResult, obj, "resultat"),
    )


def _parse_image_announce(obj: dict) -> ImageAnnounceMsg:
    sent_size = _int(obj, "taille", minimum=1, maximum=MAX_IMAGE_BYTES)
    chunks = _int(obj, "morceaux", minimum=1, maximum=MAX_CHUNKS)
    if chunks != math.ceil(sent_size / CHUNK_SIZE):
        raise ContractError(f"chunk count {chunks} does not match size {sent_size}")
    return ImageAnnounceMsg(image_num=_int(obj, "img", minimum=1), sent_size=sent_size, chunks=chunks)


_JSON_PARSERS = {
    TOPIC_CONNECTION: _parse_connection,
    TOPIC_STATE: _parse_state,
    TOPIC_MEASUREMENTS: _parse_measurement,
    TOPIC_DURATIONS: _parse_durations,
    TOPIC_EVENT: _parse_event,
    TOPIC_ACK: _parse_ack,
    TOPIC_IMAGE_ANNOUNCE: _parse_image_announce,
}


def parse_board_message(topic: str, payload: bytes) -> BoardMessage:
    """Decode one message published by the board.

    Raises ContractError for an unknown topic or an invalid payload.
    """
    parser = _JSON_PARSERS.get(topic)
    if parser is not None:
        return parser(_json_object(payload))

    match = _IMAGE_CHUNK_RE.match(topic)
    if match:
        image_num, index = int(match.group(1)), int(match.group(2))
        if image_num < 1 or index >= MAX_CHUNKS:
            raise ContractError(f"image chunk out of range: {topic}")
        if not 1 <= len(payload) <= CHUNK_SIZE:
            raise ContractError(f"image chunk size out of range: {len(payload)}")
        return ImageChunkMsg(image_num=image_num, index=index, data=bytes(payload))

    raise ContractError(f"unknown topic: {topic}")


def format_command(command: Command, command_id: int) -> bytes:
    """Build a command payload: 'WORD ID' (contract section 4)."""
    if not isinstance(command, Command):
        raise ContractError(f"unknown command: {command!r}")
    if not 1 <= command_id <= MAX_COMMAND_ID:
        raise ContractError(f"command id out of range: {command_id}")
    return f"{command.value} {command_id}".encode("ascii")

"""Tests of smarthouse.contract against the examples of docs/mqtt-topics.txt v0.8.

Run from Server/:  python -m unittest discover -s tests -v
"""

import unittest

from smarthouse import contract as c


class ParseBoardMessageTest(unittest.TestCase):
    def test_connection(self):
        msg = c.parse_board_message(c.TOPIC_CONNECTION, b'{"en_ligne": true}')
        self.assertEqual(msg, c.ConnectionMsg(online=True))
        msg = c.parse_board_message(c.TOPIC_CONNECTION, b'{"en_ligne": false}')
        self.assertFalse(msg.online)

    def test_state(self):
        payload = b'{"alarme": "alerte", "buzzer": true, "led_rouge": true, "diffusion": false}'
        msg = c.parse_board_message(c.TOPIC_STATE, payload)
        self.assertEqual(msg, c.StateMsg(c.Alarm.ALERT, True, True, False))

    def test_state_v07_field_is_ignored(self):
        # led_temp was removed in v0.8: an extra key must not break parsing.
        payload = (b'{"alarme": "latent", "buzzer": false, "led_rouge": false,'
                   b' "diffusion": false, "led_temp": "verte"}')
        self.assertEqual(c.parse_board_message(c.TOPIC_STATE, payload).alarm, c.Alarm.LATENT)

    def test_measurements(self):
        payload = (b'{"seq": 128, "temperature": 21.4, "humidite": 45.2,'
                   b' "pression": 1013.2, "gaz": 12}')
        msg = c.parse_board_message(c.TOPIC_MEASUREMENTS, payload)
        self.assertEqual(msg.seq, 128)
        self.assertAlmostEqual(msg.temperature, 21.4)
        self.assertAlmostEqual(msg.humidity, 45.2)
        self.assertAlmostEqual(msg.pressure, 1013.2)
        self.assertIsInstance(msg.gas, float)   # integer JSON accepted as number

    def test_durations(self):
        payload = b'{"depuis": 3600000, "buzzer": 1200, "led_rouge": 1200, "camera": 4500}'
        msg = c.parse_board_message(c.TOPIC_DURATIONS, payload)
        self.assertEqual(msg, c.DurationsMsg(3_600_000, 1200, 1200, 4500))

    def test_event(self):
        payload = b'{"seq": 5, "type": "intrusion", "distance_cm": 8.4}'
        msg = c.parse_board_message(c.TOPIC_EVENT, payload)
        self.assertEqual(msg.type, c.EventType.INTRUSION)
        self.assertAlmostEqual(msg.distance_cm, 8.4)

    def test_ack(self):
        msg = c.parse_board_message(c.TOPIC_ACK, b'{"id": 17, "cmd": "REARMER", "resultat": "ok"}')
        self.assertEqual(msg, c.AckMsg(17, "REARMER", c.AckResult.OK))

    def test_ack_without_valid_id(self):
        msg = c.parse_board_message(c.TOPIC_ACK, b'{"id": 0, "cmd": "", "resultat": "inconnu"}')
        self.assertEqual(msg.id, 0)
        self.assertEqual(msg.result, c.AckResult.UNKNOWN)

    def test_image_announce(self):
        msg = c.parse_board_message(c.TOPIC_IMAGE_ANNOUNCE,
                                    b'{"img": 3, "taille": 7412, "morceaux": 8}')
        self.assertEqual(msg, c.ImageAnnounceMsg(3, 7412, 8))
        # Final assembly: 3 128 to 3 192 bytes -> 4 chunks.
        msg = c.parse_board_message(c.TOPIC_IMAGE_ANNOUNCE,
                                    b'{"img": 1, "taille": 3192, "morceaux": 4}')
        self.assertEqual(msg.chunks, 4)

    def test_image_chunk(self):
        msg = c.parse_board_message("smarthouse/carte/image/3/7", b"\xff" * 244)
        self.assertEqual((msg.image_num, msg.index, len(msg.data)), (3, 7, 244))


class RejectTest(unittest.TestCase):
    """Invalid messages raise ContractError (they are ignored, never stored)."""

    def assertRejected(self, topic, payload):
        with self.assertRaises(c.ContractError):
            c.parse_board_message(topic, payload)

    def test_unknown_topic(self):
        self.assertRejected("smarthouse/carte/autre", b"{}")
        self.assertRejected("smarthouse/serveur/commande", b"REARMER 1")

    def test_not_json(self):
        self.assertRejected(c.TOPIC_STATE, b"latent")
        self.assertRejected(c.TOPIC_STATE, b"\xff\xfe")
        self.assertRejected(c.TOPIC_STATE, b"[1, 2]")

    def test_missing_field(self):
        self.assertRejected(c.TOPIC_STATE, b'{"alarme": "latent", "buzzer": false}')

    def test_wrong_types(self):
        self.assertRejected(c.TOPIC_CONNECTION, b'{"en_ligne": 1}')
        self.assertRejected(c.TOPIC_MEASUREMENTS,
                            b'{"seq": "1", "temperature": 1, "humidite": 1, "pression": 1, "gaz": 1}')
        self.assertRejected(c.TOPIC_MEASUREMENTS,
                            b'{"seq": true, "temperature": 1, "humidite": 1, "pression": 1, "gaz": 1}')
        self.assertRejected(c.TOPIC_MEASUREMENTS,
                            b'{"seq": 1, "temperature": "chaud", "humidite": 1, "pression": 1, "gaz": 1}')

    def test_not_finite(self):
        self.assertRejected(c.TOPIC_MEASUREMENTS,
                            b'{"seq": 1, "temperature": NaN, "humidite": 1, "pression": 1, "gaz": 1}')

    def test_unknown_enum_value(self):
        self.assertRejected(c.TOPIC_STATE,
                            b'{"alarme": "panique", "buzzer": false, "led_rouge": false, "diffusion": false}')
        self.assertRejected(c.TOPIC_ACK, b'{"id": 1, "cmd": "REARMER", "resultat": "peut-etre"}')

    def test_negative_counter(self):
        self.assertRejected(c.TOPIC_DURATIONS,
                            b'{"depuis": -1, "buzzer": 0, "led_rouge": 0, "camera": 0}')

    def test_announce_limits(self):
        self.assertRejected(c.TOPIC_IMAGE_ANNOUNCE, b'{"img": 1, "taille": 0, "morceaux": 0}')
        self.assertRejected(c.TOPIC_IMAGE_ANNOUNCE, b'{"img": 1, "taille": 60000, "morceaux": 59}')
        self.assertRejected(c.TOPIC_IMAGE_ANNOUNCE, b'{"img": 1, "taille": 3192, "morceaux": 5}')
        self.assertRejected(c.TOPIC_IMAGE_ANNOUNCE, b'{"img": 0, "taille": 3192, "morceaux": 4}')

    def test_chunk_limits(self):
        self.assertRejected("smarthouse/carte/image/3/0", b"")
        self.assertRejected("smarthouse/carte/image/3/0", b"x" * 1025)
        self.assertRejected("smarthouse/carte/image/3/59", b"x")
        self.assertRejected("smarthouse/carte/image/0/0", b"x")
        self.assertRejected("smarthouse/carte/image/abc/0", b"x")


class FormatCommandTest(unittest.TestCase):
    def test_words(self):
        self.assertEqual(c.format_command(c.Command.REARM, 3), b"REARMER 3")
        self.assertEqual(c.format_command(c.Command.GET_IMAGES, 12), b"OBTENIR 12")
        self.assertEqual(c.format_command(c.Command.STOP_STREAM, 1), b"ARRETER_DIFFUSION 1")

    def test_invalid(self):
        with self.assertRaises(c.ContractError):
            c.format_command(c.Command.ALERT, 0)
        with self.assertRaises(c.ContractError):
            c.format_command(c.Command.ALERT, 2**32)
        with self.assertRaises(c.ContractError):
            c.format_command("REARMER", 1)


if __name__ == "__main__":
    unittest.main()

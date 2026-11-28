#ifndef PINS_H
#define PINS_H

#define LIMITE_COURANT_BROCHE_MA   8

#define DELAI_DEMARRAGE_SERIE_MS   3000

#define PIN_I2C_SDA                A4
#define PIN_I2C_SCL                A5

#define PIN_SPI_MOSI               11
#define PIN_SPI_MISO               12
#define PIN_SPI_SCK                13

#define ADDR_I2C_BME680            0x77

#define PIN_CAMERA_CS              10
#define ADDR_I2C_CAMERA            0x3C

#define DELAI_AVANT_CAPTURE_MS     1000

#define PIN_ULTRASON_TRIG          7
#define PIN_ULTRASON_ECHO          8

#define ULTRASON_TIMEOUT_US        30000UL
#define ULTRASON_DELAI_MESURE_MS   60
#define VITESSE_SON_CM_US          0.0343f

#define ULTRASON_ARTEFACT_MIN_CM       4.0f
#define ULTRASON_ARTEFACT_MAX_CM       385.0f

#define ULTRASON_MESURES_PAR_CYCLE     5
#define ULTRASON_ECART_MAX_CM          2.0f
#define ULTRASON_SEUIL_DETECTION_CM    11.0f

#define PIN_LED_RGB_ROUGE          3
#define PIN_LED_RGB_VERT           5
#define LED_RGB_ROUGE_OHMS         1000
#define LED_RGB_VERT_OHMS          1000

#define PIN_BUZZER                 2

#endif

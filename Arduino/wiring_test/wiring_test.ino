#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <ArduCAM.h>
#include "memorysaver.h"

#if !(defined OV5642_MINI_5MP_PLUS)
  #error "Decommenter #define OV5642_MINI_5MP_PLUS dans memorysaver.h"
#endif

#define BUZZER_PRESENT  1

const int PIN_CAMERA_CS     = 10;
const int PIN_ULTRASON_TRIG = 7;
const int PIN_ULTRASON_ECHO = 8;

const int PIN_RGB_ROUGE     = 3;
const int PIN_RGB_VERT      = 5;

const int PIN_BUZZER        = 2;

const uint8_t ADDR_I2C_BME680 = 0x77;
const uint8_t ADDR_I2C_CAMERA = 0x3C;

const unsigned long ULTRASON_TIMEOUT_US = 30000UL;
const unsigned long ULTRASON_DELAI_MS   = 60;
const float         VITESSE_SON_CM_US   = 0.0343f;
const uint8_t       ULTRASON_MESURES    = 5;

const float ULTRASON_ARTEFACT_MIN_CM = 4.0f;
const float ULTRASON_ARTEFACT_MAX_CM = 385.0f;

Adafruit_BME680 bme;
ArduCAM myCAM(OV5642, PIN_CAMERA_CS);

unsigned int cycle = 0;
bool bmePresent = false;
bool camPresente = false;

void voyantVert() {
  analogWrite(PIN_RGB_ROUGE, 0);
  analogWrite(PIN_RGB_VERT, 255);
}

void voyantRouge() {
  analogWrite(PIN_RGB_ROUGE, 255);
  analogWrite(PIN_RGB_VERT, 0);
}

void voyantEteint() {
  analogWrite(PIN_RGB_ROUGE, 0);
  analogWrite(PIN_RGB_VERT, 0);
}

void scanI2C() {
  Serial.println(F("  adresses presentes sur le bus :"));
  byte trouves = 0;

  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("    0x"));
      if (a < 16) Serial.print('0');
      Serial.print(a, HEX);
      if (a == ADDR_I2C_BME680) Serial.print(F("  <- BME680"));
      if (a == ADDR_I2C_CAMERA) Serial.print(F("  <- capteur de la camera"));
      Serial.println();
      trouves++;
    }
  }

  if (trouves < 2) {
    Serial.println(F("  ATTENTION : moins de 2 peripheriques I2C."));
  }
}

void trier(float t[], uint8_t n) {
  for (uint8_t i = 1; i < n; i++) {
    float v = t[i];
    int8_t j = i - 1;
    while (j >= 0 && t[j] > v) {
      t[j + 1] = t[j];
      j--;
    }
    t[j + 1] = v;
  }
}

unsigned long mesurerDureeUs() {
  digitalWrite(PIN_ULTRASON_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_ULTRASON_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_ULTRASON_TRIG, LOW);
  return pulseIn(PIN_ULTRASON_ECHO, HIGH, ULTRASON_TIMEOUT_US);
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println();
  Serial.println(F("########################################"));
  Serial.println(F("#  Test de cablage - montage final     #"));
  Serial.println(F("#  Cette banniere ne doit apparaitre   #"));
  Serial.println(F("#  QU'UNE SEULE FOIS.                  #"));
  Serial.println(F("########################################"));
  Serial.println();

  pinMode(PIN_ULTRASON_TRIG, OUTPUT);
  pinMode(PIN_ULTRASON_ECHO, INPUT);
  digitalWrite(PIN_ULTRASON_TRIG, LOW);

  pinMode(PIN_RGB_ROUGE, OUTPUT);
  pinMode(PIN_RGB_VERT,  OUTPUT);
  voyantEteint();

#if BUZZER_PRESENT
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
#endif

  pinMode(PIN_CAMERA_CS, OUTPUT);
  digitalWrite(PIN_CAMERA_CS, HIGH);

  Wire.begin();
  SPI.begin();

  Serial.println(F("[init] Bus I2C"));
  scanI2C();

  Serial.println(F("[init] BME680"));
  bmePresent = bme.begin(ADDR_I2C_BME680);
  if (bmePresent) {
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);
    Serial.println(F("  OK"));
  } else {
    Serial.println(F("  ECHEC : non detecte a 0x77"));
  }

  Serial.println(F("[init] Camera"));
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  delay(10);
  if (myCAM.read_reg(ARDUCHIP_TEST1) == 0x55) {
    myCAM.set_format(JPEG);
    myCAM.InitCAM();
    myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
    myCAM.OV5642_set_JPEG_size(OV5642_320x240);
    delay(1000);
    myCAM.clear_fifo_flag();
    camPresente = true;
    Serial.println(F("  OK"));
  } else {
    Serial.println(F("  ECHEC : le bus SPI ne repond pas"));
  }

  Serial.print(F("\n[init] Buzzer : "));
#if BUZZER_PRESENT
  Serial.println(F("actif, via L293D sur D2 (patte 2 = 1A)"));
#else
  Serial.println(F("DESACTIVE (BUZZER_PRESENT = 0)"));
#endif

  Serial.println();
  delay(2000);
}

void loop() {
  cycle++;

  Serial.print(F("===== Cycle "));
  Serial.print(cycle);
  Serial.println(F(" ====="));

  Serial.print(F("[BME680 ] "));
  if (bmePresent && bme.performReading()) {
    Serial.print(bme.temperature, 1);
    Serial.print(F(" C   "));
    Serial.print(bme.humidity, 1);
    Serial.print(F(" %HR   "));
    Serial.print(bme.pressure / 100.0, 1);
    Serial.print(F(" hPa   "));
    Serial.print(bme.gas_resistance / 1000.0, 1);
    Serial.println(F(" kOhm"));
  } else {
    Serial.println(F("echec de lecture"));
  }

  Serial.print(F("[HC-SR04] "));

  float retenues[ULTRASON_MESURES];
  uint8_t nRetenues = 0;
  uint8_t nArtefacts = 0;
  uint8_t nSansEcho  = 0;

  for (uint8_t i = 0; i < ULTRASON_MESURES; i++) {
    unsigned long d = mesurerDureeUs();

    if (d == 0) {
      nSansEcho++;
    } else {
      float cm = d * VITESSE_SON_CM_US / 2.0;
      if (cm < ULTRASON_ARTEFACT_MIN_CM || cm > ULTRASON_ARTEFACT_MAX_CM) {
        nArtefacts++;
      } else {
        retenues[nRetenues] = cm;
        nRetenues++;
      }
    }
    delay(ULTRASON_DELAI_MS);
  }

  if (nRetenues == 0) {
    Serial.print(F("aucune mesure retenue   ("));
    Serial.print(nSansEcho);
    Serial.print(F(" sans echo, "));
    Serial.print(nArtefacts);
    Serial.println(F(" butee(s))"));
  } else {
    trier(retenues, nRetenues);
    float med  = retenues[nRetenues / 2];
    float mini = retenues[0];
    float maxi = retenues[nRetenues - 1];

    Serial.print(med, 1);
    Serial.print(F(" cm   ecart "));
    Serial.print(maxi - mini, 1);
    Serial.print(F(" cm   retenues "));
    Serial.print(nRetenues);
    Serial.print(F("/"));
    Serial.print(ULTRASON_MESURES);

    if (nArtefacts > 0) {
      Serial.print(F("   "));
      Serial.print(nArtefacts);
      Serial.print(F(" butee(s) ecartee(s)"));
    }
    if (nSansEcho > 0) {
      Serial.print(F("   "));
      Serial.print(nSansEcho);
      Serial.print(F(" sans echo"));
    }
    if (nRetenues < 3) {
      Serial.print(F("   ecart non significatif"));
    }
    Serial.println();
  }

  Serial.print(F("[Camera ] "));
  if (camPresente) {
    myCAM.flush_fifo();
    myCAM.clear_fifo_flag();
    myCAM.start_capture();

    unsigned long debut = millis();
    bool finie = false;
    while (millis() - debut < 5000) {
      if (myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
        finie = true;
        break;
      }
    }

    if (finie) {
      Serial.print(myCAM.read_fifo_length());
      Serial.println(F(" octets dans le tampon"));
    } else {
      Serial.println(F("ECHEC : capture non terminee"));
    }
    myCAM.clear_fifo_flag();
  } else {
    Serial.println(F("absente"));
  }

  Serial.println(F("[LED RGB] vert (normal), puis rouge (detection)"));

  voyantVert();    delay(1000);
  voyantRouge();   delay(1000);
  voyantEteint();
  delay(300);

#if BUZZER_PRESENT

  Serial.print(F("[Buzzer ] deux bips courts... "));
  for (uint8_t i = 0; i < 2; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delay(150);
    digitalWrite(PIN_BUZZER, LOW);
    delay(250);
  }
  Serial.println(F("fait"));
#endif

  Serial.print(F("Cycle "));
  Serial.print(cycle);
  Serial.println(F(" termine sans redemarrage.\n"));

  delay(3000);
}

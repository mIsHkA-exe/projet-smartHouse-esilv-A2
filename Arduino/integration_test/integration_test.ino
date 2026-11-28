#include <Wire.h>
#include <SPI.h>
#include <Servo.h>
#include <WiFiS3.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <ArduCAM.h>
#include "memorysaver.h"

#if !(defined OV5642_MINI_5MP_PLUS)
  #error "Decommenter #define OV5642_MINI_5MP_PLUS dans memorysaver.h"
#endif

const int PIN_CAM_CS   = 10;
const int PIN_TRIG     = 7;
const int PIN_ECHO     = 8;
const int PIN_SERVO    = 9;

const uint8_t ADDR_BME680 = 0x77;

const int SERVO_US_MIN = 900;
const int SERVO_US_MAX = 2100;
const int SERVO_CENTRE = 90;
const int SERVO_EXCURSION = 30;

const unsigned long ECHO_TIMEOUT_US = 30000UL;

Adafruit_BME680 bme;
ArduCAM myCAM(OV5642, PIN_CAM_CS);
Servo servo;

unsigned int cycle = 0;
bool bmePresent = false;
bool camPresente = false;

void scanI2C() {
  Serial.println(F("  adresses presentes sur le bus :"));
  byte trouves = 0;

  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("    0x"));
      if (a < 16) Serial.print('0');
      Serial.print(a, HEX);
      if (a == 0x77) Serial.print(F("  <- BME680"));
      if (a == 0x3C) Serial.print(F("  <- capteur de la camera"));
      Serial.println();
      trouves++;
    }
  }

  if (trouves < 2) {
    Serial.println(F("  ATTENTION : moins de 2 peripheriques I2C."));
    Serial.println(F("  Un des deux modules ne repond pas."));
  }
}

float mesurerDistanceCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long duree = pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US);
  if (duree == 0) return -1.0;
  return duree * 0.0343 / 2.0;
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println();
  Serial.println(F("########################################"));
  Serial.println(F("#  DEMARRAGE - Montage partiel         #"));
  Serial.println(F("#  Cette banniere ne doit apparaitre   #"));
  Serial.println(F("#  QU'UNE SEULE FOIS.                  #"));
  Serial.println(F("########################################"));
  Serial.println();

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);

  pinMode(PIN_CAM_CS, OUTPUT);
  digitalWrite(PIN_CAM_CS, HIGH);

  Wire.begin();
  SPI.begin();

  Serial.println(F("[init] Bus I2C"));
  scanI2C();

  Serial.println(F("[init] BME680"));
  bmePresent = bme.begin(ADDR_BME680);
  if (bmePresent) {
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);
    Serial.println(F("  OK"));
  } else {
    Serial.println(F("  ECHEC : non detecte"));
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

  Serial.println(F("[init] Module Wi-Fi"));
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("  ECHEC : module absent"));
  } else {
    Serial.print(F("  OK, version du micrologiciel : "));
    Serial.println(WiFi.firmwareVersion());
  }

  Serial.println(F("\nAucun identifiant Wi-Fi n'est utilise : scan seul.\n"));
  delay(2000);
}

void loop() {
  cycle++;

  Serial.print(F("===== Cycle "));
  Serial.print(cycle);
  Serial.println(F(" ====="));

  Serial.println(F("[1] Capteurs au repos"));
  if (bmePresent && bme.performReading()) {
    Serial.print(F("    BME680  : "));
    Serial.print(bme.temperature, 1);
    Serial.print(F(" C   "));
    Serial.print(bme.humidity, 1);
    Serial.print(F(" %HR   "));
    Serial.print(bme.pressure / 100.0, 1);
    Serial.println(F(" hPa"));
  }

  float d = mesurerDistanceCm();
  Serial.print(F("    HC-SR04 : "));
  if (d < 0) {
    Serial.println(F("aucun echo"));
  } else {
    Serial.print(d, 1);
    Serial.println(F(" cm"));
  }

  Serial.println(F("[2] Capture camera (environ 300 mA)"));
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
      Serial.print(F("    image de "));
      Serial.print(myCAM.read_fifo_length());
      Serial.println(F(" octets dans le tampon"));
    } else {
      Serial.println(F("    ECHEC : capture non terminee"));
    }
    myCAM.clear_fifo_flag();
  }
  delay(500);

  Serial.println(F("[3] Servo seul"));
  servo.attach(PIN_SERVO, SERVO_US_MIN, SERVO_US_MAX);
  servo.write(SERVO_CENTRE - SERVO_EXCURSION);
  delay(700);
  servo.write(SERVO_CENTRE + SERVO_EXCURSION);
  delay(700);
  servo.write(SERVO_CENTRE);
  delay(700);
  servo.detach();
  Serial.println(F("    deplacement effectue, servo detache"));
  delay(500);

  Serial.println(F("[4] Scan Wi-Fi seul (pics d'emission)"));
  int n = WiFi.scanNetworks();
  if (n < 0) {
    Serial.println(F("    ECHEC du scan"));
  } else {
    Serial.print(F("    "));
    Serial.print(n);
    Serial.println(F(" reseaux detectes"));
  }
  delay(500);

  Serial.println(F("[5] Servo + Wi-Fi SIMULTANES - pire cas"));
  servo.attach(PIN_SERVO, SERVO_US_MIN, SERVO_US_MAX);
  servo.write(SERVO_CENTRE - SERVO_EXCURSION);
  n = WiFi.scanNetworks();
  servo.write(SERVO_CENTRE + SERVO_EXCURSION);
  n = WiFi.scanNetworks();
  servo.write(SERVO_CENTRE);
  delay(700);
  servo.detach();
  Serial.println(F("    sequence simultanee terminee"));

  Serial.print(F("Cycle "));
  Serial.print(cycle);
  Serial.println(F(" termine sans redemarrage.\n"));

  delay(3000);
}

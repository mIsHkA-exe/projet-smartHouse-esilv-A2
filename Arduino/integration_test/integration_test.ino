/*
  integration_test — Smart House
  Chemin dans le depot : Arduino/integration_test/integration_test.ino
  (le dossier doit porter le meme nom que le fichier .ino)

  Objet  : montage partiel des quatre composants deja valides, branches
           ENSEMBLE, pour verifier ce qu'aucun test unitaire ne pouvait
           verifier :
             - la cohabitation du BME680 et de la camera sur le bus I2C ;
             - le budget de courant avec tous les consommateurs presents ;
             - le comportement pendant une emission Wi-Fi ;
             - le sequencement camera -> servo -> emission (D16).

  Composants : BME680, camera Arducam, HC-SR04, servo FS90.
  NON inclus : buzzer, LED rouge, LED RGB (composants manquants).

  Cablage complet :
    BME680   VIN->5V  GND->GND  SDI->A4  SCK->A5
    Camera   CS->D10  MOSI->D11  MISO->D12  SCK->D13
             GND->GND  VCC->5V  SDA->A4  SCL->A5
    HC-SR04  VCC->5V  TRIG->D7  ECHO->D8  GND->GND
    Servo    signal->D9  rouge->5V  marron->GND   (palonnier LIBRE)

  Prealable : correctifs Renesas dans la bibliotheque ArduCAM, et
              #define OV5642_MINI_5MP_PLUS seul decommente.

  Statut : test, pas le firmware final. AUCUN IDENTIFIANT REEL ICI.

  ---------------------------------------------------------------------
  WI-FI SANS IDENTIFIANT
  ---------------------------------------------------------------------
  Le croquis n'utilise ni SSID ni mot de passe. Il appelle un SCAN des
  reseaux : cela allume le module ESP32-S3 et emet des trames de
  sondage, donc produit le pic de courant d'emission (340 mA annonces
  par Espressif), sans aucune connexion ni identifiant.
  C'est un substitut a un envoi MQTT soutenu, pas son equivalent exact.

  ---------------------------------------------------------------------
  DETECTION D'UNE CHUTE D'ALIMENTATION, SANS MULTIMETRE
  ---------------------------------------------------------------------
  Banniere au demarrage + compteur de cycles. Si la banniere reapparait
  ou si le compteur repart a 1, la carte a redemarre : l'alimentation
  n'a pas tenu. La phase affichee juste avant designe la cause.
*/

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

// ----------------------------------------------------------- brochage
const int PIN_CAM_CS   = 10;
const int PIN_TRIG     = 7;
const int PIN_ECHO     = 8;
const int PIN_SERVO    = 9;

const uint8_t ADDR_BME680 = 0x77;

// Plage d'impulsion du FS90 (fiche FEETECH). Indispensable : sans elle
// la bibliotheque utilise 544-2400 us et pousse le servo en butee.
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

// ------------------------------------------------------------ scan I2C
// Doit montrer DEUX adresses : 0x3C (capteur de la camera) et 0x77
// (BME680). C'est le controle de cohabitation sur le bus partage.
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

// -------------------------------------------------------- mesure HC-SR04
// Renvoie la distance en cm, ou -1 si aucun echo.
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

// --------------------------------------------------------------- setup
void setup() {
  Serial.begin(115200);
  delay(3000);  // USB natif de la R4 : laisse le moniteur se reconnecter

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

  // --- Bus I2C partage ---------------------------------------------
  Serial.println(F("[init] Bus I2C"));
  scanI2C();

  // --- BME680 --------------------------------------------------------
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

  // --- Camera ---------------------------------------------------------
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

  // --- Module Wi-Fi ---------------------------------------------------
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

// ---------------------------------------------------------------- loop
void loop() {
  cycle++;

  Serial.print(F("===== Cycle "));
  Serial.print(cycle);
  Serial.println(F(" ====="));

  // --- Phase 1 : lecture des capteurs, courant faible ------------------
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

  // --- Phase 2 : capture camera seule, environ 300 mA ------------------
  // Capture dans le tampon de la camera, sans transfert : le but est le
  // courant, pas l'image.
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

  // --- Phase 3 : servo seul (D16 : jamais en meme temps que la camera) -
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

  // --- Phase 4 : emission Wi-Fi seule, pics de 340 mA ------------------
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

  // --- Phase 5 : servo ET emission Wi-Fi SIMULTANES --------------------
  // C'est le pire cas autorise : D16 interdit servo + camera, mais rien
  // n'interdit servo + emission. servo.write() rend la main aussitot,
  // le servo continue a se deplacer pendant le scan.
  Serial.println(F("[5] Servo + Wi-Fi SIMULTANES - pire cas"));
  servo.attach(PIN_SERVO, SERVO_US_MIN, SERVO_US_MAX);
  servo.write(SERVO_CENTRE - SERVO_EXCURSION);
  n = WiFi.scanNetworks();            // le servo se deplace pendant ce scan
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

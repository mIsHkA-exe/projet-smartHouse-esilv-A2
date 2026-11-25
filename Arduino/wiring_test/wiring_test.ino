/*
  wiring_test — Smart House
  Chemin dans le depot : Arduino/wiring_test/wiring_test.ino
  (le dossier doit porter le meme nom que le fichier .ino)

  Objet  : verification du CABLAGE du montage final IoT. Ce croquis ne
           contient aucune logique de fonctionnement : il controle que
           chaque composant repond a sa place, et que l'alimentation
           tient. La logique releve de NetSec (D28).

  Composants : BME680, camera Arducam, HC-SR04, LED rouge.
  Buzzer     : ABSENT tant que le transistor manque (voir plus bas).
  Retires du projet : servomoteur (D32), LED RGB (22/11).

  Cablage complet du montage final :
    BME680     VIN -> 5V   GND -> GND   SDI -> A4   SCK -> A5
    Camera     CS  -> D10  MOSI -> D11  MISO -> D12  SCK -> D13
               SDA -> A4   SCL  -> A5   VCC  -> 5V   GND -> GND
    HC-SR04    VCC -> 5V   TRIG -> D7   ECHO -> D8   GND -> GND
    LED rouge  anode -> resistance 1 kOhm -> D4   cathode -> GND

  Alimentation : port USB-C du PC. La cellule solaire n'est PAS
  raccordee (voir pins.h § 1 pour le dimensionnement qui le justifie).

  Prealable : correctifs Renesas dans la bibliotheque ArduCAM, et
              #define OV5642_MINI_5MP_PLUS seul decommente.

  Statut : test de cablage. AUCUN IDENTIFIANT REEL ICI.

  ---------------------------------------------------------------------
  Version : v2 — 25/11/2026
    - CORRECTION signalee par NetSec : rejet des lectures de butee du
      HC-SR04 AVANT le calcul de mediane et d'ecart. La v1 laissait
      passer la butee basse de 3,7 cm, dont l'ecart vaut 0,0 cm : elle
      aurait declenche une FAUSSE INTRUSION dans le firmware final.
  Version : v1 — 22/11/2026
  ---------------------------------------------------------------------

  ---------------------------------------------------------------------
  BUZZER — LIRE AVANT DE PASSER BUZZER_PRESENT A 1
  ---------------------------------------------------------------------
  Le buzzer consomme jusqu'a 30 mA. La limite d'une broche de la UNO R4
  WiFi est de 8 mA, contre 40 mA sur une Uno R3 : un montage direct,
  courant sur R3, N'EST PAS valable ici.

  Ne passer BUZZER_PRESENT a 1 QU'APRES avoir monte l'etage a
  transistor decrit dans pins.h. Le transistor est la seule piece
  encore manquante : la resistance de 1 kOhm est disponible.
  ---------------------------------------------------------------------
*/

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <ArduCAM.h>
#include "memorysaver.h"

#if !(defined OV5642_MINI_5MP_PLUS)
  #error "Decommenter #define OV5642_MINI_5MP_PLUS dans memorysaver.h"
#endif

/* Passer a 1 UNIQUEMENT une fois l'etage a transistor monte. */
#define BUZZER_PRESENT  0

/* ----------------------------------------------------------- brochage
   Ces valeurs recopient Arduino/smart_house/pins.h v4, qui reste LA
   reference. L'IDE Arduino ne sait pas inclure un fichier d'un autre
   dossier de croquis : la duplication est subie, pas choisie. Toute
   divergence avec pins.h est une erreur a corriger ici. */
const int PIN_CAMERA_CS     = 10;
const int PIN_ULTRASON_TRIG = 7;
const int PIN_ULTRASON_ECHO = 8;
const int PIN_LED_ROUGE     = 4;
#if BUZZER_PRESENT
const int PIN_BUZZER        = 2;
#endif

const uint8_t ADDR_I2C_BME680 = 0x77;
const uint8_t ADDR_I2C_CAMERA = 0x3C;

const unsigned long ULTRASON_TIMEOUT_US = 30000UL;
const unsigned long ULTRASON_DELAI_MS   = 60;
const float         VITESSE_SON_CM_US   = 0.0343f;
const uint8_t       ULTRASON_MESURES    = 5;

/* Butees du capteur : ce ne sont PAS des mesures (pins.h v4 § 3). */
const float ULTRASON_ARTEFACT_MIN_CM = 4.0f;
const float ULTRASON_ARTEFACT_MAX_CM = 385.0f;

Adafruit_BME680 bme;
ArduCAM myCAM(OV5642, PIN_CAMERA_CS);

unsigned int cycle = 0;
bool bmePresent = false;
bool camPresente = false;

/* ------------------------------------------------------------ scan I2C
   Doit montrer DEUX adresses : 0x3C (camera) et 0x77 (BME680). */
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

/* ------------------------------------------------------- tri croissant */
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

/* ------------------------------------------------- mesure d'une duree */
unsigned long mesurerDureeUs() {
  digitalWrite(PIN_ULTRASON_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_ULTRASON_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_ULTRASON_TRIG, LOW);
  return pulseIn(PIN_ULTRASON_ECHO, HIGH, ULTRASON_TIMEOUT_US);
}

/* --------------------------------------------------------------- setup */
void setup() {
  Serial.begin(115200);
  delay(3000);  /* USB natif de la R4 : jamais de while (!Serial) */

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

  pinMode(PIN_LED_ROUGE, OUTPUT);
  digitalWrite(PIN_LED_ROUGE, LOW);

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
  Serial.println(F("present, etage a transistor suppose monte"));
#else
  Serial.println(F("ABSENT (transistor manquant) - D2 reservee"));
#endif

  Serial.println();
  delay(2000);
}

/* ---------------------------------------------------------------- loop */
void loop() {
  cycle++;

  Serial.print(F("===== Cycle "));
  Serial.print(cycle);
  Serial.println(F(" ====="));

  /* --- BME680 --------------------------------------------------------- */
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

  /* --- HC-SR04 -------------------------------------------------------- */
  /* ORDRE IMPOSE (pins.h v4 § 3) :
       1. ecarter les lectures de butee ;
       2. mediane et ecart sur ce qui reste ;
       3. zero lecture retenue = absence de mesure, jamais une
          detection.
     Sans l'etape 1, la butee basse de 3,7 cm, qui se repete a
     l'identique et dont l'ecart vaut donc 0,0 cm, passerait le filtre
     d'ecart et tomberait sous le seuil de detection. */
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
        nArtefacts++;              /* butee : ce n'est pas une mesure */
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

  /* --- Camera : capture dans son tampon, sans transfert ---------------- */
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

  /* --- LED rouge : trois clignotements --------------------------------- */
  /* Verification de cablage seulement. Si elle reste eteinte, verifier
     le sens de la LED (patte longue = anode, cote resistance). */
  Serial.print(F("[LED    ] trois clignotements... "));
  for (uint8_t i = 0; i < 3; i++) {
    digitalWrite(PIN_LED_ROUGE, HIGH);
    delay(200);
    digitalWrite(PIN_LED_ROUGE, LOW);
    delay(200);
  }
  Serial.println(F("fait"));

  /* --- Buzzer : seulement si l'etage a transistor est monte ------------ */
#if BUZZER_PRESENT
  /* D16 : le buzzer et la camera ne sont jamais actifs en meme temps.
     La capture est terminee ici, donc l'emission sonore est permise. */
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

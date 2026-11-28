/*
  wiring_test — Smart House
  Chemin dans le depot : Arduino/wiring_test/wiring_test.ino
  (le dossier doit porter le meme nom que le fichier .ino)

  Objet  : verification du CABLAGE du montage final IoT. Ce croquis ne
           contient aucune logique de fonctionnement : il controle que
           chaque composant repond a sa place, et que l'alimentation
           tient. La logique releve de NetSec (D28).

  Composants : BME680, camera Arducam, HC-SR04, LED RGB (voyant d'etat,
               D45), buzzer actif pilote par un L293D.
  Retires du projet : servomoteur (D32), LED rouge simple.
  Canal BLEU de la LED RGB : NON CABLE (D45). D4 et D6 sont libres.

  Alimentation : port USB-C du PC. La cellule solaire n'est PAS
  raccordee (voir pins.h § 1).

  Prealable : correctifs Renesas dans la bibliotheque ArduCAM, et
              #define OV5642_MINI_5MP_PLUS seul decommente.

  Statut : test de cablage. AUCUN IDENTIFIANT REEL ICI.

  ---------------------------------------------------------------------
  CABLAGE COMPLET
  ---------------------------------------------------------------------
    BME680     VIN -> 5V   GND -> GND   SDI -> A4   SCK -> A5
               (SDO, CS et 3Vo NON connectees : mode I2C, 0x77)

    Camera     CS  -> D10  MOSI -> D11  MISO -> D12  SCK -> D13
               SDA -> A4   SCL  -> A5   VCC  -> 5V   GND -> GND

    HC-SR04    VCC -> 5V   TRIG -> D7   ECHO -> D8   GND -> GND

    LED RGB    rouge -> 1 kOhm -> D3
    (cathode   vert  -> 1 kOhm -> D5
     commune)  commune -> GND
               bleu : NON CABLE

    L293D      patte 16  VCC1       -> 5V
    (patte 1   patte 8   VCC2       -> 5V
     reperee   pattes 4, 5, 12, 13  -> GND   (LES QUATRE)
     par une   patte 1   1,2EN      -> 5V
     encoche)  patte 2   1A         -> D2
               patte 3   1Y         -> buzzer +
               buzzer -             -> GND

  Le boitier du L293D doit ENJAMBER le canal central de la breadboard,
  sinon ses deux rangees de pattes sont court-circuitees.

  ---------------------------------------------------------------------
  Version : v5 — 28/11/2026
    - D45 : la LED RGB est un VOYANT D'ETAT, vert en fonctionnement
      normal et rouge a la detection. Le bleu n'est plus cable : la
      sequence de test ne montre plus ni bleu ni orange.
  Version : v4 — 28/11/2026 (buzzer active via L293D)
  Version : v3 — 28/11/2026 (LED RGB a la place de la LED rouge)
  Version : v2 — 25/11/2026 (rejet des butees du HC-SR04)
  Version : v1 — 22/11/2026
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

/* Mettre a 0 pour tester le reste du montage sans le buzzer.
   A 1, le croquis suppose le L293D cable comme decrit ci-dessus.
   NE JAMAIS brancher le buzzer directement sur D2 : 30 mA contre une
   limite de 8 mA par broche sur la UNO R4 WiFi. */
#define BUZZER_PRESENT  1

/* ----------------------------------------------------------- brochage
   Ces valeurs recopient Arduino/smart_house/pins.h v6, qui reste LA
   reference. L'IDE Arduino ne sait pas inclure un fichier d'un autre
   dossier de croquis : la duplication est subie, pas choisie. */
const int PIN_CAMERA_CS     = 10;
const int PIN_ULTRASON_TRIG = 7;
const int PIN_ULTRASON_ECHO = 8;

const int PIN_RGB_ROUGE     = 3;   /* PWM */
const int PIN_RGB_VERT      = 5;   /* PWM */
/* Pas de PIN_RGB_BLEU : le canal bleu n'est pas cable (D45). */

const int PIN_BUZZER        = 2;   /* -> patte 2 (1A) du L293D */

const uint8_t ADDR_I2C_BME680 = 0x77;
const uint8_t ADDR_I2C_CAMERA = 0x3C;

const unsigned long ULTRASON_TIMEOUT_US = 30000UL;
const unsigned long ULTRASON_DELAI_MS   = 60;
const float         VITESSE_SON_CM_US   = 0.0343f;
const uint8_t       ULTRASON_MESURES    = 5;

/* Butees du capteur : ce ne sont PAS des mesures. Quand il perd sa
   cible, le HC-SR04 renvoie soit 3,7 cm, soit 386 a 393 cm. */
const float ULTRASON_ARTEFACT_MIN_CM = 4.0f;
const float ULTRASON_ARTEFACT_MAX_CM = 385.0f;

Adafruit_BME680 bme;
ArduCAM myCAM(OV5642, PIN_CAMERA_CS);

unsigned int cycle = 0;
bool bmePresent = false;
bool camPresente = false;

/* ------------------------------------------- LED RGB, voyant d'etat
   Cathode commune : une valeur HAUTE allume la couleur.
   Sur le coeur Renesas, une digitalWrite() apres une analogWrite() sur
   la meme broche peut rester sans effet : on n'utilise QUE
   analogWrite() sur ces deux broches. */
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

  pinMode(PIN_RGB_ROUGE, OUTPUT);
  pinMode(PIN_RGB_VERT,  OUTPUT);
  voyantEteint();

#if BUZZER_PRESENT
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);   /* L293D au repos : buzzer silencieux */
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

/* ---------------------------------------------------------------- loop */
void loop() {
  cycle++;

  Serial.print(F("===== Cycle "));
  Serial.print(cycle);
  Serial.println(F(" ====="));

  /* --- BME680 --------------------------------------------------------- */
  /* Affichage en hPa et kOhm pour la lisibilite du test. Le firmware
     final transmettra les valeurs BRUTES en Pa et en Ohm, sans
     conversion sur la carte (contrat NetSec v0.9). */
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
  /* ORDRE IMPOSE (C20) :
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

  /* --- LED RGB : les deux etats du voyant (D45) ------------------------ */
  /* Regarde la LED. Elle doit passer au VERT, puis au ROUGE.
     Si une couleur manque, verifier sa resistance de 1 kOhm et le sens
     de la LED (la patte la plus longue est la cathode commune, vers
     GND, sur la plupart des modeles a cathode commune). */
  Serial.println(F("[LED RGB] vert (normal), puis rouge (detection)"));

  voyantVert();    delay(1000);
  voyantRouge();   delay(1000);
  voyantEteint();
  delay(300);

  /* --- Buzzer, via le L293D ------------------------------------------- */
#if BUZZER_PRESENT
  /* D16 : le buzzer et la camera ne sont jamais actifs en meme temps.
     La capture est terminee ici, donc l'emission sonore est permise.
     Signal NON inverse : HIGH sur D2 -> 1A haut -> 1Y haut -> buzzer. */
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

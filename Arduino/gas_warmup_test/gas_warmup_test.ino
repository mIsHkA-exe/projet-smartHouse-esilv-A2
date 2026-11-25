/*
  gas_warmup_test — Smart House
  Chemin dans le depot : Arduino/gas_warmup_test/gas_warmup_test.ino
  (le dossier doit porter le meme nom que le fichier .ino)

  Objet  : repondre a deux demandes de NetSec (25/11/2026) en une seule
           manipulation de 20 minutes, entierement automatique.

    PHASE A (0 a 10 min) — chauffage ACTIF, releve toutes les 15 s.
      Fixe la duree de chauffe de la resistance de gaz apres un
      demarrage a froid.

    PHASE B (10 a 20 min) — chauffage COUPE, releve toutes les 15 s.
      Mesure le DECALAGE DE TEMPERATURE du a l'auto-echauffement, sans
      thermometre de reference : le capteur sert de sa propre
      reference, une fois refroidi.

  Cablage: BME680 seul. VIN -> 5V, GND -> GND, SDI -> A4, SCK -> A5.
           Camera, HC-SR04 et LED debranches : ils chaufferaient la
           breadboard et fausseraient la mesure de temperature.

  Statut : mesure de caracterisation. AUCUN IDENTIFIANT REEL ICI.

  ---------------------------------------------------------------------
  CONDITIONS A RESPECTER, SINON LA MESURE NE VAUT RIEN
  ---------------------------------------------------------------------
  1. DEMARRAGE A FROID : la carte doit etre DEBRANCHEE depuis au moins
     30 minutes. Le capteur de gaz met longtemps a refroidir, et un
     redemarrage « a chaud » donnerait une courbe tronquee.
  2. Ne pas toucher le capteur, ne pas souffler dessus, ne pas le
     deplacer pendant les 20 minutes.
  3. Piece a temperature stable : pas de fenetre ouverte, pas de
     radiateur qui se declenche, pas de passage repete.
  4. Noter l'heure de depart et la temperature de la piece si tu
     disposes d'un thermometre, meme approximatif.

  SORTIE : lignes au format CSV, directement exploitables par NetSec.
    t_s;phase;temp_C;hum_pct;press_Pa;gas_Ohm
  ---------------------------------------------------------------------
*/

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

#define ADDR_I2C_BME680   0x77

/* Cadence et durees demandees par NetSec. */
const unsigned long PERIODE_MS      = 15000UL;        /* 15 s           */
const unsigned long DUREE_PHASE_MS  = 600000UL;       /* 10 min         */

/* Reglages du chauffage, identiques a ceux des autres croquis pour que
   les releves soient comparables. */
const uint16_t CHAUFFE_TEMP_C  = 320;
const uint16_t CHAUFFE_DUREE_MS = 150;

Adafruit_BME680 bme;

unsigned long t0 = 0;
unsigned long prochainReleve = 0;
bool phaseB = false;

/* Moyennes de fin de phase, pour le resume final. Les 2 dernieres
   minutes de chaque phase, soit 8 releves, servent de valeur stabilisee. */
float sommeTempA = 0;  uint8_t nTempA = 0;
float sommeTempB = 0;  uint8_t nTempB = 0;
float dernierGaz = 0;

/* --------------------------------------------------------------- setup */
void setup() {
  Serial.begin(115200);
  delay(3000);  /* USB natif de la R4 : jamais de while (!Serial) */

  Serial.println();
  Serial.println(F("=== Chauffe du BME680 et decalage de temperature ==="));
  Serial.println(F("Duree totale : 20 minutes, sans intervention."));
  Serial.println();

  Wire.begin();

  if (!bme.begin(ADDR_I2C_BME680)) {
    Serial.println(F("ECHEC : BME680 non detecte a 0x77. Test arrete."));
    while (1) {
      delay(1000);
    }
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);

  /* --- PHASE A : chauffage actif ------------------------------------ */
  bme.setGasHeater(CHAUFFE_TEMP_C, CHAUFFE_DUREE_MS);

  Serial.println(F("PHASE A : chauffage ACTIF pendant 10 minutes."));
  Serial.println(F("Ne touchez pas au capteur.\n"));

  /* En-tete CSV */
  Serial.println(F("t_s;phase;temp_C;hum_pct;press_Pa;gas_Ohm"));

  t0 = millis();
  prochainReleve = t0;
}

/* ---------------------------------------------------------------- loop */
void loop() {
  unsigned long maintenant = millis();

  /* Bascule en phase B a 10 minutes : on COUPE le chauffage. */
  if (!phaseB && (maintenant - t0) >= DUREE_PHASE_MS) {
    phaseB = true;
    bme.setGasHeater(0, 0);          /* 0, 0 = chauffage desactive */
    Serial.println();
    Serial.println(F("# --- PHASE B : chauffage COUPE ---"));
    Serial.println(F("# La temperature va redescendre vers l'ambiante."));
    Serial.println(F("# La colonne gas_Ohm n'a plus de sens ici."));
    Serial.println();
  }

  /* Fin du test a 20 minutes. */
  if ((maintenant - t0) >= (2 * DUREE_PHASE_MS)) {
    resume();
    while (1) {
      delay(1000);
    }
  }

  if ((long)(maintenant - prochainReleve) < 0) {
    return;
  }
  prochainReleve += PERIODE_MS;

  if (!bme.performReading()) {
    Serial.print((maintenant - t0) / 1000);
    Serial.println(F(";?;echec;echec;echec;echec"));
    return;
  }

  unsigned long t_s = (maintenant - t0) / 1000;

  Serial.print(t_s);
  Serial.print(F(";"));
  Serial.print(phaseB ? 'B' : 'A');
  Serial.print(F(";"));
  Serial.print(bme.temperature, 2);
  Serial.print(F(";"));
  Serial.print(bme.humidity, 2);
  Serial.print(F(";"));
  Serial.print(bme.pressure, 0);
  Serial.print(F(";"));
  Serial.println(bme.gas_resistance, 0);

  /* Moyennes sur les 2 dernieres minutes de chaque phase. */
  unsigned long dansPhase = phaseB ? (maintenant - t0 - DUREE_PHASE_MS)
                                   : (maintenant - t0);
  if (dansPhase >= (DUREE_PHASE_MS - 120000UL)) {
    if (phaseB) {
      sommeTempB += bme.temperature;
      nTempB++;
    } else {
      sommeTempA += bme.temperature;
      nTempA++;
    }
  }

  if (!phaseB) {
    dernierGaz = bme.gas_resistance;
  }
}

/* -------------------------------------------------------------- resume */
void resume() {
  Serial.println();
  Serial.println(F("=== RESUME ==="));

  if (nTempA > 0 && nTempB > 0) {
    float tA = sommeTempA / nTempA;
    float tB = sommeTempB / nTempB;

    Serial.print(F("Temperature stabilisee, chauffage ACTIF  : "));
    Serial.print(tA, 2);
    Serial.println(F(" C"));

    Serial.print(F("Temperature stabilisee, chauffage COUPE  : "));
    Serial.print(tB, 2);
    Serial.println(F(" C"));

    Serial.print(F("DECALAGE d'auto-echauffement             : "));
    Serial.print(tA - tB, 2);
    Serial.println(F(" C"));
    Serial.println(F("(a retrancher des mesures faites chauffage actif)"));
  } else {
    Serial.println(F("Moyennes indisponibles : releves insuffisants."));
  }

  Serial.print(F("Derniere resistance de gaz, fin de phase A : "));
  Serial.print(dernierGaz / 1000.0, 1);
  Serial.println(F(" kOhm"));

  Serial.println();
  Serial.println(F("Copier tout l'affichage et le transmettre a NetSec."));
  Serial.println(F("Les lignes CSV commencent apres l'en-tete"));
  Serial.println(F("t_s;phase;temp_C;hum_pct;press_Pa;gas_Ohm"));
}

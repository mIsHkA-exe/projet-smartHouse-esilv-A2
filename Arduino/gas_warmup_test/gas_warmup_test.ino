#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

#define ADDR_I2C_BME680   0x77

const unsigned long PERIODE_MS      = 15000UL;
const unsigned long DUREE_PHASE_MS  = 600000UL;

const uint16_t CHAUFFE_TEMP_C  = 320;
const uint16_t CHAUFFE_DUREE_MS = 150;

Adafruit_BME680 bme;

unsigned long t0 = 0;
unsigned long prochainReleve = 0;
bool phaseB = false;

float sommeTempA = 0;  uint8_t nTempA = 0;
float sommeTempB = 0;  uint8_t nTempB = 0;
float dernierGaz = 0;

void setup() {
  Serial.begin(115200);
  delay(3000);

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

  bme.setGasHeater(CHAUFFE_TEMP_C, CHAUFFE_DUREE_MS);

  Serial.println(F("PHASE A : chauffage ACTIF pendant 10 minutes."));
  Serial.println(F("Ne touchez pas au capteur.\n"));

  Serial.println(F("t_s;phase;temp_C;hum_pct;press_Pa;gas_Ohm"));

  t0 = millis();
  prochainReleve = t0;
}

void loop() {
  unsigned long maintenant = millis();

  if (!phaseB && (maintenant - t0) >= DUREE_PHASE_MS) {
    phaseB = true;
    bme.setGasHeater(0, 0);
    Serial.println();
    Serial.println(F("# --- PHASE B : chauffage COUPE ---"));
    Serial.println(F("# La temperature va redescendre vers l'ambiante."));
    Serial.println(F("# La colonne gas_Ohm n'a plus de sens ici."));
    Serial.println();
  }

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

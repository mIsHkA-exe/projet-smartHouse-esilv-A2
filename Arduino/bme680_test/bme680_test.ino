/*
  bme680_test — Smart House
  Chemin dans le depot : firmware/tests/bme680_test/bme680_test.ino
  (le dossier doit porter le meme nom que le fichier .ino)

  Objet  : test unitaire du BME680 Adafruit sur Arduino UNO R4 WiFi.
  Cablage: VIN -> 5V, GND -> GND, SDI -> A4 (SDA), SCK -> A5 (SCL).
           SDO, CS et 3Vo non connectees (mode I2C, adresse 0x77).
  Statut : test, pas le firmware final. Aucun identifiant reel ici.

  Le croquis fait deux choses dans l'ordre :
    1. un scan du bus I2C, pour verifier le cablage avant tout dialogue ;
    2. une lecture des 4 grandeurs toutes les 2 secondes.
*/

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

#define BME680_I2C_ADDR 0x77   // marquage au dos de la carte Adafruit

Adafruit_BME680 bme;           // I2C par defaut

// ---------------------------------------------------------------- scan I2C
void scanI2C() {
  Serial.println(F("--- Scan du bus I2C ---"));
  byte trouves = 0;

  for (byte adresse = 1; adresse < 127; adresse++) {
    Wire.beginTransmission(adresse);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("  peripherique trouve a 0x"));
      if (adresse < 16) Serial.print('0');
      Serial.println(adresse, HEX);
      trouves++;
    }
  }

  if (trouves == 0) {
    Serial.println(F("  aucun peripherique : verifier SDA/SCL, VIN et GND"));
  }
  Serial.println();
}

// ------------------------------------------------------------------- setup
void setup() {
  Serial.begin(115200);
  // USB natif sur la R4 : le port disparait puis revient apres televersement.
  // Un delai fixe laisse au moniteur serie le temps de se reconnecter, et le
  // programme demarre meme si aucun moniteur n'est ouvert.
  delay(3000);

  Serial.println(F("=== Test BME680 - Smart House ==="));

  Wire.begin();
  scanI2C();

  if (!bme.begin(BME680_I2C_ADDR)) {
    Serial.println(F("ECHEC : BME680 non detecte a 0x77."));
    Serial.println(F("Si le scan a montre 0x76, changer BME680_I2C_ADDR."));
    while (1) {
      delay(1000);
    }
  }
  Serial.println(F("BME680 detecte."));

  // Reglages recommandes par Adafruit pour un usage general
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);  // 320 degres C pendant 150 ms

  Serial.println(F("Note : la resistance de gaz se stabilise apres"));
  Serial.println(F("plusieurs minutes de chauffe. Les premieres"));
  Serial.println(F("valeurs de gaz ne sont pas exploitables.\n"));
}

// -------------------------------------------------------------------- loop
void loop() {
  if (!bme.performReading()) {
    Serial.println(F("Echec de lecture."));
    delay(2000);
    return;
  }

  Serial.print(F("Temperature : "));
  Serial.print(bme.temperature, 2);
  Serial.println(F(" degres C"));

  Serial.print(F("Humidite    : "));
  Serial.print(bme.humidity, 2);
  Serial.println(F(" %HR"));

  Serial.print(F("Pression    : "));
  Serial.print(bme.pressure / 100.0, 2);
  Serial.println(F(" hPa"));

  Serial.print(F("Gaz         : "));
  Serial.print(bme.gas_resistance / 1000.0, 2);
  Serial.println(F(" kOhm"));

  Serial.println();
  delay(2000);
}

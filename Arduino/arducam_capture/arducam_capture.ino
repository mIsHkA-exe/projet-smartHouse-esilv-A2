/*
  arducam_capture — Smart House

  Objet  : capture d'une image JPEG 320x240 avec l'Arducam Mini 5MP Plus
           (B0068, OV5642) sur Arduino UNO R4 WiFi, et transfert des octets
           en hexadecimal sur la liaison serie.
  Cablage: CS->D10, MOSI->D11, MISO->D12, SCK->D13, GND->GND, VCC->5V,
           SDA->A4, SCL->A5. BME680 debranche pendant ce test.
  Prealab: les deux correctifs Renesas dans la bibliotheque ArduCAM, et
           #define OV5642_MINI_5MP_PLUS seul decommente dans memorysaver.h.
  Statut : test, pas le firmware final. Aucun identifiant reel ici.

  Point important pour NetSec : l'image reste dans le tampon de 8 Mo de la
  carte camera. Elle est lue par blocs et retransmise au fil de l'eau.
  A aucun moment l'image entiere n'occupe la SRAM de l'Arduino : seuls
  quelques octets de travail sont utilises.

  Usage : televerser, ouvrir le moniteur serie a 115200 bauds, appuyer sur
  RESET, attendre la ligne FIN IMAGE, puis copier tout l'affichage dans un
  fichier texte et le passer a hex_to_jpg.py.
*/

#include <Wire.h>
#include <SPI.h>
#include <ArduCAM.h>
#include "memorysaver.h"

#if !(defined OV5642_MINI_5MP_PLUS)
  #error "Decommenter #define OV5642_MINI_5MP_PLUS dans memorysaver.h"
#endif

const int CAM_CS = 10;

// Nombre d'octets affiches par ligne. 32 octets donnent des lignes de
// 64 caracteres, lisibles et faciles a copier.
const uint8_t OCTETS_PAR_LIGNE = 32;

// Garde-fou : une image 320x240 depasse rarement 40 ko. Au-dela, on
// considere que la lecture de la taille est erronee et on arrete.
const uint32_t TAILLE_MAX = 60000UL;

ArduCAM myCAM(OV5642, CAM_CS);

// ------------------------------------------------- affichage d'un octet
void afficherOctet(uint8_t v) {
  if (v < 0x10) Serial.print('0');
  Serial.print(v, HEX);
}

// ------------------------------------------------------------------- setup
void setup() {
  Serial.begin(115200);
  delay(3000);  // USB natif de la R4 : laisse le moniteur se reconnecter

  Serial.println(F("=== Capture Arducam 320x240 - Smart House ==="));

  pinMode(CAM_CS, OUTPUT);
  digitalWrite(CAM_CS, HIGH);

  Wire.begin();
  SPI.begin();

  // --- Verification du bus SPI, comme dans arducam_test -----------------
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  delay(10);
  if (myCAM.read_reg(ARDUCHIP_TEST1) != 0x55) {
    Serial.println(F("ECHEC : le bus SPI ne repond pas. Test arrete."));
    while (1) {
      delay(1000);
    }
  }

  // --- Configuration du capteur ----------------------------------------
  Serial.println(F("Configuration du capteur..."));
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
  myCAM.OV5642_set_JPEG_size(OV5642_320x240);
  delay(1000);           // laisse l'exposition automatique se stabiliser
  myCAM.clear_fifo_flag();

  // --- Capture ----------------------------------------------------------
  Serial.println(F("Capture en cours..."));
  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();

  uint32_t debut = millis();
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
    if (millis() - debut > 5000) {
      Serial.println(F("ECHEC : capture non terminee apres 5 s."));
      while (1) {
        delay(1000);
      }
    }
  }

  uint32_t longueur = myCAM.read_fifo_length();
  Serial.print(F("Taille annoncee par la camera : "));
  Serial.print(longueur);
  Serial.println(F(" octets"));

  if (longueur == 0) {
    Serial.println(F("ECHEC : taille nulle, aucune image dans le tampon."));
    while (1) {
      delay(1000);
    }
  }
  if (longueur >= TAILLE_MAX) {
    Serial.println(F("ECHEC : taille incoherente, transfert annule."));
    while (1) {
      delay(1000);
    }
  }

  // --- Transfert en hexadecimal ----------------------------------------
  // On ignore ce qui precede le marqueur de debut JPEG (FF D8) et on
  // s'arrete au marqueur de fin (FF D9).
  Serial.println(F("---DEBUT IMAGE---"));

  myCAM.CS_LOW();
  myCAM.set_fifo_burst();

  uint8_t octet = SPI.transfer(0x00);
  uint8_t precedent = 0;
  bool dansImage = false;
  uint8_t surLaLigne = 0;
  uint32_t ecrits = 0;

  longueur--;
  while (longueur--) {
    precedent = octet;
    octet = SPI.transfer(0x00);

    if (!dansImage) {
      // on attend la sequence FF D8 qui ouvre tout fichier JPEG
      if (precedent == 0xFF && octet == 0xD8) {
        dansImage = true;
        afficherOctet(precedent);
        afficherOctet(octet);
        surLaLigne = 2;
        ecrits = 2;
      }
    } else {
      afficherOctet(octet);
      surLaLigne++;
      ecrits++;

      if (surLaLigne >= OCTETS_PAR_LIGNE) {
        Serial.println();
        surLaLigne = 0;
      }

      // FF D9 ferme le fichier JPEG : inutile de lire la suite
      if (precedent == 0xFF && octet == 0xD9) {
        break;
      }
    }
    delayMicroseconds(15);
  }

  myCAM.CS_HIGH();
  myCAM.clear_fifo_flag();

  if (surLaLigne != 0) Serial.println();
  Serial.println(F("---FIN IMAGE---"));

  if (!dansImage) {
    Serial.println(F("ECHEC : marqueur de debut JPEG (FF D8) introuvable."));
  } else {
    Serial.print(F("Octets transferes : "));
    Serial.println(ecrits);
    Serial.println(F("Copier tout l'affichage dans un fichier texte,"));
    Serial.println(F("puis le passer a hex_to_jpg.py."));
  }
}

// -------------------------------------------------------------------- loop
void loop() {
  // rien : une seule capture au demarrage, relancee par RESET
}

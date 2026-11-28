#include <Wire.h>
#include <SPI.h>
#include <ArduCAM.h>
#include "memorysaver.h"

#if !(defined OV5642_MINI_5MP_PLUS)
  #error "Decommenter #define OV5642_MINI_5MP_PLUS dans memorysaver.h"
#endif

const int CAM_CS = 10;

ArduCAM myCAM(OV5642, CAM_CS);

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println(F("=== Test Arducam Mini 5MP Plus - Smart House ==="));

  pinMode(CAM_CS, OUTPUT);
  digitalWrite(CAM_CS, HIGH);

  Wire.begin();
  SPI.begin();

  Serial.println(F("\n[1] Test du bus SPI"));
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  delay(10);
  uint8_t lu = myCAM.read_reg(ARDUCHIP_TEST1);

  Serial.print(F("  ecrit 0x55, relu 0x"));
  if (lu < 16) Serial.print('0');
  Serial.println(lu, HEX);

  if (lu != 0x55) {
    Serial.println(F("  ECHEC : verifier CS, MOSI, MISO, SCK, VCC et GND."));
    Serial.println(F("  Le test s'arrete ici."));
    while (1) {
      delay(1000);
    }
  }
  Serial.println(F("  OK : le bus SPI repond."));

  Serial.println(F("\n[2] Version de l'ArduChip"));
  uint8_t version = myCAM.read_reg(ARDUCHIP_REV);
  Serial.print(F("  registre 0x40 = 0x"));
  if (version < 16) Serial.print('0');
  Serial.println(version, HEX);

  Serial.println(F("\n[3] Identifiant du capteur sur le bus I2C"));
  uint8_t vid = 0, pid = 0;
  myCAM.wrSensorReg16_8(0xff, 0x01);
  myCAM.rdSensorReg16_8(0x300a, &vid);
  myCAM.rdSensorReg16_8(0x300b, &pid);

  Serial.print(F("  attendu 0x56 0x42, lu 0x"));
  if (vid < 16) Serial.print('0');
  Serial.print(vid, HEX);
  Serial.print(F(" 0x"));
  if (pid < 16) Serial.print('0');
  Serial.println(pid, HEX);

  if (vid == 0x56 && pid == 0x42) {
    Serial.println(F("  OK : OV5642 identifie."));
    Serial.println(F("\nLes deux bus repondent. Capture d'image : etape suivante."));
  } else {
    Serial.println(F("  ECHEC : verifier SDA (A4) et SCL (A5)."));
  }
}

void loop() {

}

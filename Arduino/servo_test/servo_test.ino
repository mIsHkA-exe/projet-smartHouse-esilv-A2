#include <Servo.h>

const int PIN_SERVO = 9;

const int US_MIN = 900;
const int US_MAX = 2100;

const int ANGLE_CENTRE = 90;
const int EXCURSION_PETITE = 15;
const int EXCURSION_GRANDE = 30;

Servo servo;

unsigned int cycle = 0;

void allerLentement(int depuis, int vers, int pauseMs) {
  if (depuis < vers) {
    for (int a = depuis; a <= vers; a++) {
      servo.write(a);
      delay(pauseMs);
    }
  } else {
    for (int a = depuis; a >= vers; a--) {
      servo.write(a);
      delay(pauseMs);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println();
  Serial.println(F("########################################"));
  Serial.println(F("#  DEMARRAGE - Test servo FS90         #"));
  Serial.println(F("#  Cette banniere ne doit apparaitre   #"));
  Serial.println(F("#  QU'UNE SEULE FOIS.                  #"));
  Serial.println(F("########################################"));
  Serial.println();
  Serial.println(F("Palonnier libre, sans charge mecanique."));
  Serial.println(F("Debranchez tout si le servo force ou bourdonne.\n"));

  delay(2000);
}

void loop() {
  cycle++;

  Serial.print(F("===== Cycle "));
  Serial.print(cycle);
  Serial.println(F(" ====="));

  Serial.println(F("[1] Attache et maintien au centre (3 s)"));
  servo.attach(PIN_SERVO, US_MIN, US_MAX);
  servo.write(ANGLE_CENTRE);
  delay(3000);

  Serial.println(F("[2] Petits deplacements lents (+/- 15 deg)"));
  allerLentement(ANGLE_CENTRE, ANGLE_CENTRE - EXCURSION_PETITE, 20);
  delay(500);
  allerLentement(ANGLE_CENTRE - EXCURSION_PETITE, ANGLE_CENTRE + EXCURSION_PETITE, 20);
  delay(500);
  allerLentement(ANGLE_CENTRE + EXCURSION_PETITE, ANGLE_CENTRE, 20);
  delay(1000);

  Serial.println(F("[3] Deplacements amples lents (+/- 30 deg)"));
  allerLentement(ANGLE_CENTRE, ANGLE_CENTRE - EXCURSION_GRANDE, 20);
  delay(500);
  allerLentement(ANGLE_CENTRE - EXCURSION_GRANDE, ANGLE_CENTRE + EXCURSION_GRANDE, 20);
  delay(500);
  allerLentement(ANGLE_CENTRE + EXCURSION_GRANDE, ANGLE_CENTRE, 20);
  delay(1000);

  Serial.println(F("[4] Deplacements RAPIDES (+/- 30 deg) - pic de courant"));
  for (uint8_t i = 0; i < 3; i++) {
    servo.write(ANGLE_CENTRE - EXCURSION_GRANDE);
    delay(600);
    servo.write(ANGLE_CENTRE + EXCURSION_GRANDE);
    delay(600);
  }
  servo.write(ANGLE_CENTRE);
  delay(600);

  Serial.println(F("[5] Detachement : le servo est relache"));
  servo.detach();

  Serial.print(F("Cycle "));
  Serial.print(cycle);
  Serial.println(F(" termine sans redemarrage.\n"));

  delay(3000);
}

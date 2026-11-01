/*
  servo_test — Smart House
  Chemin dans le depot : Arduino/servo_test/servo_test.ino
  (le dossier doit porter le meme nom que le fichier .ino)

  Objet  : test unitaire du servomoteur FS90 sur Arduino UNO R4 WiFi,
           alimente par le seul port USB du PC (D30), sans alimentation
           externe et sans condensateur de decouplage.
           Faute de multimetre, la tenue de l'alimentation est jugee de
           maniere OBSERVATIONNELLE : voir plus bas.
  Cablage: signal (orange) -> D9, rouge -> 5V, marron -> GND.
           Camera et HC-SR04 debranches. Palonnier LIBRE, sans charge.
  Statut : test, pas le firmware final. Aucun identifiant reel ici.

  ---------------------------------------------------------------------
  COMMENT LIRE CE TEST SANS MULTIMETRE
  ---------------------------------------------------------------------
  Une chute de tension sous le seuil de fonctionnement provoque une
  reinitialisation de la carte. Le programme affiche au demarrage une
  banniere et un compteur de cycles qui repart a 1.

    - La banniere reapparait en cours de test  -> la carte a redemarre :
      l'alimentation NE TIENT PAS.
    - Le compteur avance sans interruption     -> l'alimentation tient.

  Signes a surveiller en plus de l'affichage :
    - le port serie disparait puis revient ;
    - le servo tressaute ou bourdonne au lieu de se deplacer net ;
    - la matrice de LED de la carte se reinitialise.

  ---------------------------------------------------------------------
  PRECAUTION CONTRE LE BLOCAGE
  ---------------------------------------------------------------------
  Un FS90 bloque en butee tire 700 mA (fiche FEETECH a 4,8 V), contre
  100 mA en mouvement libre. Deux garde-fous sont appliques ici :

  1. attach() recoit explicitement 900 et 2100 microsecondes, la plage
     annoncee par le fabricant pour le FS90. Sans ces valeurs, la
     bibliotheque utiliserait 544 a 2400 us par defaut et pousserait le
     servo au-dela de ses butees mecaniques, donc en blocage.
  2. Le deplacement reste volontairement entre 60 et 120 degres, soit
     1300 a 1700 us : largement a l'interieur de la course, quelle que
     soit l'amplitude reelle du FS90 (les sources divergent entre 120 et
     180 degres, point non tranche).

  Le test monte en severite phase par phase. Si la carte redemarre,
  la phase affichee juste avant indique ce qui a provoque la chute.
*/

#include <Servo.h>

const int PIN_SERVO = 9;

// Plage d'impulsion du FS90 selon le fabricant, en microsecondes.
const int US_MIN = 900;
const int US_MAX = 2100;

// Course utilisee : centre a 90, excursion limitee de part et d'autre.
const int ANGLE_CENTRE = 90;
const int EXCURSION_PETITE = 15;
const int EXCURSION_GRANDE = 30;

Servo servo;

unsigned int cycle = 0;

// ------------------------------------------- deplacement degre par degre
// Un pas de 1 degre toutes les 'pauseMs' millisecondes : le servo tire un
// courant modere et regulier.
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

// --------------------------------------------------------------- setup
void setup() {
  Serial.begin(115200);
  delay(3000);  // USB natif de la R4 : laisse le moniteur se reconnecter

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

// ---------------------------------------------------------------- loop
void loop() {
  cycle++;

  Serial.print(F("===== Cycle "));
  Serial.print(cycle);
  Serial.println(F(" ====="));

  // --- Phase 1 : mise sous tension du signal, sans mouvement -----------
  Serial.println(F("[1] Attache et maintien au centre (3 s)"));
  servo.attach(PIN_SERVO, US_MIN, US_MAX);
  servo.write(ANGLE_CENTRE);
  delay(3000);

  // --- Phase 2 : petits deplacements lents -----------------------------
  Serial.println(F("[2] Petits deplacements lents (+/- 15 deg)"));
  allerLentement(ANGLE_CENTRE, ANGLE_CENTRE - EXCURSION_PETITE, 20);
  delay(500);
  allerLentement(ANGLE_CENTRE - EXCURSION_PETITE, ANGLE_CENTRE + EXCURSION_PETITE, 20);
  delay(500);
  allerLentement(ANGLE_CENTRE + EXCURSION_PETITE, ANGLE_CENTRE, 20);
  delay(1000);

  // --- Phase 3 : deplacements plus amples, toujours lents --------------
  Serial.println(F("[3] Deplacements amples lents (+/- 30 deg)"));
  allerLentement(ANGLE_CENTRE, ANGLE_CENTRE - EXCURSION_GRANDE, 20);
  delay(500);
  allerLentement(ANGLE_CENTRE - EXCURSION_GRANDE, ANGLE_CENTRE + EXCURSION_GRANDE, 20);
  delay(500);
  allerLentement(ANGLE_CENTRE + EXCURSION_GRANDE, ANGLE_CENTRE, 20);
  delay(1000);

  // --- Phase 4 : deplacements rapides, le cas le plus exigeant ---------
  // Le servo part a pleine vitesse : c'est le pic de courant le plus eleve
  // du test. Si l'alimentation doit ceder, c'est ici.
  Serial.println(F("[4] Deplacements RAPIDES (+/- 30 deg) - pic de courant"));
  for (uint8_t i = 0; i < 3; i++) {
    servo.write(ANGLE_CENTRE - EXCURSION_GRANDE);
    delay(600);
    servo.write(ANGLE_CENTRE + EXCURSION_GRANDE);
    delay(600);
  }
  servo.write(ANGLE_CENTRE);
  delay(600);

  // --- Detachement au repos -------------------------------------------
  // Mesure d'attenuation retenue pour D30 : le servo ne consomme plus
  // rien entre deux manoeuvres de la fenetre.
  Serial.println(F("[5] Detachement : le servo est relache"));
  servo.detach();

  Serial.print(F("Cycle "));
  Serial.print(cycle);
  Serial.println(F(" termine sans redemarrage.\n"));

  delay(3000);
}

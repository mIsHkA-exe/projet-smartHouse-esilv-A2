/*
  hcsr04_test — Smart House
  Chemin dans le depot : Arduino/hcsr04_test/hcsr04_test.ino
  (le dossier doit porter le meme nom que le fichier .ino)

  Objet  : test unitaire du capteur a ultrasons HC-SR04 sur Arduino
           UNO R4 WiFi. Mesure de distance et evaluation de la stabilite
           des mesures, en vue de la calibration de la detection
           d'intrusion (cible fixe du cache camera).
  Cablage: VCC -> 5V, TRIG -> D7, ECHO -> D8, GND -> GND.
           Camera et BME680 debranches pendant ce test.
  Statut : test, pas le firmware final. Aucun identifiant reel ici.
  Note   : aucune bibliotheque externe. Le module est en logique 5 V,
           comme la carte : pas d'adaptation de niveau necessaire.

  Principe de la mesure :
    1. une impulsion de 10 us sur TRIG lance une salve d'ultrasons ;
    2. ECHO passe au niveau haut pendant la duree de l'aller-retour ;
    3. distance = duree x vitesse du son / 2.

  Pour juger la stabilite (fausses detections), chaque cycle fait
  5 mesures et affiche la mediane, le minimum et le maximum.
*/

const int PIN_TRIG = 7;
const int PIN_ECHO = 8;

// Vitesse du son a 20 degres C, en cm par microseconde.
// Elle varie d'environ 0,17 % par degre : le BME680 pourra servir a la
// corriger plus tard si la precision le demande.
const float VITESSE_SON = 0.0343;

// Portee annoncee : 4 m. Aller-retour de 8 m, soit environ 23 ms.
// 30 ms laissent une marge et evitent une attente inutile.
const unsigned long TIMEOUT_US = 30000UL;

// La fiche du HC-SR04 demande au moins 60 ms entre deux mesures pour
// que l'echo precedent se soit eteint.
const unsigned long DELAI_ENTRE_MESURES_MS = 60;

const uint8_t NB_MESURES = 5;

// ------------------------------------------------- une mesure unitaire
// Renvoie la duree de l'echo en microsecondes, ou 0 si aucun echo.
unsigned long mesurerDureeUs() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  return pulseIn(PIN_ECHO, HIGH, TIMEOUT_US);
}

// ------------------------------------------------------ tri croissant
void trier(unsigned long t[], uint8_t n) {
  for (uint8_t i = 1; i < n; i++) {
    unsigned long v = t[i];
    int8_t j = i - 1;
    while (j >= 0 && t[j] > v) {
      t[j + 1] = t[j];
      j--;
    }
    t[j + 1] = v;
  }
}

// ------------------------------------------------------------- setup
void setup() {
  Serial.begin(115200);
  delay(3000);  // USB natif de la R4 : laisse le moniteur se reconnecter

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);

  Serial.println(F("=== Test HC-SR04 - Smart House ==="));
  Serial.println(F("TRIG sur D7, ECHO sur D8."));
  Serial.print(F("Mediane de "));
  Serial.print(NB_MESURES);
  Serial.println(F(" mesures par cycle, un cycle par seconde.\n"));
  Serial.println(F("mediane      min      max    ecart   duree"));
  Serial.println(F("------------------------------------------"));

  delay(100);  // laisse le module se stabiliser apres la mise sous tension
}

// -------------------------------------------------------------- loop
void loop() {
  unsigned long durees[NB_MESURES];
  uint8_t valides = 0;

  for (uint8_t i = 0; i < NB_MESURES; i++) {
    unsigned long d = mesurerDureeUs();
    if (d > 0) {
      durees[valides] = d;
      valides++;
    }
    delay(DELAI_ENTRE_MESURES_MS);
  }

  if (valides == 0) {
    Serial.println(F("hors portee : aucun echo recu"));
    Serial.println(F("  (objet trop loin, trop oblique, ou cablage a verifier)"));
    delay(1000);
    return;
  }

  trier(durees, valides);

  unsigned long mediane = durees[valides / 2];
  unsigned long mini = durees[0];
  unsigned long maxi = durees[valides - 1];

  float distMediane = mediane * VITESSE_SON / 2.0;
  float distMini = mini * VITESSE_SON / 2.0;
  float distMaxi = maxi * VITESSE_SON / 2.0;

  Serial.print(distMediane, 1);
  Serial.print(F(" cm   "));
  Serial.print(distMini, 1);
  Serial.print(F("    "));
  Serial.print(distMaxi, 1);
  Serial.print(F("     "));
  Serial.print(distMaxi - distMini, 1);
  Serial.print(F("    "));
  Serial.print(mediane);
  Serial.print(F(" us"));

  Serial.print(F("   "));
  Serial.print(valides);
  Serial.print(F("/"));
  Serial.print(NB_MESURES);

  // Avec moins de 3 mesures valides, min, max et mediane se confondent :
  // l'ecart affiche 0.0 alors que la mesure est au contraire peu fiable.
  if (valides < 3) {
    Serial.print(F("  ecart non significatif"));
  }
  Serial.println();

  delay(1000);
}

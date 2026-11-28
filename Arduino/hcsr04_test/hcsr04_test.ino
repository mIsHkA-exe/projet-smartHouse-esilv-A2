const int PIN_TRIG = 7;
const int PIN_ECHO = 8;

const float VITESSE_SON = 0.0343;

const unsigned long TIMEOUT_US = 30000UL;

const unsigned long DELAI_ENTRE_MESURES_MS = 60;

const uint8_t NB_MESURES = 5;

unsigned long mesurerDureeUs() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  return pulseIn(PIN_ECHO, HIGH, TIMEOUT_US);
}

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

void setup() {
  Serial.begin(115200);
  delay(3000);

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

  delay(100);
}

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

  if (valides < 3) {
    Serial.print(F("  ecart non significatif"));
  }
  Serial.println();

  delay(1000);
}

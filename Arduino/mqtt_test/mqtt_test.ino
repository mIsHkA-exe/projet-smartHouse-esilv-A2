#include <Wire.h>
#include <SPI.h>
#include <ArduCAM.h>
#include "memorysaver.h"
#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include "arduino_secrets.h"
#include <unistd.h>

#if !(defined OV5642_MINI_5MP_PLUS)
  #error "Decommenter #define OV5642_MINI_5MP_PLUS dans memorysaver.h"
#endif

const uint16_t BROKER_PORT = 8883;
const char CLIENT_ID[] = "smarthouse-carte";

const char TOPIC_CONNEXION[] = "smarthouse/carte/connexion";
const char TOPIC_ETAT[]      = "smarthouse/carte/etat";
const char TOPIC_ACK[]       = "smarthouse/carte/ack";
const char TOPIC_COMMANDE[]  = "smarthouse/serveur/commande";
const char TOPIC_IMAGE_ANNONCE[] = "smarthouse/carte/image/annonce";

const char PAYLOAD_EN_LIGNE[]   = "{\"en_ligne\":true}";
const char PAYLOAD_HORS_LIGNE[] = "{\"en_ligne\":false}";

const unsigned long KEEPALIVE_MS = 15000UL;

const unsigned long PERIODE_ETAT_MS        = 10000UL;
const unsigned long PERIODE_RECONNEXION_MS = 5000UL;

const size_t TAILLE_CMD_MAX = 40;
const size_t TAILLE_MOT_MAX = 20;

const int CAM_CS = 10;
const unsigned long DELAI_AVANT_CAPTURE_MS = 1000UL;
const unsigned long CAPTURE_TIMEOUT_MS = 5000UL;
const uint32_t TAILLE_FIFO_MAX = 60000UL;

const size_t TAILLE_MORCEAU = 1024;
const unsigned long CADENCE_IMAGE_MS = 3000UL;

ArduCAM myCAM(OV5642, CAM_CS);
bool cameraOk = false;

uint8_t tamponMorceau[TAILLE_MORCEAU];
unsigned long numeroImage = 0;
unsigned long dernierEnvoiImage = 0;
unsigned long nbImagesEnvoyees = 0;
unsigned long nbEchecsImage = 0;
unsigned long dureeMaxImageMs = 0;
unsigned long dureeTotaleImagesMs = 0;
unsigned long dureeMaxCaptureMs = 0;

enum Alarme { LATENT, INTRUSION, ALERTE };
Alarme alarme = LATENT;
bool buzzer = false;
bool ledRouge = false;
bool diffusion = false;

unsigned long dernierIdTraite = 0;
const char* dernierResultat = "ok";

char commandeRecue[TAILLE_CMD_MAX + 1];
bool commandeEnAttente = false;
bool commandeTropLongue = false;

WiFiSSLClient sslClient;
MqttClient mqttClient(sslClient);

unsigned long dernierEtat = 0;
unsigned long dernierEssaiMqtt = 0;
unsigned long nbEchecsMqtt = 0;

const char* nomAlarme(Alarme a) {
  switch (a) {
    case LATENT:    return "latent";
    case INTRUSION: return "intrusion";
    case ALERTE:    return "alerte";
  }
  return "latent";
}

void publierEtat() {
  char json[128];
  snprintf(json, sizeof(json),
           "{\"alarme\":\"%s\",\"buzzer\":%s,\"led_rouge\":%s,"
           "\"diffusion\":%s}",
           nomAlarme(alarme),
           buzzer ? "true" : "false",
           ledRouge ? "true" : "false",
           diffusion ? "true" : "false");
  mqttClient.beginMessage(TOPIC_ETAT, true  , 1  );
  mqttClient.print(json);
  mqttClient.endMessage();
  Serial.print(F("[ETAT->] "));
  Serial.println(json);
}

void publierAck(unsigned long id, const char* mot, const char* resultat) {
  char json[96];
  snprintf(json, sizeof(json),
           "{\"id\":%lu,\"cmd\":\"%s\",\"resultat\":\"%s\"}",
           id, mot, resultat);
  mqttClient.beginMessage(TOPIC_ACK, false  , 1  );
  mqttClient.print(json);
  mqttClient.endMessage();
  Serial.print(F("[ACK->] "));
  Serial.println(json);
}

bool motValide(const char* mot) {
  size_t n = strlen(mot);
  if (n == 0 || n > TAILLE_MOT_MAX) return false;
  for (size_t i = 0; i < n; i++) {
    char c = mot[i];
    if (!((c >= 'A' && c <= 'Z') || c == '_')) return false;
  }
  return true;
}

bool lireId(const char* texte, unsigned long* id) {
  if (texte == nullptr || *texte == '\0') return false;
  for (const char* p = texte; *p; p++) {
    if (*p < '0' || *p > '9') return false;
  }
  char* fin = nullptr;
  unsigned long v = strtoul(texte, &fin, 10);
  if (v == 0) return false;
  *id = v;
  return true;
}

const char* executer(const char* mot, bool* change) {
  *change = false;

  if (strcmp(mot, "OBTENIR") == 0) {
    if (alarme == ALERTE && buzzer) return "refuse";
    if (!diffusion) { diffusion = true; *change = true;
      Serial.println(F("[ACTION] Diffusion d'images démarrée")); }
    return "ok";
  }
  if (strcmp(mot, "ARRETER_DIFFUSION") == 0) {
    if (diffusion) { diffusion = false; *change = true;
      Serial.println(F("[ACTION] Diffusion d'images arrêtée")); }
    return "ok";
  }
  if (strcmp(mot, "ALERTER") == 0) {
    if (alarme != ALERTE) {
      diffusion = false;
      alarme = ALERTE; buzzer = true; ledRouge = true; *change = true;
      Serial.println(F("[ACTION] Diffusion arrêtée, buzzer + LED rouge (simulés)"));
    }
    return "ok";
  }
  if (strcmp(mot, "STOP_BUZZER") == 0) {
    if (alarme == ALERTE && buzzer) { buzzer = false; *change = true;
      Serial.println(F("[ACTION] Buzzer arrêté (simulé), LED rouge maintenue")); }
    return "ok";
  }
  if (strcmp(mot, "REARMER") == 0) {
    if (alarme != LATENT) {
      alarme = LATENT; buzzer = false; ledRouge = false; *change = true;
      Serial.println(F("[ACTION] Réarmement : retour à l'état latent (simulé)"));
    }
    return "ok";
  }
  return nullptr;
}

void traiterCommande() {
  Serial.print(F("[CMD<-] "));
  Serial.println(commandeTropLongue ? "(trop longue)" : commandeRecue);

  if (commandeTropLongue) {
    publierAck(0, "", "inconnu");
    return;
  }

  char* espace = strchr(commandeRecue, ' ');
  if (espace == nullptr) {
    publierAck(0, motValide(commandeRecue) ? commandeRecue : "", "inconnu");
    return;
  }
  *espace = '\0';
  const char* mot = commandeRecue;
  const char* texteId = espace + 1;

  unsigned long id = 0;
  if (!motValide(mot) || !lireId(texteId, &id)) {
    publierAck(0, motValide(mot) ? mot : "", "inconnu");
    return;
  }

  if (id <= dernierIdTraite) {
    const char* res = (id == dernierIdTraite) ? dernierResultat : "ok";
    Serial.print(F("[CMD] Doublon ignoré, ID "));
    Serial.println(id);
    publierAck(id, mot, res);
    return;
  }

  bool change = false;
  const char* resultat = executer(mot, &change);
  if (resultat == nullptr) resultat = "inconnu";
  dernierIdTraite = id;
  dernierResultat = resultat;

  publierAck(id, mot, resultat);
  if (change) publierEtat();
}

long memoireMin = 1000000L;

long memoireLibre() {
  char repere;
  return (long)(&repere - (char*)sbrk(0));
}

void noterMemoire() {
  long m = memoireLibre();
  if (m < memoireMin) memoireMin = m;
}

void afficherMemoire(const __FlashStringHelper* etape) {
  long m = memoireLibre();
  if (m < memoireMin) memoireMin = m;
  Serial.print(F("[MEM] "));
  Serial.print(etape);
  Serial.print(F(" : "));
  Serial.print(m);
  Serial.print(F(" o libres | minimum depuis le démarrage : "));
  Serial.print(memoireMin);
  Serial.println(F(" o"));
}

void surMessage(int taille) {
  noterMemoire();
  String topic = mqttClient.messageTopic();
  if (topic != TOPIC_COMMANDE) {
    while (mqttClient.available()) mqttClient.read();
    return;
  }
  size_t n = 0;
  commandeTropLongue = false;
  while (mqttClient.available()) {
    int c = mqttClient.read();
    if (n < TAILLE_CMD_MAX) commandeRecue[n++] = (char)c;
    else commandeTropLongue = true;
  }
  commandeRecue[n] = '\0';
  commandeEnAttente = true;
}

bool initialiserCamera() {
  pinMode(CAM_CS, OUTPUT);
  digitalWrite(CAM_CS, HIGH);
  Wire.begin();
  SPI.begin();

  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  delay(10);
  if (myCAM.read_reg(ARDUCHIP_TEST1) != 0x55) {
    Serial.println(F("[CAM] ECHEC : le bus SPI ne répond pas. Images désactivées."));
    return false;
  }

  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
  myCAM.OV5642_set_JPEG_size(OV5642_320x240);
  delay(DELAI_AVANT_CAPTURE_MS);
  myCAM.clear_fifo_flag();
  Serial.println(F("[CAM] Caméra prête (JPEG 320x240)."));
  return true;
}

void lireTampon(uint8_t* dest, size_t n) {
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();
  for (size_t i = 0; i < n; i++) {
    dest[i] = SPI.transfer(0x00);
    delayMicroseconds(15);
  }
  myCAM.CS_HIGH();
}

void terminerLecture() {
  myCAM.CS_HIGH();
  myCAM.clear_fifo_flag();
}

bool envoyerImageCamera() {
  if (!cameraOk) {
    Serial.println(F("[IMG] Caméra non initialisée"));
    return false;
  }
  const unsigned long debut = millis();

  myCAM.flush_fifo();
  myCAM.clear_fifo_flag();
  myCAM.start_capture();
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
    if (millis() - debut > CAPTURE_TIMEOUT_MS) {
      Serial.println(F("[IMG] ECHEC : capture non terminée après 5 s"));
      myCAM.clear_fifo_flag();
      return false;
    }
  }
  const unsigned long dureeCapture = millis() - debut;

  uint32_t restants = myCAM.read_fifo_length();
  if (restants == 0 || restants >= TAILLE_FIFO_MAX) {
    Serial.print(F("[IMG] ECHEC : taille du tampon incohérente : "));
    Serial.println(restants);
    myCAM.clear_fifo_flag();
    return false;
  }
  const uint32_t tailleTampon = restants;

  myCAM.CS_LOW();
  myCAM.set_fifo_burst();
  uint8_t octet = SPI.transfer(0x00);
  restants--;
  uint8_t precedent = 0;
  bool debutTrouve = false;
  while (restants > 0) {
    precedent = octet;
    octet = SPI.transfer(0x00);
    restants--;
    delayMicroseconds(15);
    if (precedent == 0xFF && octet == 0xD8) { debutTrouve = true; break; }
  }
  myCAM.CS_HIGH();
  if (!debutTrouve) {
    Serial.println(F("[IMG] ECHEC : marqueur FF D8 introuvable"));
    terminerLecture();
    return false;
  }

  const uint32_t tailleEnvoyee = restants + 2;
  const uint32_t nbMorceaux = (tailleEnvoyee + TAILLE_MORCEAU - 1) / TAILLE_MORCEAU;
  numeroImage++;

  char json[80];
  snprintf(json, sizeof(json), "{\"img\":%lu,\"taille\":%lu,\"morceaux\":%lu}",
           numeroImage, (unsigned long)tailleEnvoyee, (unsigned long)nbMorceaux);
  mqttClient.beginMessage(TOPIC_IMAGE_ANNONCE, false, 0);
  mqttClient.print(json);
  if (!mqttClient.endMessage()) {
    Serial.println(F("[IMG] Échec de l'annonce"));
    terminerLecture();
    return false;
  }

  char topic[48];
  uint32_t aEnvoyer = tailleEnvoyee;
  long finJpeg = -1;
  uint32_t position = 0;
  uint8_t dernier = 0;

  for (uint32_t n = 0; n < nbMorceaux; n++) {
    size_t taille = (aEnvoyer < TAILLE_MORCEAU) ? aEnvoyer : TAILLE_MORCEAU;
    size_t debutLecture = 0;

    if (n == 0) {
      tamponMorceau[0] = 0xFF;
      tamponMorceau[1] = 0xD8;
      debutLecture = 2;
    }
    lireTampon(tamponMorceau + debutLecture, taille - debutLecture);

    for (size_t i = 0; i < taille; i++) {
      if (finJpeg < 0 && dernier == 0xFF && tamponMorceau[i] == 0xD9 && position + i >= 2) {
        finJpeg = (long)(position + i + 1);
      }
      dernier = tamponMorceau[i];
    }

    snprintf(topic, sizeof(topic), "smarthouse/carte/image/%lu/%lu",
             numeroImage, (unsigned long)n);
    if (!mqttClient.beginMessage(topic, (unsigned long)taille, false, 0)) {
      Serial.print(F("[IMG] Échec beginMessage, morceau "));
      Serial.println(n);
      terminerLecture();
      return false;
    }
    const size_t ecrits = mqttClient.write(tamponMorceau, taille);
    if (ecrits != taille || !mqttClient.endMessage()) {
      Serial.print(F("[IMG] Échec d'envoi, morceau "));
      Serial.print(n);
      Serial.print(F(", octets écrits : "));
      Serial.println(ecrits);
      terminerLecture();
      return false;
    }
    noterMemoire();
    aEnvoyer -= taille;
    position += taille;
  }
  terminerLecture();

  const unsigned long duree = millis() - debut;
  nbImagesEnvoyees++;
  dureeTotaleImagesMs += duree;
  if (duree > dureeMaxImageMs) dureeMaxImageMs = duree;
  if (dureeCapture > dureeMaxCaptureMs) dureeMaxCaptureMs = dureeCapture;

  Serial.print(F("[IMG->] image "));
  Serial.print(numeroImage);
  Serial.print(F(" | tampon "));
  Serial.print(tailleTampon);
  Serial.print(F(" o | envoyés "));
  Serial.print(tailleEnvoyee);
  Serial.print(F(" o en "));
  Serial.print(nbMorceaux);
  Serial.print(F(" morceaux | JPEG réel "));
  if (finJpeg > 0) { Serial.print(finJpeg); Serial.print(F(" o")); }
  else Serial.print(F("FF D9 NON TROUVÉ"));
  Serial.print(F(" | capture "));
  Serial.print(dureeCapture);
  Serial.print(F(" ms | total "));
  Serial.print(duree);
  Serial.println(F(" ms"));
  return true;
}

unsigned long maxBoucleMs = 0;
unsigned long maxStatutWifiMs = 0;
unsigned long maxConnectedMs = 0;
unsigned long maxPollMs = 0;
unsigned long maxLocalIpMs = 0;

void noterMax(unsigned long* maxi, unsigned long debut) {
  unsigned long d = millis() - debut;
  if (d > *maxi) *maxi = d;
}

void afficherDurees() {
  Serial.print(F("[DUREES] max sur 10 s (ms) | boucle "));
  Serial.print(maxBoucleMs);
  Serial.print(F(" | WiFi.status "));
  Serial.print(maxStatutWifiMs);
  Serial.print(F(" | connected "));
  Serial.print(maxConnectedMs);
  Serial.print(F(" | poll "));
  Serial.print(maxPollMs);
  Serial.print(F(" | localIP "));
  Serial.println(maxLocalIpMs);
  maxBoucleMs = maxStatutWifiMs = maxConnectedMs = 0;
  maxPollMs = maxLocalIpMs = 0;
}

void connecterWifi() {
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("[WIFI] Module Wi-Fi introuvable : arrêt du test."));
    while (true) {   }
  }
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(F("[WIFI] Connexion au réseau : "));
    Serial.println(SECRET_SSID);
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    delay(2000);
  }
  Serial.println(F("[WIFI] Connecté."));
}

bool connecterMqtt() {
  IPAddress brokerIp;
  if (!brokerIp.fromString(SECRET_BROKER_IP)) {
    Serial.println(F("[MQTT] Adresse du broker invalide dans arduino_secrets.h"));
    return false;
  }

  sslClient.setCACert(SECRET_CA_CERT);

  mqttClient.setId(CLIENT_ID);
  mqttClient.setUsernamePassword(SECRET_MQTT_USER, SECRET_MQTT_PASS);
  mqttClient.setKeepAliveInterval(KEEPALIVE_MS);

  mqttClient.beginWill(TOPIC_CONNEXION, true  , 1  );
  mqttClient.print(PAYLOAD_HORS_LIGNE);
  mqttClient.endWill();

  Serial.print(F("[MQTT] Connexion TLS au broker "));
  Serial.print(brokerIp);
  Serial.print(':');
  Serial.println(BROKER_PORT);

  const unsigned long debutConnexion = millis();
  if (!mqttClient.connect(SECRET_BROKER_IP, BROKER_PORT)) {
    nbEchecsMqtt++;
    Serial.print(F("[MQTT] Échec, code d'erreur : "));
    Serial.println(mqttClient.connectError());
    return false;
  }
  Serial.print(F("[MQTT] Connecté en TLS (poignée de main + MQTT : "));
  Serial.print(millis() - debutConnexion);
  Serial.println(F(" ms)."));

  mqttClient.onMessage(surMessage);
  if (mqttClient.subscribe(TOPIC_COMMANDE, 1  )) {
    Serial.print(F("[MQTT] Abonné à "));
    Serial.println(TOPIC_COMMANDE);
  } else {
    Serial.println(F("[MQTT] Échec de l'abonnement aux commandes"));
  }

  mqttClient.beginMessage(TOPIC_CONNEXION, true  , 1  );
  mqttClient.print(PAYLOAD_EN_LIGNE);
  mqttClient.endMessage();
  publierEtat();
  return true;
}

void afficherEtat() {
  unsigned long t0 = millis();
  IPAddress ip = WiFi.localIP();
  noterMax(&maxLocalIpMs, t0);
  t0 = millis();
  bool connecte = mqttClient.connected();
  noterMax(&maxConnectedMs, t0);

  Serial.print(F("[ETAT] IP carte : "));
  Serial.print(ip);
  Serial.print(F(" | MQTT : "));
  Serial.print(connecte ? F("connecté") : F("déconnecté"));
  Serial.print(F(" | alarme : "));
  Serial.print(nomAlarme(alarme));
  Serial.print(F(" | dernier ID : "));
  Serial.println(dernierIdTraite);

  if (nbImagesEnvoyees > 0 || nbEchecsImage > 0) {
    Serial.print(F("[IMG] envoyées : "));
    Serial.print(nbImagesEnvoyees);
    Serial.print(F(" | échecs : "));
    Serial.print(nbEchecsImage);
    Serial.print(F(" | total moyen : "));
    Serial.print(nbImagesEnvoyees ? dureeTotaleImagesMs / nbImagesEnvoyees : 0);
    Serial.print(F(" ms | total max : "));
    Serial.print(dureeMaxImageMs);
    Serial.print(F(" ms | capture max : "));
    Serial.print(dureeMaxCaptureMs);
    Serial.println(F(" ms"));
  }
  afficherDurees();
  afficherMemoire(F("en fonctionnement"));
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println(F("=== Smart House — test MQTT v5.5 (TLS, port 8883) ==="));
  afficherMemoire(F("au démarrage (rien d'initialisé)"));

  cameraOk = initialiserCamera();
  afficherMemoire(F("après la caméra"));
  connecterWifi();
  afficherMemoire(F("après le Wi-Fi"));
  connecterMqtt();
  afficherMemoire(F("après MQTT + TLS"));
  dernierEssaiMqtt = millis();
}

void loop() {
  const unsigned long maintenant = millis();
  unsigned long t0;

  t0 = millis();
  bool wifiOk = (WiFi.status() == WL_CONNECTED);
  noterMax(&maxStatutWifiMs, t0);
  if (!wifiOk) {
    Serial.println(F("[WIFI] Connexion perdue, reconnexion..."));
    connecterWifi();
  }

  t0 = millis();
  bool mqttOk = mqttClient.connected();
  noterMax(&maxConnectedMs, t0);

  if (mqttOk) {
    t0 = millis();
    mqttClient.poll();
    noterMax(&maxPollMs, t0);
  } else if (maintenant - dernierEssaiMqtt >= PERIODE_RECONNEXION_MS) {
    dernierEssaiMqtt = maintenant;
    connecterMqtt();
  }

  if (commandeEnAttente) {
    commandeEnAttente = false;
    traiterCommande();
  }

  bool mqttOk2 = false;
  if (diffusion) {
    t0 = millis();
    mqttOk2 = mqttClient.connected();
    noterMax(&maxConnectedMs, t0);
  }
  if (diffusion && mqttOk2 &&
      maintenant - dernierEnvoiImage >= CADENCE_IMAGE_MS) {
    dernierEnvoiImage = maintenant;
    if (!envoyerImageCamera()) nbEchecsImage++;
  }

  if (maintenant - dernierEtat >= PERIODE_ETAT_MS) {
    dernierEtat = maintenant;
    afficherEtat();
  }

  noterMax(&maxBoucleMs, maintenant);
  noterMemoire();
}

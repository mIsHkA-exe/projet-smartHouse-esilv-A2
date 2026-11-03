# Arduino — Smart House

Firmware de la carte et croquis de test des composants.

> **Version :** v1 — 03/11/2026 · **Responsable :** discussion `Electronics`
> Toute modification du brochage doit être signalée : la discussion `NetSec` en dépend.

---

## 1. Matériel

| Élément | Référence | Remarque |
|---|---|---|
| Carte | Arduino UNO R4 WiFi (ABX00087) | Renesas RA4M1 + module Wi-Fi ESP32-S3, logique 5 V |
| Capteur d'ambiance | Adafruit BME680 STEMMA QT | I²C, adresse 0x77 |
| Caméra | Arducam Mini 5MP Plus (B0068, OV5642) | SPI + I²C, carte marquée UC-367 Rev.G |
| Détection | HC-SR04 | Logique 5 V |
| Fenêtre | Servomoteur FEETECH FS90 | Positionnel, **pas** FS90R |
| Alerte sonore | Buzzer actif 5 V | Transistor obligatoire, voir § 6 |
| Alerte visuelle | LED RGB à cathode commune, LED rouge 5 mm | Résistances série obligatoires |

Alimentation : **port USB-C du PC uniquement**, aucune alimentation externe.

---

## 2. Préparation de l'IDE

### 2.1 Paquet de cartes

Outils → Type de carte → Gestionnaire de cartes → chercher `UNO R4` → installer **Arduino UNO R4 Boards** (`arduino:renesas_uno`).

Windows demande ensuite l'installation d'un pilote USB signé **Arduino SA** : l'accepter. La UNO R4 WiFi utilise une liaison USB native et le port n'apparaît pas sans ce pilote.

Sélectionner ensuite **Arduino UNO R4 WiFi**, puis le port.

> **Contrôle :** après compilation, la taille maximale affichée doit être de **262144 octets** pour le programme et **32768 octets** pour la mémoire dynamique. Si l'IDE affiche 32256 et 2048, c'est une carte Uno R3 qui est sélectionnée, et le téléversement échouera avec `not in sync: resp=0x00`.

### 2.2 Bibliothèques du gestionnaire

Outils → Gérer les bibliothèques :

| Bibliothèque | Rôle |
|---|---|
| `Adafruit BME680 Library` | Pilote du capteur d'ambiance |
| `Adafruit Unified Sensor` | Dépendance de la précédente |
| `Adafruit BusIO` | Dépendance de la précédente |

L'IDE propose d'installer les deux dépendances automatiquement : accepter.

`Wire`, `SPI`, `Servo` et `WiFiS3` sont fournies avec le paquet de cartes : rien à installer.

### 2.3 Bibliothèque ArduCAM — installation manuelle et correctifs

La bibliothèque ArduCAM n'est pas dans le gestionnaire, **et elle ne prend pas en charge l'architecture Renesas de la UNO R4**. Deux correctifs sont nécessaires.

> ⚠️ Ces correctifs sont appliqués dans votre dossier `libraries/`, **hors de ce dépôt**. Une mise à jour de la bibliothèque les écrase : il faut alors les réappliquer.

**Installation**

1. Télécharger le dépôt <https://github.com/ArduCAM/Arduino> (bouton « Code » → « Download ZIP »).
2. Copier **uniquement le dossier `ArduCAM`** dans `Documents/Arduino/libraries/`.

Version utilisée pour les tests validés : dépôt officiel récupéré le **22/10/2026** (branche `master`).

**Correctif 1 — `libraries/ArduCAM/ArduCAM.h`**

Sans ce bloc, la compilation échoue sur `'regtype' does not name a type` et `'regsize' does not name a type`.

Ajouter, **juste après** le bloc `#if defined(ESP32) ... #endif` :

```cpp
// --- Smart House : support Arduino UNO R4 (Renesas RA4M1) ---
// La bibliotheque ne couvre pas cette architecture. Bloc calque sur
// celui de l'ESP32 : CS pilote par digitalWrite, B_CS = numero de broche.
#if defined(ARDUINO_ARCH_RENESAS)
	#define cbi(reg, bitmask) digitalWrite(bitmask, LOW)
	#define sbi(reg, bitmask) digitalWrite(bitmask, HIGH)
	#define regtype volatile uint32_t
	#define regsize uint32_t
#endif
```

Les macros `swap`, `fontbyte`, `cport`, `sport`, `pulse_high` et `pulse_low` ne sont pas utilisées par `ArduCAM.cpp` : elles ne sont volontairement pas redéfinies, pour éviter un conflit de nom.

**Correctif 2 — `libraries/ArduCAM/ArduCAM.cpp`, ligne 133**

Remplacer :

```cpp
		#if (defined(ESP8266)||defined(ESP32)||defined(TEENSYDUINO) ||defined(NRF52840_XXAA))
```

par :

```cpp
		#if (defined(ESP8266)||defined(ESP32)||defined(TEENSYDUINO) ||defined(NRF52840_XXAA)||defined(ARDUINO_ARCH_RENESAS))
```

> Ce second correctif est **indispensable** et sa omission ne produit aucune erreur de compilation. Sans lui, `B_CS` reçoit un masque de bits au lieu du numéro de broche, la ligne CS n'est jamais pilotée, et le test du bus SPI échoue sans explication.

**Configuration — `libraries/ArduCAM/memorysaver.h`**

À l'étape 1 du fichier, une seule ligne doit être décommentée :

```cpp
#define OV5642_MINI_5MP_PLUS
```

Toutes les autres lignes de cette étape, y compris `ARDUCAM_SHIELD_V2`, doivent rester commentées : notre matériel est un module Mini, pas un shield.

---

## 3. Contenu du dossier

| Chemin | Rôle |
|---|---|
| `smart_house/smart_house.ino` | Croquis principal, boucle non bloquante |
| `smart_house/pins.h` | **Brochage et contraintes matérielles.** Propriété d'`Electronics` |
| `smart_house/arduino_secrets.h.example` | Modèle des identifiants, sans valeurs réelles |
| `bme680_test/` | Test du capteur d'ambiance : scan I²C et lecture des 4 grandeurs |
| `arducam_test/` | Test des bus SPI et I²C de la caméra, sans capture |
| `arducam_capture/` | Capture JPEG 320 × 240 et transfert en hexadécimal |
| `hcsr04_test/` | Mesure de distance et évaluation de la stabilité |
| `servo_test/` | Test du servo en 4 phases, avec détection de redémarrage |
| `integration_test/` | Les quatre composants ensemble, Wi-Fi actif |
| `tools/hex_to_jpg.py` | Reconstitution d'une image sur le PC |

Contrainte de l'IDE : chaque croquis vit dans un dossier portant le même nom que son fichier `.ino`.

---

## 4. Lancer un croquis de test

1. Ouvrir le `.ino`, sélectionner **Arduino UNO R4 WiFi** et le port.
2. Téléverser.
3. Ouvrir le moniteur série, réglé sur **115200 bauds**.

Les croquis de test se lancent **un composant à la fois**, les autres débranchés, sauf `integration_test` qui les réunit.

### Reconstituer une image de la caméra

Après `arducam_capture`, copier tout l'affichage du moniteur série dans un fichier texte, puis :

```
python tools/hex_to_jpg.py capture.txt
```

Le script n'utilise que la bibliothèque standard de Python. Il ne garde que ce qui se trouve entre les marqueurs `---DEBUT IMAGE---` et `---FIN IMAGE---`, et refuse un fichier dont l'en-tête JPEG est absent.

---

## 5. Identifiants

**Aucun identifiant réel ne doit figurer dans ce dépôt.** Les valeurs réelles vont dans `smart_house/arduino_secrets.h`, qui est listé dans `.gitignore` et n'est jamais commité. Seul `arduino_secrets.h.example` est publié.

Un secret poussé une fois reste dans l'historique Git, même après suppression.

Les croquis de test n'utilisent aucun identifiant. `integration_test` active le Wi-Fi par un **scan des réseaux**, qui allume le module et émet des trames sans connexion ni mot de passe.

---

## 6. Contraintes matérielles à connaître

Ces points ont tous été constatés sur le matériel réel. Les ignorer provoque des pannes difficiles à diagnostiquer.

### Ne jamais utiliser `while (!Serial)`

La liaison USB de la R4 est native : le port disparaît puis réapparaît après téléversement. Cette attente **bloque le démarrage** tant qu'aucun moniteur série n'est rattaché, donc la maquette ne démarrerait pas sans PC. Utiliser un délai fixe :

```cpp
Serial.begin(115200);
delay(3000);
```

### Servo : toujours préciser la plage d'impulsion

```cpp
servo.attach(PIN_SERVO, SERVO_US_MIN, SERVO_US_MAX);  // 900, 2100
```

Sans ces deux valeurs, la bibliothèque `Servo` utilise 544 à 2400 µs par défaut, pousse le FS90 **au-delà de ses butées mécaniques** et le place en blocage permanent, à 700 mA contre 100 mA en mouvement libre.

Appeler `detach()` au repos : le servo ne consomme alors plus rien entre deux manœuvres.

### Limite de courant par broche : 8 mA

C'est quatre fois moins que sur une Uno R3. Conséquences :

- **Une LED ne se branche jamais sans résistance série.** C'est le seul cas où l'absence d'un composant peut endommager la carte.
- Le buzzer actif consomme jusqu'à 30 mA : **un transistor est obligatoire**, ce n'est pas optionnel.

### Point non tranché : conflit de timers

Sur les cartes AVR, l'usage de la bibliothèque `Servo` désactive le PWM de certaines broches. Aucune documentation officielle n'a été trouvée pour l'architecture Renesas.

À vérifier dès que la LED RGB sera disponible : ses couleurs doivent varier normalement pendant que le servo est attaché. Si le PWM est désactivé sur D3, D5 ou D6, le brochage de `pins.h` devra être modifié.

Autre point constaté sur le cœur Renesas : une `digitalWrite()` après une `analogWrite()` sur la même broche peut rester sans effet. Pour la LED RGB, toujours utiliser `analogWrite()`.

---

## 7. Résultats de référence

Valeurs relevées pendant la campagne de tests, utiles pour comparer en cas de panne.

| Mesure | Valeur de référence |
|---|---|
| Adresses sur le bus I²C | 0x3C (caméra) et 0x77 (BME680) |
| Registre de test de l'ArduChip | Écrire 0x55, relire 0x55 |
| Identifiant du capteur OV5642 | 0x56 puis 0x42 |
| Taille d'une image JPEG 320 × 240 | 6 000 à 8 500 octets |
| Bruit du HC-SR04 sur cible immobile | Écart min-max de 0,0 à 1,6 cm |
| Occupation d'`integration_test` | 85 420 o de flash (32 %), 10 572 o de SRAM (32 %) |

---

## 8. En cas de problème

| Symptôme | Cause probable |
|---|---|
| `not in sync: resp=0x00`, maximum 32256 octets | Type de carte Uno R3 au lieu de UNO R4 WiFi |
| `'regtype' does not name a type` | Correctif 1 non appliqué (§ 2.3) |
| Test SPI de la caméra échoue sans erreur de compilation | Correctif 2 non appliqué (§ 2.3) |
| `Decommenter #define OV5642_MINI_5MP_PLUS` | `memorysaver.h` non configuré (§ 2.3) |
| Moniteur série vide après téléversement | Appuyer une fois sur RESET, moniteur ouvert |
| Le port n'apparaît pas | Double appui rapide sur RESET |
| BME680 trouvé à 0x76 au lieu de 0x77 | Pastille « Addr » pontée au dos de la carte |

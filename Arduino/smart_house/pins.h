/*
  pins.h — Smart House
  Chemin dans le depot : Arduino/smart_house/pins.h

  Brochage, contraintes materielles et constantes de temps de la carte
  Arduino UNO R4 WiFi.

  PROPRIETAIRE : discussion Electronics (D28).
  Toute modification de ce fichier doit etre signalee a Brain par une
  fiche de synchronisation : NetSec en depend pour le firmware.

  Ce fichier ne contient AUCUNE logique de fonctionnement, AUCUNE
  communication et AUCUN identifiant. Les identifiants reels vont dans
  arduino_secrets.h, qui n'est jamais commite (R23).

  Statuts employes dans les commentaires :
    VALIDATED  verifie sur le materiel reel
    PROPOSED   propose, pas encore cable ni verifie
    TO VERIFY  connu mais non confirme

  ---------------------------------------------------------------------
  Version : v4 — 25/11/2026
    - CORRECTION IMPORTANTE, signalee par NetSec : ajout du rejet des
      lectures de butee du HC-SR04 (§ 3). Sans lui, la butee basse de
      3,7 cm passait le filtre d'ecart et declenchait une FAUSSE
      INTRUSION. L'ordre des deux filtres est desormais impose.
  Version : v3 — 22/11/2026 (retrait de la LED RGB)
  Version : v2 — 17/11/2026 (retrait du servomoteur, D32)
  Version : v1 — 01/11/2026
  ---------------------------------------------------------------------
*/

#ifndef PINS_H
#define PINS_H

/* =====================================================================
   1. CARTE ET ALIMENTATION
   ---------------------------------------------------------------------
   Arduino UNO R4 WiFi (ABX00087)
     - Renesas RA4M1, Cortex-M4 48 MHz, 256 ko flash, 32 ko SRAM
     - module Wi-Fi ESP32-S3-MINI-1-N8
     - logique 5 V
   Alimentation : port USB-C du PC uniquement (D30).        VALIDATED

   CELLULE SOLAIRE (Nedis SOLCH10WT, recue le 22/11) : NON RACCORDEE.
   Dimensionnement qui le justifie :
     - panneau   : 3 W, 5,3 V / 0,5 A, soit 500 mA en plein soleil ;
     - besoin    : jusqu'a 640 mA (camera 300 mA + Wi-Fi 340 mA) ;
     - interieur : 1 a 5 % du nominal, soit 5 a 25 mA ;
     - aucun stockage : la moindre variation de lumiere ferait
       redemarrer la carte.
   => insuffisant d'un facteur 20 au minimum.               VALIDATED
   ===================================================================== */

/* Courant maximal continu par broche d'entree-sortie, en mA.
   Fiche officielle ABX00087.                              VALIDATED
   ATTENTION : c'est 8 mA sur la R4, contre 40 mA sur une Uno R3. Les
   montages « sans transistor » valables sur R3 ne le sont PAS ici. */
#define LIMITE_COURANT_BROCHE_MA   8

/* L'USB natif de la R4 fait disparaitre puis reapparaitre le port apres
   televersement. Une attente « while (!Serial) » BLOQUE le demarrage
   tant qu'aucun moniteur serie n'est rattache.             VALIDATED */
#define DELAI_DEMARRAGE_SERIE_MS   3000

/* =====================================================================
   2. BUS PARTAGES
   ---------------------------------------------------------------------
   I2C : le BME680 et le capteur de la camera cohabitent sur le meme
   bus, sans conflit d'adresse.                             VALIDATED
   SPI : utilise par la camera seule.                       VALIDATED
   ===================================================================== */

#define PIN_I2C_SDA                A4
#define PIN_I2C_SCL                A5

#define PIN_SPI_MOSI               11
#define PIN_SPI_MISO               12
#define PIN_SPI_SCK                13

/* =====================================================================
   3. CAPTEURS
   ===================================================================== */

/* --- BME680 Adafruit STEMMA QT -------------------------- VALIDATED --
   Cablage : VIN -> 5V, GND -> GND, SDI -> A4, SCK -> A5.
   SDO, CS et 3Vo NON connectees :
     - 3Vo est une SORTIE du regulateur 3,3 V : ne rien y injecter ;
     - SDO est tiree au niveau haut sur la carte, ce qui fixe 0x77 ;
     - CS est maintenue haute, ce qui force le mode I2C. */
#define ADDR_I2C_BME680            0x77

/* UNITES RENVOYEES PAR LA BIBLIOTHEQUE Adafruit_BME680    VALIDATED
   (verifiees sur les releves reels du 22/10)

     bme.temperature       degres Celsius
     bme.humidity          pourcentage d'humidite relative
     bme.pressure          PASCALS        -> diviser par 100 pour hPa
     bme.gas_resistance    OHMS           -> diviser par 1000 pour kOhm

   Exemple reel : bme.pressure = 100464 soit 1004,64 hPa ;
   bme.gas_resistance = 92250 soit 92,25 kOhm.

   Le LIEU de la conversion (carte ou serveur) releve du contrat MQTT,
   donc de NetSec. Electronics signale seulement que les valeurs brutes
   sont des ENTIERS exacts, alors que les valeurs converties sont des
   flottants arrondis : une conversion faite sur la carte est
   irreversible cote serveur.

   La valeur de gaz est une RESISTANCE, pas une concentration. Elle
   derive longuement apres la mise sous tension : voir le croquis
   Arduino/gas_warmup_test/ qui mesure cette duree. */

/* --- Camera Arducam Mini 5MP Plus (B0068, OV5642) -------- VALIDATED --
   Cablage : CS -> D10, MOSI -> D11, MISO -> D12, SCK -> D13,
             SDA -> A4, SCL -> A5, VCC -> 5V, GND -> GND.
   Entrees-sorties tolerantes 5 V (fabricant), confirme en pratique.
   L'image reste dans le tampon de 8 Mo DE LA CAMERA : elle se lit par
   blocs et n'occupe jamais la SRAM en entier.
   La bibliotheque ArduCAM exige deux correctifs pour l'architecture
   Renesas ; ils sont hors du depot (voir Arduino/readme.md). */
#define PIN_CAMERA_CS              10
#define ADDR_I2C_CAMERA            0x3C   /* 0x78/0x79 en notation 8 bits */

/* TAILLE D'UNE IMAGE JPEG 320x240 — NE PAS S'EN SERVIR POUR VALIDER
   UNE IMAGE.
     Electronics, scenes variees : 3 917 a 8 008 octets
     Electronics, scene sombre   : 3 128 a 3 192 octets
     NetSec,      piece sombre   : 2 800 a 3 600 octets  (C17)
   La taille depend entierement du detail de la scene. Controler
   l'integrite par les marqueurs FF D8 et FF D9, jamais par un seuil. */

#define DELAI_AVANT_CAPTURE_MS     1000

/* --- Capteur a ultrasons HC-SR04 ------------------------- VALIDATED --
   Detection d'intrusion (D01). Logique 5 V : aucune adaptation de
   niveau. Aucune bibliotheque externe.
   Faisceau d'environ 15 degres. Les surfaces obliques renvoient l'echo
   hors du capteur ; les tissus et mousses l'absorbent. */
#define PIN_ULTRASON_TRIG          7
#define PIN_ULTRASON_ECHO          8

#define ULTRASON_TIMEOUT_US        30000UL
#define ULTRASON_DELAI_MESURE_MS   60
#define VITESSE_SON_CM_US          0.0343f

/* -- REJET DES LECTURES DE BUTEE ------------------------- VALIDATED --
   Signale par NetSec le 25/11. Quand le capteur perd sa cible, il ne
   renvoie pas « rien » : il renvoie l'une de ses deux butees.

     butee basse : 3,7 cm   (215 us, valeur repetee a l'identique)
     butee haute : 386 a 393 cm

   Ce ne sont PAS des mesures. Releves de reference a l'appui :
     - plus grande distance REELLE observee ..... 278,1 cm
     - artefacts hauts observes ................. 386,2 / 390,1 /
                                                  392,5 / 392,6 cm
   Les seuils ci-dessous laissent donc un large ecart de part et
   d'autre. La fiche du HC-SR04 annonce par ailleurs 2 cm de distance
   minimale : en deca, aucune mesure n'est fiable.

   >> ORDRE DES FILTRES, IMPOSE — ne pas inverser :
      1. ecarter d'abord toute lecture hors [MIN ; MAX] ;
      2. calculer ENSUITE mediane et ecart sur ce qui reste ;
      3. s'il ne reste AUCUNE lecture, c'est une absence de mesure,
         JAMAIS une detection.

   Raison : la butee basse de 3,7 cm se repete a l'identique, donc son
   ecart vaut 0,0 cm. Elle passait le filtre d'ecart et tombait sous le
   seuil de detection : elle declenchait une FAUSSE INTRUSION. */
#define ULTRASON_ARTEFACT_MIN_CM       4.0f
#define ULTRASON_ARTEFACT_MAX_CM       385.0f

/* -- Criteres de detection -------------------------------- PROPOSED --
   Etablis par Electronics a partir de la caracterisation du bruit ;
   LA DECISION D'USAGE APPARTIENT A NETSEC.

   Cible immobile : ecart min-max de 0,0 a 1,6 cm.
   Objet en mouvement : 3,4 a 7,8 cm. Parasites : 356 a 389 cm.
   A 13,25 cm (cible fixe de la maquette), bruit de +/- 0,4 cm.
   A REPRENDRE une fois la maquette assemblee.

   Limite connue du rejet de butee : un objet colle au capteur, a moins
   de 4 cm, serait ecarte et donc NON detecte. En pratique il traverse
   la zone 11 -> 5 cm en approchant, ou la detection a deja eu lieu. */
#define ULTRASON_MESURES_PAR_CYCLE     5
#define ULTRASON_ECART_MAX_CM          2.0f   /* au-dela : cycle rejete  */
#define ULTRASON_SEUIL_DETECTION_CM    11.0f  /* en deca : intrusion     */

/* =====================================================================
   4. ACTIONNEURS
   ---------------------------------------------------------------------
   Il n'y a plus de servomoteur (D32) ni de LED RGB (22/11).

   >> POINT A TRANCHER PAR BRAIN
      D33 etablit que la LED RGB est la SEULE reaction a une temperature
      hors consigne. Elle est retiree : il ne reste donc AUCUNE reaction
      thermique sur la maquette.

   Exclusivite D16 : le BUZZER et la CAMERA ne sont jamais actifs en
   meme temps.
   ===================================================================== */

/* --- Buzzer actif ------------------------------------ VALIDATED/PROP --
   Type ACTIF confirme sur la piece reelle (D05).           VALIDATED
   Courant de la fiche du format presume TMB12A05 : jusqu'a 30 mA,
   soit pres de quatre fois la limite de 8 mA.              TO VERIFY

   => TRANSISTOR OBLIGATOIRE sur une R4.

   Etat des pieces au 25/11 :
     resistance de base 1 kOhm ... DISPONIBLE
     transistor NPN .............. MANQUANT  <- seul blocage
     diode 1N4148 ................ manquante (recommandee)

   Montage, des que le transistor sera la :                 PROPOSED
     buzzer + -> 5V
     buzzer - -> collecteur d'un transistor NPN
     emetteur -> GND
     base     -> resistance 1 kOhm -> PIN_BUZZER
     diode 1N4148 en parallele du buzzer, cathode cote 5V
   Le montage N'INVERSE PAS le signal : niveau haut = buzzer actif. */
#define PIN_BUZZER                 2
#define BUZZER_BASE_OHMS           1000

/* --- LED rouge exterieure -------------------------------- VALIDATED --
   Alerte intrusion, poteau avant gauche (D14, M7).
   Cablage : anode -> resistance 1 kOhm -> D4, cathode -> GND.
   Resistance serie OBLIGATOIRE.
   1 kOhm donne environ 3,0 mA : clignotement confirme visuellement le
   22/11, bien visible. */
#define PIN_LED_ROUGE              4
#define LED_ROUGE_OHMS             1000

/* =====================================================================
   5. BROCHES RESERVEES ET LIBRES
   ---------------------------------------------------------------------
   D0, D1       : liaison serie, ne pas utiliser.
   D3, D5, D6   : LIBRES (LED RGB retiree). Broches PWM.
   D9           : LIBRE (servomoteur retire, D32). PWM.
   D13          : SPI SCK, partagee avec la LED integree de la carte.
   A0..A3       : libres.
   ===================================================================== */

/* =====================================================================
   6. POINT NON TRANCHE
   ---------------------------------------------------------------------
   Sur le coeur Renesas, une digitalWrite() apres une analogWrite() sur
   la meme broche peut rester sans effet. Sans objet tant qu'aucune
   sortie PWM n'est utilisee.                               TO VERIFY
   ===================================================================== */

#endif  /* PINS_H */

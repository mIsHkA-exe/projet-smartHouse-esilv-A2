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
  Version : v3 — 22/11/2026
    - LED RGB RETIREE du projet (decision utilisateur du 22/11).
      PIN_LED_RGB_* et LED_RGB_*_OHMS supprimees. D3, D5, D6 libres.
      >> CONFLIT AVEC D33 : la LED RGB etait la SEULE reaction a une
         temperature hors consigne. Il n'en reste plus aucune.
         A trancher par Brain.
    - LED rouge : resistance 680 -> 1000 ohms (valeur reellement
      disponible). Courant 4,4 -> 3,0 mA. Moins lumineux, sans risque.
    - Buzzer : la resistance de base de 1 kOhm est DISPONIBLE. Seul le
      transistor manque encore.
    - Alimentation : cellule solaire recue mais NON RACCORDEE (§ 1).
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
   Elle est montee sur la maquette comme element de la chaine d'energie,
   sans alimenter la carte. Dimensionnement qui le justifie :
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
   tant qu'aucun moniteur serie n'est rattache : la maquette ne
   demarrerait pas sans PC. Utiliser un delai fixe.         VALIDATED */
#define DELAI_DEMARRAGE_SERIE_MS   3000

/* =====================================================================
   2. BUS PARTAGES
   ---------------------------------------------------------------------
   I2C : le BME680 et le capteur de la camera cohabitent sur le meme
   bus, sans conflit d'adresse. Verifie en fonctionnement simultane.
                                                            VALIDATED
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

   La valeur de gaz est une RESISTANCE, pas une concentration. Elle
   derive pendant plusieurs minutes apres la mise sous tension. Son
   interpretation releve de NetSec. */

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

   Mesures Electronics (scenes variees)  : 3 917 a 8 008 octets
   Mesures NetSec     (piece sombre)     : 2 800 a 3 600 octets  (C17)

   La taille depend entierement du detail de la scene. Il n'existe donc
   PAS de plage attendue, et aucune constante de taille n'est fournie.
   Le seul controle valable sur le tampon est celui retenu par NetSec :
   longueur nulle, ou superieure a la capacite plausible. */

#define DELAI_AVANT_CAPTURE_MS     1000

/* --- Capteur a ultrasons HC-SR04 ------------------------- VALIDATED --
   Detection d'intrusion (D01). Logique 5 V, comme la carte : aucune
   adaptation de niveau. Aucune bibliotheque externe.
   Faisceau d'environ 15 degres. Les surfaces obliques renvoient l'echo
   hors du capteur ; les tissus et mousses l'absorbent. */
#define PIN_ULTRASON_TRIG          7
#define PIN_ULTRASON_ECHO          8

#define ULTRASON_TIMEOUT_US        30000UL
#define ULTRASON_DELAI_MESURE_MS   60
#define VITESSE_SON_CM_US          0.0343f

/* Quand il perd sa cible, le capteur renvoie l'une de ses deux butees :
   environ 3,7 cm ou 386 a 392 cm. Artefacts, pas des mesures. */

/* -- Criteres de detection -------------------------------- PROPOSED --
   Etablis par Electronics a partir de la caracterisation du bruit ;
   LA DECISION D'USAGE APPARTIENT A NETSEC.

   Cible immobile : ecart min-max de 0,0 a 1,6 cm.
   Objet en mouvement : 3,4 a 7,8 cm. Parasites : 356 a 389 cm.
   A 13,25 cm (cible fixe de la maquette), bruit de +/- 0,4 cm.
   A REPRENDRE une fois la maquette assemblee. */
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
      thermique sur la maquette. Trois issues possibles :
        - modifier D33 ;
        - confier ce role a la LED rouge ;
        - faire de la temperature un simple releve envoye au serveur.

   Exclusivite D16 : le BUZZER et la CAMERA ne sont jamais actifs en
   meme temps.
   ===================================================================== */

/* --- Buzzer actif ------------------------------------ VALIDATED/PROP --
   Type ACTIF confirme sur la piece reelle : commande en tout ou rien
   (D05).                                                   VALIDATED
   Courant de la fiche du format presume TMB12A05 : jusqu'a 30 mA,
   soit pres de quatre fois la limite de 8 mA.              TO VERIFY

   => TRANSISTOR OBLIGATOIRE sur UNE R4. Un montage direct, valable sur
      une Uno R3 (40 mA par broche), ne l'est PAS ici.

   Etat des pieces au 22/11 :
     resistance de base 1 kOhm ... DISPONIBLE
     transistor NPN .............. MANQUANT  <- seul blocage
     diode 1N4148 ................ manquante (recommandee, le buzzer
                                   actif integre son propre circuit qui
                                   amortit en partie les surtensions)

   Montage, des que le transistor sera la :                 PROPOSED
     buzzer + -> 5V
     buzzer - -> collecteur d'un transistor NPN
     emetteur -> GND
     base     -> resistance 1 kOhm -> PIN_BUZZER
     diode 1N4148 en parallele du buzzer, cathode cote 5V
   Avec 1 kOhm, la broche fournit environ 4,3 mA : sous la limite, et
   suffisant pour saturer largement un NPN courant.
   Le montage N'INVERSE PAS le signal : niveau haut = buzzer actif.

   D2 reste reservee : rien a modifier le jour de l'ajout. */
#define PIN_BUZZER                 2
#define BUZZER_BASE_OHMS           1000

/* --- LED rouge exterieure -------------------------------- VALIDATED --
   Alerte intrusion, poteau avant gauche (D14, M7).
   Cablage : anode -> resistance 1 kOhm -> D4, cathode -> GND.

   Resistance serie OBLIGATOIRE : sans elle, le courant n'est limite
   que par la broche.

   1 kOhm donne (5 - 2,0) / 1000 = environ 3,0 mA : moins lumineux
   qu'avec 680 ohms (4,4 mA), mais visible et sans risque. La tension
   de seuil reelle n'est pas mesurable tant que le multimetre manque
   (U43) : valeur a ajuster a vue si besoin. */
#define PIN_LED_ROUGE              4
#define LED_ROUGE_OHMS             1000

/* =====================================================================
   5. BROCHES RESERVEES ET LIBRES
   ---------------------------------------------------------------------
   D0, D1       : liaison serie, ne pas utiliser.
   D3, D5, D6   : LIBRES depuis le retrait de la LED RGB (22/11).
                  Broches PWM.
   D9           : LIBRE depuis le retrait du servomoteur (D32). PWM.
   D13          : SPI SCK, partagee avec la LED integree de la carte.
   A0..A3       : libres.
   ===================================================================== */

/* =====================================================================
   6. POINT NON TRANCHE
   ---------------------------------------------------------------------
   Le conflit de timers Servo / PWM (R13) est sans objet : plus de
   servomoteur, et plus de LED RGB a piloter en PWM.

   Reste valable si une sortie PWM est un jour utilisee : sur le coeur
   Renesas, une digitalWrite() apres une analogWrite() sur la meme
   broche peut rester sans effet.                           TO VERIFY
   ===================================================================== */

#endif  /* PINS_H */

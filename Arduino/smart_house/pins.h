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

  Version : v1 — 01/11/2026
*/

#ifndef PINS_H
#define PINS_H

/* =====================================================================
   1. CARTE
   ---------------------------------------------------------------------
   Arduino UNO R4 WiFi (ABX00087)
     - Renesas RA4M1, Cortex-M4 48 MHz, 256 ko flash, 32 ko SRAM
     - module Wi-Fi ESP32-S3-MINI-1-N8
     - logique 5 V
   Alimentation : port USB-C du PC uniquement, aucune alimentation
   externe (D30).                                          VALIDATED
   ===================================================================== */

/* Courant maximal continu par broche d'entree-sortie, en mA.
   Fiche officielle ABX00087.                              VALIDATED
   Toute charge au-dela de cette valeur exige un etage de commande. */
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

/* Broches I2C de la carte : A4 = SDA, A5 = SCL. Elles sont implicites
   pour la bibliotheque Wire ; rappelees ici pour le cablage. */
#define PIN_I2C_SDA                A4
#define PIN_I2C_SCL                A5

/* Broches SPI de la carte. Implicites pour la bibliotheque SPI. */
#define PIN_SPI_MOSI               11
#define PIN_SPI_MISO               12
#define PIN_SPI_SCK                13

/* =====================================================================
   3. CAPTEURS
   ===================================================================== */

/* --- BME680 Adafruit STEMMA QT -------------------------- VALIDATED --
   Temperature, humidite, pression, gaz.
   Cablage : VIN -> 5V, GND -> GND, SDI -> A4, SCK -> A5.
   SDO, CS et 3Vo NON connectees :
     - 3Vo est une SORTIE du regulateur 3,3 V de la carte : ne rien y
       injecter ;
     - SDO est tiree au niveau haut sur la carte, ce qui fixe 0x77 ;
     - CS est maintenue haute, ce qui force le mode I2C.
   La valeur de gaz est une resistance en ohms, pas une concentration.
   Elle derive pendant plusieurs minutes apres la mise sous tension. */
#define ADDR_I2C_BME680            0x77

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

/* Taille d'une image JPEG 320x240, en octets. Mesuree sur 6 captures :
   de 6360 a 8008. Varie avec le detail de la scene.        VALIDATED */
#define TAILLE_IMAGE_MIN_OCTETS    6000
#define TAILLE_IMAGE_MAX_OCTETS    8500

/* Delai entre la configuration du capteur et la capture. Une seconde
   suffit : exposition correcte verifiee en pleine lumiere. VALIDATED */
#define DELAI_AVANT_CAPTURE_MS     1000

/* --- Capteur a ultrasons HC-SR04 ------------------------- VALIDATED --
   Detection d'intrusion (D01). Logique 5 V, comme la carte : aucune
   adaptation de niveau. Aucune bibliotheque externe.
   Faisceau d'environ 15 degres. Les surfaces obliques renvoient l'echo
   hors du capteur ; les tissus et mousses l'absorbent. */
#define PIN_ULTRASON_TRIG          7
#define PIN_ULTRASON_ECHO          8

/* Portee annoncee 4 m, soit un aller-retour d'environ 23 ms.
   30 ms laissent une marge sans attente inutile.           VALIDATED */
#define ULTRASON_TIMEOUT_US        30000UL

/* Delai minimal entre deux mesures, pour que l'echo precedent se soit
   eteint. Fiche HC-SR04.                                   VALIDATED */
#define ULTRASON_DELAI_MESURE_MS   60

/* Vitesse du son a 20 degres C, en cm par microseconde.
   distance_cm = duree_us * VITESSE_SON_CM_US / 2
   Varie d'environ 0,17 % par degre : le BME680 permettrait de la
   corriger si la precision le demandait.                   VALIDATED */
#define VITESSE_SON_CM_US          0.0343f

/* Quand il perd sa cible, le capteur renvoie l'une de ses deux butees :
   environ 3,7 cm ou 386 a 392 cm. Ce sont des artefacts, pas des
   mesures.                                                 VALIDATED */

/* -- Criteres de detection -------------------------------- PROPOSED --
   Etablis par Electronics a partir de la caracterisation du bruit ;
   LA DECISION D'USAGE APPARTIENT A NETSEC, qui peut les ajuster.

   Mesures sur cible immobile : ecart min-max de 0,0 a 1,6 cm.
   Objet en mouvement : 3,4 a 7,8 cm. Parasites : 356 a 389 cm.
   Aucun recouvrement entre les trois groupes.

   A la distance de la cible fixe de la maquette (environ 13,25 cm),
   le bruit mesure vaut +/- 0,4 cm.

   A REPRENDRE une fois la maquette assemblee : le materiau et
   l'orientation de la cible reelle influent sur l'echo. */
#define ULTRASON_MESURES_PAR_CYCLE     5
#define ULTRASON_ECART_MAX_CM          2.0f   /* au-dela : cycle rejete  */
#define ULTRASON_SEUIL_DETECTION_CM    11.0f  /* en deca : intrusion     */

/* =====================================================================
   4. ACTIONNEURS
   ===================================================================== */

/* --- Servomoteur de la fenetre, FEETECH FS90 ------------- VALIDATED --
   Un seul servo dans le projet (D03). Positionnel, pas FS90R.
   Cablage : signal (orange) -> D9, rouge -> 5V, marron -> GND.

   Courants (fiche FEETECH, a 4,8 V) :
     repos 5 mA, mouvement a vide 100 mA, BLOCAGE 700 mA.

   ATTENTION, POINT CRITIQUE POUR NETSEC :
   attach() doit IMPERATIVEMENT recevoir SERVO_US_MIN et SERVO_US_MAX.
   Sans ces valeurs la bibliotheque Servo utilise 544 a 2400 us par
   defaut, pousse le servo AU-DELA DE SES BUTEES MECANIQUES et le place
   en blocage permanent, a 700 mA.

     servo.attach(PIN_SERVO, SERVO_US_MIN, SERVO_US_MAX);

   detach() au repos : le servo ne consomme plus rien entre deux
   manoeuvres. Mesure d'attenuation retenue pour D30.       VALIDATED */
#define PIN_SERVO                  9
#define SERVO_US_MIN               900
#define SERVO_US_MAX               2100

/* Course utilisee pendant les tests, sans charge.          VALIDATED
   L'amplitude reelle du FS90 n'est pas tranchee : les sources annoncent
   120 degres (Pololu) ou 180 degres (autres revendeurs).   TO VERIFY
   Les angles d'ouverture et de fermeture de la fenetre restent a fixer
   avec Mod-3D, et doivent rester A L'INTERIEUR de la course. */
#define SERVO_ANGLE_CENTRE         90
#define SERVO_EXCURSION_TESTEE     30

/* --- Buzzer actif ---------------------------------- PROPOSED/VALID --
   Type ACTIF confirme sur la piece reelle : commande en tout ou rien
   (D05).                                                   VALIDATED
   Courant de la fiche du format presume TMB12A05 : jusqu'a 30 mA, soit
   pres de quatre fois la limite de 8 mA par broche.        TO VERIFY

   => TRANSISTOR OBLIGATOIRE. Ce n'est plus la condition de D05, c'est
      une certitude : aucun buzzer de ce format ne descend sous 8 mA.

   Montage propose, composants non encore disponibles :     PROPOSED
     buzzer + -> 5V
     buzzer - -> collecteur d'un transistor NPN
     emetteur -> GND
     base     -> resistance 1 kOhm -> PIN_BUZZER
     diode en parallele du buzzer, cathode cote 5V
   Avec 1 kOhm, la broche fournit environ 4,3 mA : sous la limite, et
   suffisant pour saturer largement un NPN courant.
   Le montage N'INVERSE PAS le signal : niveau haut = buzzer actif. */
#define PIN_BUZZER                 2
#define BUZZER_BASE_OHMS           1000

/* --- LED RGB a cathode commune --------------------------- PROPOSED --
   Couleur selon la temperature (D14). Pieces non recues.
   Patte commune au GND, une resistance par couleur.
   Lentille TRANSPARENTE, non diffusante : l'orange (rouge + vert) peut
   apparaitre comme deux points distincts au lieu d'une teinte melangee.
   A observer au premier essai.                             TO VERIFY

   Resistances differenciees : le vert et le bleu ont une tension de
   seuil plus elevee que le rouge, donc une resistance plus faible
   compense leur moindre luminosite. Les seuils reels ne sont pas
   mesurables (pas de multimetre) : valeurs a ajuster a vue. */
#define PIN_LED_RGB_ROUGE          3     /* PWM */
#define PIN_LED_RGB_VERT           5     /* PWM */
#define PIN_LED_RGB_BLEU           6     /* PWM */
#define LED_RGB_ROUGE_OHMS         680
#define LED_RGB_VERT_OHMS          330
#define LED_RGB_BLEU_OHMS          330

/* --- LED rouge exterieure -------------------------------- PROPOSED --
   Alerte intrusion, poteau avant gauche (D14, M7). Piece non recue.
   Resistance serie OBLIGATOIRE : sans elle, le courant n'est limite
   que par la broche. */
#define PIN_LED_ROUGE              4
#define LED_ROUGE_OHMS             680

/* =====================================================================
   5. BROCHES RESERVEES ET LIBRES
   ---------------------------------------------------------------------
   D0, D1  : liaison serie, ne pas utiliser.
   D13     : SPI SCK, partagee avec la LED integree de la carte.
   A0..A3  : libres.
   ===================================================================== */

/* =====================================================================
   6. POINT NON TRANCHE
   ---------------------------------------------------------------------
   Conflit de timers entre la bibliotheque Servo et le PWM (R13).
   Sur les cartes AVR, l'usage de Servo desactive le PWM de certaines
   broches. Aucune documentation officielle trouvee pour l'architecture
   Renesas de la UNO R4.                                    TO VERIFY

   A verifier au premier essai de la LED RGB : ses couleurs doivent
   varier normalement pendant que le servo est attache. Si le PWM est
   desactive sur D3, D5 ou D6, il faudra deplacer la LED RGB vers
   d'autres broches PWM, ce qui modifierait ce fichier.

   Autre point constate sur le coeur Renesas : une digitalWrite() apres
   une analogWrite() sur la meme broche peut rester sans effet. Pour la
   LED RGB, toujours utiliser analogWrite().                TO VERIFY
   ===================================================================== */

#endif  /* PINS_H */

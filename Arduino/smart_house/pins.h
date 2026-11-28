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
  Version : v7 — 28/11/2026
    - D33 ABROGEE : il n'y a AUCUNE reaction thermique sur la maquette.
      La temperature, l'humidite, la pression et la resistance de gaz
      restent mesurees et transmises au serveur, mais n'ont plus aucun
      effet local. Les deux blocs « POINT NON TRANCHE » de la v6, qui
      portaient sur cette question, sont donc RETIRES.
    - Correction de date : le servomoteur a ete retire le 05/11/2026
      (D32). La v2 de ce fichier, du 17/11, ne faisait qu'en prendre
      acte.
  Version : v6 — 28/11/2026 (D45, LED RGB en voyant d'etat)
  Version : v5 — 28/11/2026 (LED RGB remise, buzzer via L293D)
  Version : v4 — 25/11/2026 (rejet des butees du HC-SR04, C20)
  Version : v3 — 22/11/2026 (retrait de la LED RGB)
  Version : v2 — 17/11/2026 (prise en compte de D32 du 05/11)
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

   CELLULE SOLAIRE (Nedis SOLCH10WT) : NON RACCORDEE.
     - panneau   : 3 W, 5,3 V / 0,5 A, soit 500 mA en plein soleil ;
     - besoin    : jusqu'a 640 mA (camera 300 mA + Wi-Fi 340 mA) ;
     - interieur : 1 a 5 % du nominal, soit 5 a 25 mA ;
     - aucun stockage : la moindre variation de lumiere ferait
       redemarrer la carte.
   => insuffisant d'un facteur 20 au minimum.               VALIDATED
   ===================================================================== */

/* Courant maximal continu par broche d'entree-sortie, en mA.
   Fiche officielle ABX00087.                              VALIDATED
   ATTENTION : c'est 8 mA sur la R4, contre 40 mA sur une Uno R3. */
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
     - CS est maintenue haute, ce qui force le mode I2C.

   D33 etant abrogee, ces mesures n'ont AUCUN effet sur la maquette :
   elles sont transmises au serveur et exploitees sur le dashboard. */
#define ADDR_I2C_BME680            0x77

/* UNITES RENVOYEES PAR LA BIBLIOTHEQUE Adafruit_BME680    VALIDATED
   (verifiees sur les releves reels du 22/10)

     bme.temperature       degres Celsius
     bme.humidity          pourcentage d'humidite relative
     bme.pressure          PASCALS
     bme.gas_resistance    OHMS

   Exemple reel : bme.pressure = 100464 (soit 1004,64 hPa) ;
   bme.gas_resistance = 92250 (soit 92,25 kOhm).

   >> DECISION NETSEC du 25/11, contrat v0.9 : les valeurs sont
      transmises BRUTES, sans conversion sur la carte, sous les cles
      "pression_pa" (entier, Pa) et "gaz_ohm" (entier, Ohm).
      Les valeurs brutes sont des entiers exacts, alors qu'une
      conversion faite sur la carte serait irreversible cote serveur.

   La valeur de gaz est une RESISTANCE, pas une concentration. Elle
   derive longuement apres la mise sous tension : le croquis
   Arduino/gas_warmup_test/ mesure cette duree, mais n'a PAS ENCORE
   ETE EXECUTE. Aucune valeur de duree de chauffe n'est donc etablie. */

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
     Electronics, 28/11          : 4 520 a 4 616 octets
     NetSec,      piece sombre   : 2 800 a 3 600 octets  (C17)
   Controler l'integrite par les marqueurs FF D8 et FF D9, jamais par
   un seuil d'octets. */

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

/* -- REJET DES LECTURES DE BUTEE (C20) ------------------- VALIDATED --
   Correction du 25/11, signalee par NetSec. Quand le capteur perd sa
   cible, il ne renvoie pas « rien » : il renvoie l'une de ses deux
   butees.

     butee basse : 3,7 cm   (215 us, valeur repetee a l'identique)
     butee haute : 386 a 393 cm

   Releves de reference :
     - plus grande distance REELLE observee ..... 278,1 cm
     - artefacts hauts observes ................. 386,2 / 390,1 /
                                                  392,5 / 392,6 cm

   >> ORDRE DES FILTRES, IMPOSE — ne pas inverser :
      1. ecarter d'abord toute lecture hors [MIN ; MAX] ;
      2. calculer ENSUITE mediane et ecart sur ce qui reste ;
      3. s'il ne reste AUCUNE lecture, c'est une absence de mesure,
         JAMAIS une detection.

   Raison : la butee basse de 3,7 cm se repete a l'identique, donc son
   ecart vaut 0,0 cm. Elle passait le filtre d'ecart et tombait sous le
   seuil de detection : elle declenchait une FAUSSE INTRUSION.
   Les memes seuils sont repris cote serveur par NetSec (controle de
   coherence des deux cotes). */
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
   Il n'y a plus de servomoteur (D32, 05/11) ni de LED rouge simple.
   Il n'y a AUCUNE reaction thermique (D33 abrogee, 28/11).

   Le buzzer releve du mode ALERTER et les images du mode OBTENIR
   (D11) ; D16 interdit par ailleurs qu'ils soient actifs en meme
   temps. Electronics fournit les broches et les etages de puissance :
   l'enchainement des modes releve de NetSec (D28).
   ===================================================================== */

/* --- LED RGB, voyant d'etat (D45) ------------------------ VALIDATED --
   Decision de l'utilisateur du 28/11 :
     VERT  = fonctionnement normal
     ROUGE = detection (signalee aussi sur le dashboard)

   Cablage : rouge -> 1 kOhm -> D3 ; vert -> 1 kOhm -> D5 ;
             patte commune (cathode) -> GND.

   Le canal BLEU n'est PAS cable. Aucune constante ne lui est associee,
   pour qu'aucun croquis ne l'utilise par inadvertance. D6 est libre.

   Courants, pour une alimentation de 5 V :
     rouge (5 - 2,0) / 1000 = 3,0 mA
     vert  (5 - 3,1) / 1000 = 1,9 mA
   Tous deux sous la limite de 8 mA. Le vert sera moins lumineux que le
   rouge, sa tension de seuil etant plus elevee. Une 330 ohms les
   equilibrerait ; seules des 1 kOhm sont disponibles. Les seuils reels
   ne sont pas mesurables faute de multimetre (U43).

   Sur le coeur Renesas, une digitalWrite() apres une analogWrite() sur
   la meme broche peut rester sans effet : n'utiliser QUE
   analogWrite() sur ces deux broches.                      TO VERIFY */
#define PIN_LED_RGB_ROUGE          3     /* PWM */
#define PIN_LED_RGB_VERT           5     /* PWM */
#define LED_RGB_ROUGE_OHMS         1000
#define LED_RGB_VERT_OHMS          1000

/* --- Buzzer actif, pilote par un L293D ------------------ VALIDATED --
   Type ACTIF confirme sur la piece reelle : commande en tout ou rien
   (D05). Fonctionnement verifie le 28/11 sur 8 cycles.
   Le buzzer appartient au mode ALERTER (D11).
   Courant : jusqu'a 30 mA (fiche du format presume TMB12A05), soit
   pres de quatre fois la limite de 8 mA d'une broche.      TO VERIFY

   => UN ETAGE DE PUISSANCE EST OBLIGATOIRE. Un montage direct, valable
      sur une Uno R3 (40 mA par broche), ne l'est PAS ici.

   ETAGE RETENU : L293D (quadruple demi-pont). Il remplace le
   transistor NPN envisage avant :
     - 600 mA par canal, tres au-dela des 30 mA necessaires ;
     - entree LOGIQUE : aucune resistance de base ;
     - diodes de protection INTEGREES (le « D » de L293D).
   Le transistor NPN et la diode 1N4148 ne sont donc plus a acquerir.

   Cablage du L293D, canal 1 (patte 1 reperee par une encoche) :
     patte 16  VCC1        -> 5V    (alimentation logique)
     patte 8   VCC2        -> 5V    (alimentation des sorties)
     pattes 4, 5, 12, 13   -> GND   (LES QUATRE : aussi dissipation)
     patte 1   1,2EN       -> 5V    (canal active en permanence)
     patte 2   1A          -> PIN_BUZZER
     patte 3   1Y          -> buzzer +
     buzzer -              -> GND

   Le signal N'EST PAS INVERSE : niveau haut = buzzer actif.

   Reserves, non mesurables faute de multimetre :
     - chute de tension du L293D entre VCC2 et sa sortie, specifiee par
       TI a 600 mA seulement (1,4 a 1,8 V) ; a 30 mA elle sera bien
       plus faible. Volume juge correct a l'oreille.         TO VERIFY
     - le L293D consomme lui-meme 13 a 22 mA sur VCC1.       TO VERIFY
     - le boitier doit enjamber le canal central de la breadboard. */
#define PIN_BUZZER                 2

/* =====================================================================
   5. BROCHES RESERVEES ET LIBRES
   ---------------------------------------------------------------------
   D0, D1       : liaison serie, ne pas utiliser.
   D4           : LIBRE (LED rouge simple retiree).
   D6           : LIBRE (canal bleu de la LED RGB non cable, D45). PWM.
   D9           : LIBRE (servomoteur retire, D32). PWM.
   D13          : SPI SCK, partagee avec la LED integree de la carte.
   A0..A3       : libres.
   ===================================================================== */

#endif  /* PINS_H */

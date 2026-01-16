# JF-ECU32
RC Jet Engine ECU

Details ici : https://www.usinages.com/threads/creation-dun-ecu-opensource-pour-micro-reacteur.160245/

## Environement
- VsCode
- IDF 5.0.8

## Démarrage du projet d'ECU opensource pour réacteurs

### Fonctionnalitées :

- Démarrage GAZ/KERO
- Fonctionnement du réacteur*
- Apprentissage du moteur*
- Surveillance des paramètres
- Redémarrage en vol*
- Log
- Interface WEB
- Ecran sans fil*
- Log sur MicroSd
- Baromètre


### Composants :
- Processeur ESP32
- Interface sonde K MAX31855
- INA218
- Max31855K
- BMP280
- MicroSD

### Entrées :
- Entrée 2S - 3S pour accus LIPO ou LIION
- Entrée PPM - Gaz ou SBUS
- Entrée PPM - AUX
- Entrée compte tours IR ou HALL
- Entrée sonde K (module MAX31865)
- INA219 courant de la bougie
- BMP280 pression / altitude

### Sorties :
- Sortie PWM/PPM 10A pour le moteur de démarrage
- Sortie PWM/PPM 10A pour la pompe ou ESC brushless
- Sortie PWM/PPM 10A pour ESC de pompe numéro 2 (fumigène ou redondance, comme sur les grandeurs)
- Sorties PWM pour 2 électrovannes (KERO/GAZ - KERO/KERO) 1A
- Sortie PWM pour une bougies Glow ou Kero 10A
- Sortie FPORT FRSky/Hott/Futaba (télémétrie)*

### Parties fonctionnelles :
 
 #### Entrées
 - Lecture des RPM
 - Lecture EGT
 - Lecture courant bougie
 - Lecture tension de batterie
 - Lecture Voie des gaz et voie aux
 
 #### Sortie   
 - Pilotage starter (PPM ou PWM)
 - Pilotage bougie
 - pilotage pompe1 (PPM ou PWM)
 - pilotage pompe2 (PPM ou PWM)
 - pilotage des vannes

#### Logiciel
 - Ecriture des logs (sur Spiffs pour le moment)
 - Calcul des deltas (EGT - RPM)
 - Calibration du démareur (Récupere les mini maxi en commande (%) et en RPM)
 - Préchauffage au gaz
 - Protection batterie trop faible ou trop haute en tension
 - Refroidissement de la turbine (action du démarreur)


### Parties à coder :
  - Lecture BMP280
  - Log sur MicroSD
  - fonction de démarrage du moteur
  - fonction de pilotage du moteur
  - fonction apprentissage du moteur
  - plein d'autres choses


*en cours de codage

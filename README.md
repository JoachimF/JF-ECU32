# JF-ECU32
RC Jet Engine ECU
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


### Composants :
- Processeur ESP32
- Interface sonde K MAX31855

### Entrées :
- Entrée 2S - 3S pour accus LIPO ou LIION
- Entrée PPM - Gaz ou SBUS
- Entrée PPM - AUX
- Entrée compte tours IR ou HALL
- Entrée sonde K (module MAX31865)
- INA219 courant de la bougie

### Sorties :
- Sortie PWM/PPM 10A pour le moteur de démarrage
- Sortie PWM/PPM 10A pour la pompe ou ESC brushless
- Sortie PWM/PPM 10A pour ESC de pompe numéro 2 (fumigène ou redondance, comme sur les grandeurs)
- Sorties PWM pour 2 électrovannes (KERO/GAZ - KERO/KERO) 1A
- Sortie PWM pour une bougies Glow ou Kero 10A
- Sortie FPORT FRSky (télémétrie)*

*en cours de fabrication

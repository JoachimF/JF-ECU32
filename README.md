# JF-ECU32
RC Jet Engine ECU

## Démarrage du projet d'ECU opensource pour réacteurs

### Fonctionnalitées :

- Démarrage GAZ/KERO
- Fonctionnement du réacteur
- Surveillance des paramètres
- Redémarrage en vol


### Composants :
- Processeur ESP32
- Interface sonde K MAX31865

### Entrées :
- Entrée 2S - 3S pour accus LIPO ou LIION
- Entrée PPM - Gaz ou SBUS
- Entrée PPM - AUX
- Entrée compte tours IR ou HALL
- Entrée sonde K (module MAX6675 ou MAX31865)
- Entrée pression (à voir le capteur selon les plages de pression 2 bars max?)

### Sorties :
- Sortie PPM pour le moteur de démarrage
- Sortie PWM/PPM pour la pompe 10A ou ESC brushless
- Sortie PWM/PPM pour ESC de pompe numéro 2 (fumigène ou redondance, comme sur les grandeurs)
- Sorties pour 2 électrovannes (KERO/GAZ - KERO/KERO) 1A
- Sortie pour une bougies Glow ou Kero 10A
- Sortie FPORT FRSky (télémétrie)
- Sortie LED W2812


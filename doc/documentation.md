#Documentation

Detail du fonctionnement :

Les taches :

- Main
- Serveur HTTP
- Log
- ECU
- Inputs
- Htop (pour le debug)

Détail des taches :

- Main
Lance les autres taches

- Serveur HTTP
Gère l'interface WEB, s'arrete au bout de 60 secondes si pas de connection
Coupe le Wifi aussi

- Log
Enregistre dans un fichier téléchargeable par le Web tous les parmètres a partir de démarrage du moteur jusqu'a l'extinction

- ECU
Tache prioritaire qui gère le moteur en pilotant les sorties en fonction des entrée

- Inputs
Tache prioritaire qui lit les entrées

- Htop
Gestionnaire de tache qui donne le pourcentage d'utilisation du processeur par tache



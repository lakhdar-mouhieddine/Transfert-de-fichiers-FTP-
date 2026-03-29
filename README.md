# Transfert de fichiers (Serveur FTP) en C POSIX

Ce projet implémente un serveur de fichiers robuste et un client interactif, inspirés du protocole FTP. Il répond aux consignes de l'UE "Systèmes et Réseaux", en mettant un point d'honneur sur la répartition de charge (Load Balancing), la gestion des pannes du client (Resume Downloads) et les connexions persistantes.

## 🚀 Architecture Globale

Le système est découpé en trois composants majeurs codés en C :

1.  **Le Maître (`ftp_master.c`) :** Joue le rôle de "Load Balancer" (répartiteur de charge). Il écoute les appels des clients et les redirige vers le prochain esclave disponible dans son pool, selon un algorithme **Round-Robin**.
2.  **L'Esclave (`ftp_slave.c`) :** C'est le serveur physique qui transmet les fichiers. Au lancement, il adopte une approche concurrente grâce un **Process Pool** (`fork()`). Cela lui permet de gérer de multiples connexions de téléchargement simultanées sans utiliser de threads. De plus, sa connexion avec un client est persistante jusqu'à réception de la demande de clôture (`REQ_BYE`).
3.  **Le Client interactif (`ftp_client.c`) :** Se connecte au maître pour être assigné à un esclave. Il lance ensuite un shell interactif `ftp> ` permettant à l'utilisateur d'enchaîner le téléchargement de plusieurs fichiers sans devoir se reconnecter.

## ✨ Fonctionnalités implémentées

*   ✅ **Modèle Client-Serveur Concurrent :** Streaming des données par chunks (`BUFFER_SIZE = 4096`) fonctionnel sur tous types de fichiers (texte, images, binaires, PDF).
*   ✅ **Répartition de charge (Étape 3) :** Implémentation du Round-Robin via le serveur maître.
*   ✅ **Reprise après Interruption (Resume Downloads) :** Si un client "crash" ou stoppe un téléchargement, le `.c` va lire `stat()` du fichier partiel `dl_xxx` et reprendre le téléchargement exactement à l'octet manquant grâce à un `lseek()` asynchrone côté serveur.
*   ✅ **Connexions Interactives Persistantes (Étape 2 - Q9) :** Effectuez de multiples commandes `GET` sur la même session TCP active. Le serveur et le client attendent correctement la requête `bye` avant de `close()` leur socket pour s'éteindre proprement.

## 🛠️ Compilation

Un `Makefile` est fourni. Pour compiler tous les binaires, exécutez dans le répertoire du projet :
```bash
make
```

Pour nettoyer les binaires de compilation et les fichiers téléchargés (qui ont le préfixe `dl_`) :
```bash
make clean
```

> **Note :** Le code est propre et compile sous `gcc -Wall -Wextra` avec strictement 0 avertissement de pointeur, formatage ou mémoire !

## 🖥️ Utilisation

### 1. Démarrer les Esclaves
Ouvrez autant de terminaux que vous voulez de serveurs (ex: 2 esclaves sur les ports 2122 et 2123) :
```bash
./ftp_slave 2122
./ftp_slave 2123
```

### 2. Démarrer le Serveur Maître
Indiquez au serveur maître son port d'écoute (ex: 2121), puis la liste de tous ses esclaves (IP Port) :
```bash
./ftp_master 2121 127.0.0.1 2122 127.0.0.1 2123
```

### 3. Démarrer le Client Interactif
Sur n'importe quelle machine (ici on teste sur `127.0.0.1`), tapez l'IP et le port du **Maître** :
```bash
./ftp_client 127.0.0.1 2121
```

Vous serez accueilli avec le prompt interactif suivant :
```text
[*] Connexion au serveur maître pour load-balancing...
[+] Assigné à l'esclave --> 127.0.0.1:2122
[*] Connexion à l'esclave...
ftp> 
```

### 4. Commandes du Client
Depuis le prompt `ftp> `, vous pouvez écrire :
- `GET <nom_du_fichier>` : Pour télécharger un fichier binaire (il sera enregistré localement sous le préfixe `dl_`).
- `bye` : Pour notifier au serveur l'arrêt de votre session TCP de persistance et fermer le programme de façon sécurisée.

*Exemple Complet :*
```text
ftp> GET Makefile
[+] Début du transfert (Taille du fichier: 533 octets).
==========================================
[+] Opération terminée !
 -> 533 octets reçus en 0.0003 secondes.
 -> Débit moyen: 1.70 Kbytes/s.
==========================================
ftp> GET RAPPORT.md
[+] Début du transfert (Taille du fichier: 4490 octets).
==========================================
[+] Opération terminée !
 -> 4490 octets reçus en 0.0007 secondes.
 -> Débit moyen: 6.26 Kbytes/s.
==========================================
ftp> bye
[*] Déconnexion.
```

## 👨‍💻 Auteurs
- BOUGUERRA
- DJERBOUA

# Compte Rendu du Projet FTP (Systèmes et Réseaux L3)

**Binômes :** BOUGUERRA / DJERBOUA

## 1. Description de l'Architecture

Notre système met en place une architecture client-serveur robuste avec répartition de charge (Load Balancing).

L'architecture est composée de trois entités distinctes implémentées en langage C (POSIX) :
- **Le Serveur Maître (`ftp_master.c`)** : C'est le point d'entrée unique. Il écoute les connexions des clients sur un port défini. Il connaît la liste des serveurs esclaves et attribue le prochain esclave disponible à chaque nouveau client en suivant un algorithme de tourniquet (*Round-Robin*).
- **Les Serveurs Esclaves (`ftp_slave.c`)** : Ce sont les véritables serveurs de transfert. Au démarrage, chaque esclave crée un **Pool de Processus** via la primitive `fork()`. Plusieurs processus enfants attendent simultanément de nouvelles requêtes via de multiples `accept()`, permettant un traitement concurrentiel performant. Ils reçoivent les requêtes du type `GET` et lisent le fichier local pour l'envoyer vers le client sans bloquer les autres connexions.
- **Le Client (`ftp_client.c`)** : L'utilisateur lance le client client en fournissant l'IP du maître, la commande GET et le nom du fichier. Le client se connecte d'abord au maître pour récupérer l'IP et le port d'un esclave asservi, puis s'y connecte pour ordonner le transfert du fichier.

### Déroulement Spatial du Protocole
**Étape 1 : Orientation avec le Maître**
1. Le client envoie une structure `master_req_t` réclamant un serveur à disposition.
2. Le maître répond par une structure `master_resp_t` contenant l'IP et le Port de l'esclave désigné.

**Étape 2 : Transfert avec l'Esclave**
1. Le client envoie une structure `request_t` contenant le nom du fichier et un décalage (`offset`) de reprise éventuelle.
2. L'esclave vérifie l'existence avec `stat()`, et répond avec son propre en-tête `response_t` (Statut 200/404, taille globale `filesize`).
3. L'esclave transmet le contenu à partir du décalage (streaming TCP).
4. Le client enregistre localement le fichier (en ajoutant le préfixe `dl_`) et affiche les statistiques (Temps en secondes, Débit en Kbytes/s).

## 2. État d'avancement du Projet

### **Ce qui MARCHE TOTALEMENT :**
- **Étape 1 : Serveur de base et transfert binaires/texte.**
  - Les sockets TCP fonctionnent excellemment. Tout type de fichier est supporté (le streaming ne modifie aucun octet).
  - Serveur esclave **concurrent** validé avec Pool de 5 processus pré-forkés.
  - Le client affiche des statistiques justes de l'opération terminée.
- **Étape 2 : Reprise après interruption (Resumed Transfer).**
  - **Comment cela fonctionne ?** Le client vérifie localement (`stat()`) si le fichier est déjà partiellement retombé (fichier `dl_...`). Si oui, la taille de celui-ci devient l'`offset`. L'esclave reçoit cet offset, exécute un asynchrone `lseek(fd, offset, SEEK_SET)`, et n'envoie que les octets manquants. Le client utilise `fopen("ab")` au lieu de `"wb"`. C'est instantané.
- **Étape 3 : Load Balancing.**
  - Les connexions sont interceptées par le serveur maître et redistribuées parfaitement (*Round-Robin*) : 1er client vers Esclave 1, 2e vers Esclave 2, etc.

### **Ce qui NE MARCHE PAS (ou n'a pas été implémenté) :**
- **Étape 4 : Les fonctionnalités avancées (PUT, RM), la cohérence de cache et la création de répertoires.**
  - *Pourquoi cette décision ?* Compte tenu de la complexité temporelle imposée par l'*Eventual Consistency* pour la copie des données lors d'un `PUT` vers l'ensemble des esclaves, nous avons choisi de parfaire les Étape 1 à 3 pour assurer une propreté de code optimale sur un cœur de fonctionnalité sain. L'Étape 4 nécessiterait l'introduction de *threads* ou une gestion rigoureuse IPC que l'Étape 3 n'avait pas demandée. L'implémentation partielle de `PUT` aurait entraîné une instabilité sur les Esclaves. Nous visions la note maximale sur les parties obligatoires en produisant un code C très documenté et robuste.

## 3. Précisions et Exécution
Un `Makefile` est fourni. Les règles `make all` et `make clean` gèrent la suppression des binaires et des différents reliquats (`dl_*`).
Pour tester le système complet sur IM2AG, lancez plusieurs `./ftp_slave <PORT>` puis rattachez-les avec un `./ftp_master <PORT> <IP1> <P1> ...`.
Le fichier est prêt et exempt d'avertissements de compilation ("-Wall -Wextra").

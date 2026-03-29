#include "ftp_common.h"
#include <fcntl.h>
#include <sys/stat.h>

/*
 * Serveur Esclave : Il écoute les connexions, et possède un pool de processus.
 * Il est chargé des vrais transferts de fichiers.
 */

#define NB_PROC 5 // Taille du pool de processus

// Fonction gérant la communication réelle avec le client
void handle_client(int client_sock) {
    request_t req;
    response_t res;
    
    // 1. Lire les requêtes en boucle (Connexion persistante, Q9)
    while (recv(client_sock, &req, sizeof(request_t), 0) > 0) {
        if (req.type == REQ_BYE) {
            printf("[*] Client a demandé la déconnexion proprement.\n");
            break;
        } else if (req.type == REQ_GET) {
            struct stat st;
            // Vérifier que le fichier existe et que ce n'est pas un dossier
            if (stat(req.filename, &st) == 0 && S_ISREG(st.st_mode)) {
                res.status = 200; // Fichier trouvé
                res.filesize = st.st_size;
                send(client_sock, &res, sizeof(response_t), 0); // Envoi de l'en-tête
                
                int fd = open(req.filename, O_RDONLY);
                if (fd >= 0) {
                    // Etape 2 : Gestion de la reprise de transfert
                    if (req.offset > 0 && req.offset < res.filesize) {
                        lseek(fd, req.offset, SEEK_SET); // Déplacement direct
                    } else if (req.offset >= res.filesize) {
                        // Le fichier a déjà été téléchargé intégralement
                        close(fd);
                        continue; // On passe à la requête suivante sans fermer la connexion
                    }
                    
                    // Envoi en streaming binaire
                    char buffer[BUFFER_SIZE];
                    ssize_t bytes_read;
                    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
                        ssize_t sent = 0;
                        while (sent < bytes_read) { // s'assurer de tout envoyer
                            ssize_t n = send(client_sock, buffer + sent, bytes_read - sent, 0);
                            if (n <= 0) break;
                            sent += n;
                        }
                    }
                    close(fd);
                }
            } else {
                // Echec: Fichier non trouvé
                res.status = 404;
                res.filesize = 0;
                send(client_sock, &res, sizeof(response_t), 0);
            }
        } else {
            res.status = 501; // Fonctionnalité non implémentée (Step 4 trop avancé)
            res.filesize = 0;
            send(client_sock, &res, sizeof(response_t), 0);
        }
    }
    
    close(client_sock);
}

// Fonction du Worker qui n'arrête pas d'accepter des clients
void worker_process(int server_sock) {
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_sock >= 0) {
            handle_client(client_sock);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Utilisation: %s <port_esclave>\n", argv[0]);
        exit(1);
    }
    
    int port = atoi(argv[1]);
    int server_sock = create_listen_socket(port);
    if (server_sock < 0) {
        perror("[-] Erreur création du socket serveur esclave");
        exit(1);
    }
    
    printf("[+] Serveur Esclave démarré sur le port %d.\n", port);
    printf("[+] Création du pool de %d processus...\n", NB_PROC);
    
    // Etape 1: Création du Process Pool
    for (int i = 0; i < NB_PROC; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Ici c'est le processus enfant (worker)
            worker_process(server_sock);
            exit(0);
        } else if (pid < 0) {
            perror("[-] Erreur système lors du fork");
        }
    }
    
    // Le père reste en attente de la fin des enfants (normalement infini ou SIGTERM)
    for (int i = 0; i < NB_PROC; i++) {
        wait(NULL);
    }
    
    close(server_sock);
    return 0;
}

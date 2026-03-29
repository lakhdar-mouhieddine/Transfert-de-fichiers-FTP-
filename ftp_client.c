#include "ftp_common.h"
#include <sys/time.h>
#include <sys/stat.h>

/*
 * Programme Client : Contacte le Maître, récupère l'IP/Port d'un esclave,
 * et s'y connecte pour ordonner un transfert de fichier, gère la reprise (Étape 2) 
 * et affiche les statistiques (Étape 1).
 */

int connect_to_server(const char *ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        close(sock);
        return -1;
    }
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Utilisation: %s <ip_maitre> <port_maitre>\n", argv[0]);
        exit(1);
    }
    
    const char *master_ip = argv[1];
    int master_port = atoi(argv[2]);
    
    printf("[*] Connexion au serveur maître pour load-balancing...\n");
    // 1. Contacter le Maître pour demander "Où aller ?"
    int master_sock = connect_to_server(master_ip, master_port);
    if (master_sock < 0) {
        perror("[-] Erreur de connexion au serveur Maître");
        exit(1);
    }
    
    master_req_t mreq;
    mreq.dummy = 1;
    send(master_sock, &mreq, sizeof(master_req_t), 0);
    
    master_resp_t mres;
    if (recv(master_sock, &mres, sizeof(master_resp_t), 0) <= 0) {
        perror("[-] Erreur réponse Maître");
        close(master_sock);
        exit(1);
    }
    close(master_sock);
    
    printf("[+] Assigné à l'esclave --> %s:%d\n", mres.slave_ip, mres.slave_port);
    
    // 2. Connexion à l'esclave (Connexion persistante)
    printf("[*] Connexion à l'esclave...\n");
    int slave_sock = connect_to_server(mres.slave_ip, mres.slave_port);
    if (slave_sock < 0) {
        perror("[-] Impossible de se connecter à l'esclave");
        exit(1);
    }
    
    char input[512];
    // 3. Boucle interactive pour commandes (Question 9)
    while (1) {
        printf("ftp> ");
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        // Supprimer le saut de ligne
        input[strcspn(input, "\n")] = 0;

        if (strncmp(input, "bye", 3) == 0) {
            request_t req;
            req.type = REQ_BYE;
            send(slave_sock, &req, sizeof(request_t), 0);
            printf("[*] Déconnexion.\n");
            break;
        } else if (strncmp(input, "GET ", 4) == 0) {
            char *filename = input + 4;
            while (*filename == ' ') filename++; // ignorer espaces

            if (strlen(filename) == 0) {
                printf("[-] Erreur: Spécifiez un nom de fichier.\n");
                continue;
            }

            // Vérification locale pour Reprise du Transfert (Step 2)
            off_t local_offset = 0;
            struct stat st;
            char local_filename[MAX_FILENAME + 10];
            snprintf(local_filename, sizeof(local_filename), "dl_%.250s", filename);
            
            if (stat(local_filename, &st) == 0) {
                local_offset = st.st_size; // On reprend d'où on s'était arrêté
                printf("[!] Fichier partiel détecté. Reprise à partir de %ld octets.\n", (long)local_offset);
            }

            request_t req;
            req.type = REQ_GET;
            strncpy(req.filename, filename, MAX_FILENAME - 1);
            req.filename[MAX_FILENAME - 1] = '\0';
            req.offset = local_offset; // Décalage envoyé au serveur

            send(slave_sock, &req, sizeof(request_t), 0);

            response_t res;
            if (recv(slave_sock, &res, sizeof(response_t), 0) <= 0) {
                perror("[-] Erreur réception statut ou connexion perdue");
                break;
            }

            if (res.status == 404) {
                printf("[-] Erreur 404 : Le fichier '%s' n'existe pas chez l'esclave.\n", filename);
            } else if (res.status == 200) {
                printf("[+] Début du transfert (Taille du fichier: %ld octets).\n", (long)res.filesize);
                
                if (local_offset >= res.filesize) {
                    printf("[+] Fichier déjà complet localement.\n");
                    continue;
                }
                
                // Mode 'a' pour ajout/reprise "ab", sinon 'w' pour création nouveau "wb"
                FILE *f = fopen(local_filename, local_offset > 0 ? "ab" : "wb");
                if (!f) {
                    perror("[-] Erreur ouverture fichier local dl_xxx");
                    continue;
                }
                
                struct timeval start, end;
                gettimeofday(&start, NULL);
                
                char buffer[BUFFER_SIZE];
                ssize_t bytes_recv;
                long total_received = 0;
                long to_receive = res.filesize - local_offset;
                
                // Boucle de réception des octets EXACTS manquants (pour éviter de lire la réponse de la req suivante)
                while (total_received < to_receive) {
                    long chunk = to_receive - total_received;
                    if (chunk > BUFFER_SIZE) chunk = BUFFER_SIZE;
                    
                    bytes_recv = recv(slave_sock, buffer, chunk, 0);
                    if (bytes_recv <= 0) break;
                    
                    fwrite(buffer, 1, bytes_recv, f);
                    total_received += bytes_recv;
                }
                
                gettimeofday(&end, NULL); // Stop chrono
                fclose(f);
                
                // Statistiques (Step 1)
                double elapsed_sec = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
                double kbps = (total_received / 1024.0) / (elapsed_sec > 0.0001 ? elapsed_sec : 1);
                
                printf("\n==========================================\n");
                printf("[+] Opération terminée !\n");
                printf(" -> %ld octets reçus en %.4f secondes.\n", total_received, elapsed_sec);
                printf(" -> Débit moyen: %.2f Kbytes/s.\n", kbps);
                printf("==========================================\n");
            } else {
                printf("[-] Code erreur renvoyé par le serveur: %d\n", res.status);
            }
        } else {
            printf("[-] Commande invalide. Utilisez 'GET <fichier>' ou 'bye'.\n");
        }
    }
    
    close(slave_sock);
    return 0;
}

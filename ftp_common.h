#ifndef FTP_COMMON_H
#define FTP_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define MAX_FILENAME 256
#define BUFFER_SIZE 4096

// Types de requêtes (Step 1 et Step 2-Q9)
typedef enum {
    REQ_GET = 1,
    REQ_PUT = 2,
    REQ_LS  = 3,
    REQ_BYE = 4
} req_type_t;

// Structure de requête (client vers esclave)
typedef struct {
    req_type_t type;
    char filename[MAX_FILENAME];
    off_t offset; // Etape 2: Reprise de téléchargement
} request_t;

// Structure de réponse (esclave vers client)
typedef struct {
    int status; // 200 = OK, 404 = Fichier non trouvé
    off_t filesize; // Taille totale du fichier à télécharger
} response_t;

// Requête init vers le maître (Etape 3)
typedef struct {
    int dummy; // Un simple entier permet au maître de comprendre qu'un client le sollicite
} master_req_t;

// Réponse du maître indiquant vers quel esclave le client doit se diriger
typedef struct {
    char slave_ip[INET_ADDRSTRLEN];
    int slave_port;
} master_resp_t;

// Fonction utilitaire pour la création d'un socket écouteur (pour économiser des lignes redondantes)
static inline int create_listen_socket(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    if (listen(sock, 10) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}

#endif

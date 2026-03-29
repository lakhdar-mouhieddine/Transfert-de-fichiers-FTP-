#include "ftp_common.h"

/*
 * Serveur Maître : (Étape 3: Load Balancing)
 * Reçoit la requête initiale, et applique un tourniquet (Round-Robin) pour assigner un esclave.
 */

#define MAX_SLAVES 10

typedef struct {
    char ip[INET_ADDRSTRLEN];
    int port;
} slave_info_t;

int main(int argc, char *argv[]) {
    // Les esclaves sont donnés en argument
    if (argc < 4 || (argc - 2) % 2 != 0) {
        fprintf(stderr, "Utilisation: %s <port_maitre> <ip_esclave1> <port_esclave1> [<ip_esclave2> <port_esclave2>...]\n", argv[0]);
        exit(1);
    }
    
    int master_port = atoi(argv[1]);
    int num_slaves = (argc - 2) / 2;
    if (num_slaves > MAX_SLAVES) num_slaves = MAX_SLAVES;
    
    // Enregistrement des données esclaves
    slave_info_t slaves[MAX_SLAVES];
    for (int i = 0; i < num_slaves; i++) {
        strncpy(slaves[i].ip, argv[2 + i*2], INET_ADDRSTRLEN);
        slaves[i].port = atoi(argv[3 + i*2]);
    }
    
    int server_sock = create_listen_socket(master_port);
    if (server_sock < 0) {
        perror("[-] Échec de création du socket maître");
        exit(1);
    }
    
    printf("[+] Maître démarré sur le port %d. %d esclave(s) enregistré(s) en tourniquet.\n", master_port, num_slaves);
    
    int current_slave = 0; // Index pour le tourniquet (Round-Robin)
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_sock >= 0) {
            master_req_t req;
            if (recv(client_sock, &req, sizeof(master_req_t), 0) > 0) {
                // Créer et renseigner la réponse avec le tour de l'esclave courant
                master_resp_t res;
                strncpy(res.slave_ip, slaves[current_slave].ip, INET_ADDRSTRLEN - 1);
                res.slave_ip[INET_ADDRSTRLEN - 1] = '\0';
                res.slave_port = slaves[current_slave].port;
                
                send(client_sock, &res, sizeof(master_resp_t), 0);
                
                printf("[INFO] Client redirigé vers l'esclave #%d (%s:%d)\n", current_slave+1, res.slave_ip, res.slave_port);
                
                // Rotation (Tourniquet)
                current_slave = (current_slave + 1) % num_slaves;
            }
            close(client_sock);
        }
    }
    
    close(server_sock);
    return 0;
}

#include "../lib/all.h"

TcpServer* tcp_server_create(int port) {
    TcpServer* server = calloc(1, sizeof(TcpServer));
    if (!server) return NULL;

    server->port = port;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        perror("WSAStartup");
        free(server);
        return NULL;
    }
#endif

    server->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_socket < 0) {
        perror("socket");
#ifdef _WIN32
        WSACleanup();
#endif
        free(server);
        return NULL;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(server->server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(server->server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server->server_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        CLOSESOCKET(server->server_socket);
    #ifdef _WIN32
        WSACleanup();
    #endif
        free(server);
        return NULL;
    }

    if (listen(server->server_socket, 10) < 0) {
        perror("listen");
        CLOSESOCKET(server->server_socket);
    #ifdef _WIN32
        WSACleanup();
    #endif
        free(server);
        return NULL;
    }

    printf("TCP server listening on port %d\n", port);
    return server;
}

void tcp_server_destroy(TcpServer* server) {
    if (!server) return;

    for (int i = 0; i < TCP_MAX_CLIENTS; i++) {
        if (server->clients[i].socket > 0)
            CLOSESOCKET(server->clients[i].socket);
    }

    CLOSESOCKET(server->server_socket);
#ifdef _WIN32
    WSACleanup();
#endif
    free(server);
}

/** Compte les connexions déjà ouvertes depuis une adresse IP donnée. */
static int count_clients_from_ip(TcpServer* server, struct in_addr addr) {
    int count = 0;
    for (int i = 0; i < TCP_MAX_CLIENTS; i++) {
        if (server->clients[i].socket > 0 &&
            server->clients[i].addr.sin_addr.s_addr == addr.s_addr) {
            count++;
        }
    }
    return count;
}

static void add_client(Codenames* codenames, int client_socket, struct sockaddr_in addr) {
    TcpServer* server = codenames->tcp;

    /* Empêche une seule machine de monopoliser les 128 emplacements. */
    if (count_clients_from_ip(server, addr.sin_addr) >= TCP_MAX_CLIENTS_PER_IP) {
        printf("Rejected connection from %s: too many connections from this address\n",
               inet_ntoa(addr.sin_addr));
        CLOSESOCKET(client_socket);
        return;
    }

    for (int i = 0; i < TCP_MAX_CLIENTS; i++) {
        if (server->clients[i].socket == 0) {
            memset(&server->clients[i], 0, sizeof(TcpClient));
            server->clients[i].socket = client_socket;
            server->clients[i].addr = addr;
            server->clients[i].id = i;
            server->clients[i].connected_at = time(NULL);
            server->clients[i].has_spoken = 0;

            tcp_on_client_connect(codenames, &server->clients[i]);
            return;
        }
    }

    CLOSESOCKET(client_socket);
}

static void remove_client(Codenames* codenames, TcpClient* client) {
    tcp_on_client_disconnect(codenames, client);
    CLOSESOCKET(client->socket);
    memset(client, 0, sizeof(TcpClient));
}

void tcp_disconnect(Codenames* codenames, TcpClient* client) {
    remove_client(codenames, client);
}

/**
 * Extrait et traite toutes les lignes complètes présentes dans le tampon du client.
 * TCP ne garantit aucune frontière de message : sans cette accumulation, une trame
 * coupée en deux paquets était interprétée comme deux messages invalides.
 * @return 1 si le client est toujours connecté, 0 s'il a été déconnecté.
 */
static int drain_client_lines(Codenames* codenames, TcpClient* client) {
    for (;;) {
        char* newline = memchr(client->rx_buffer, '\n', client->rx_len);
        if (!newline) return 1;

        size_t line_len = (size_t)(newline - client->rx_buffer);
        size_t consumed = line_len + 1;

        if (client->rx_overflow) {
            /* Fin d'une ligne trop longue : on la jette sans la traiter. */
            client->rx_overflow = 0;
        } else {
            client->rx_buffer[line_len] = '\0';
            /* Tolère les fins de ligne CRLF. */
            if (line_len > 0 && client->rx_buffer[line_len - 1] == '\r') {
                client->rx_buffer[line_len - 1] = '\0';
            }

            if (client->rx_buffer[0] != '\0') {
                int socket_before = client->socket;
                tcp_on_client_message(codenames, client, client->rx_buffer);
                /* Le traitement a pu déconnecter le client (version obsolète,
                   protocole invalide) : la structure est alors remise à zéro. */
                if (client->socket != socket_before || client->socket == 0) return 0;
            }
        }

        memmove(client->rx_buffer, client->rx_buffer + consumed, client->rx_len - consumed);
        client->rx_len -= consumed;
    }
}

void tcp_server_tick(Codenames* codenames) {
    TcpServer* server = codenames->tcp;

    fd_set readfds;
    FD_ZERO(&readfds);

    int max_fd = server->server_socket;
    FD_SET(server->server_socket, &readfds);

    for (int i = 0; i < TCP_MAX_CLIENTS; i++) {
        int sock = server->clients[i].socket;
        if (sock > 0) {
            FD_SET(sock, &readfds);
            if (sock > max_fd) max_fd = sock;
        }
    }

    /* Timeout sur le select pour pouvoir balayer périodiquement les sockets
       muettes, même quand aucun trafic n'arrive. */
    struct timeval timeout = { .tv_sec = 5, .tv_usec = 0 };
    int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

    if (activity < 0) return;

    if (activity > 0 && FD_ISSET(server->server_socket, &readfds)) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int client_socket = accept(server->server_socket, (struct sockaddr*)&addr, &len);
        if (client_socket >= 0) {
            add_client(codenames, client_socket, addr);
        }
    }

    char buffer[TCP_BUFFER_SIZE];

    for (int i = 0; i < TCP_MAX_CLIENTS; i++) {
        TcpClient* client = &server->clients[i];
        if (client->socket <= 0) continue;

        if (activity > 0 && FD_ISSET(client->socket, &readfds)) {
            int bytes = recv(client->socket, buffer, sizeof(buffer), 0);
            if (bytes <= 0) {
                remove_client(codenames, client);
                continue;
            }

            client->has_spoken = 1;

            for (int offset = 0; offset < bytes; offset++) {
                char c = buffer[offset];

                if (client->rx_len >= TCP_MAX_LINE && c != '\n') {
                    /* Ligne surdimensionnée : on marque le dépassement et on
                       ignore les octets jusqu'au prochain '\n'. */
                    client->rx_overflow = 1;
                    client->rx_len = 0;
                    continue;
                }
                client->rx_buffer[client->rx_len++] = c;
            }

            if (!drain_client_lines(codenames, client)) continue;
        }

        /* Ferme les sockets connectées qui n'ont jamais rien envoyé. */
        if (!client->has_spoken && client->connected_at != 0 &&
            difftime(time(NULL), client->connected_at) > TCP_HANDSHAKE_TIMEOUT_SEC) {
            printf("Closing idle connection from %s (no data within %d s)\n",
                   inet_ntoa(client->addr.sin_addr), TCP_HANDSHAKE_TIMEOUT_SEC);
            remove_client(codenames, client);
        }
    }
}

// Fonctions utilisables 

int tcp_send_to_client(Codenames* codenames, int client_id, const char* message) {
    if (!codenames || !codenames->tcp || !message) return -1;
    if (client_id < 0 || client_id >= TCP_MAX_CLIENTS) return -1;

    /* add_client garantit clients[i].id == i : l'accès est direct, plus besoin
       de balayer les 128 emplacements à chaque envoi (une diffusion à 8
       joueurs faisait 1024 comparaisons). */
    TcpClient* client = &codenames->tcp->clients[client_id];

    /* Un emplacement libre a été remis à zéro : sans ce test, son id de 0
       correspondrait au client 0. */
    if (client->socket <= 0) return -1;

    size_t len = strlen(message);
    char frame[TCP_MAX_LINE + 2];

    if (len + 2 <= sizeof(frame)) {
        /* Cas courant : pas d'allocation, la trame tient sur la pile. */
        memcpy(frame, message, len);
        frame[len] = '\n';
        frame[len + 1] = '\0';
        return (int)send(client->socket, frame, len + 1, TCP_SEND_FLAGS);
    }

    char* heap_frame = malloc(len + 2);
    if (!heap_frame) return -1;
    memcpy(heap_frame, message, len);
    heap_frame[len] = '\n';
    heap_frame[len + 1] = '\0';
    int result = (int)send(client->socket, heap_frame, len + 1, TCP_SEND_FLAGS);
    free(heap_frame);
    return result;
}

void tcp_on_client_connect(Codenames* codenames, TcpClient* client) {
    char response[64];
    format_to(response, sizeof(response), "%d %d", MSG_SEND_CLIENT_ID, client->id);

    if (tcp_send_to_client(codenames, client->id, response) < 0) {
        printf("Failed to send client ID to client %d\n", client->id);
    }
}

void tcp_on_client_disconnect(Codenames* codenames, TcpClient* client) {
    // printf("Client disconnected: ID=%d\n", client->id);
    on_leave(codenames, client);
}

void tcp_on_client_message(Codenames* codenames, TcpClient* client, char* message) {

    // Verifie que le message est bien envoyé par le client et pas un navigateur ou autre
    if (!starts_with(message, "CODENAMES ")) {
        tcp_send_to_client(codenames, client->id, "ERROR: Methode de connexion invalide");
        remove_client(codenames, client);
        return;
    }

    char* payload = message + strlen("CODENAMES ");

    on_message(codenames, client, payload);
}

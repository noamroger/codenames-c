#include "../lib/all.h"
#ifndef _WIN32
#include <netinet/tcp.h>
#endif

#define BUFFER_SIZE 1024
#define APP_PING_INTERVAL_MS 2000U
/** Taille maximale d'une trame applicative reçue du serveur. */
#define RX_MAX_LINE 1024
char buffer[BUFFER_SIZE];
static Uint32 last_app_ping_sent_at = 0;

/* Tampon d'accumulation pour le cadrage des trames (voir tick_tcp). */
static char rx_line[RX_MAX_LINE + 1];
static size_t rx_len = 0;
static int rx_overflow = 0;

static int network_initialized = 0;

static int ensure_network_initialized(void) {
#ifdef _WIN32
    if (network_initialized) return 0;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        perror("WSAStartup");
        return -1;
    }
#endif
    network_initialized = 1;
    return 0;
}

static void maybe_send_app_ping(AppContext* context) {
    if (!context || context->sock < 0) {
        return;
    }

    Uint32 now = SDL_GetTicks();
    if (last_app_ping_sent_at != 0 && (Uint32)(now - last_app_ping_sent_at) < APP_PING_INTERVAL_MS) {
        return;
    }

    char payload[64];
    format_to(payload, sizeof(payload), "%d %u", MSG_PING, now);
    if (send_tcp(context->sock, payload) >= 0) {
        last_app_ping_sent_at = now;
    }
}

int init_tcp(const char* server_ip, int port) {

    if (ensure_network_initialized() != 0) {
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

#ifdef _WIN32
    unsigned long ip_addr = inet_addr(server_ip);
    if (ip_addr == INADDR_NONE && strcmp(server_ip, "255.255.255.255") != 0) {
        perror("inet_addr");
        CLOSESOCKET(sock);
        return -1;
    }
    server_addr.sin_addr.s_addr = ip_addr;
#else
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        CLOSESOCKET(sock);
        return -1;
    }
#endif

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        CLOSESOCKET(sock);
        return -1;
    }

    return sock;
}

int tick_tcp(AppContext* context) {
    if (!context || context->sock < 0) {
        return EXIT_SUCCESS;
    }

    int sock = context->sock;
    maybe_send_app_ping(context);
    
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    int maxfd = sock;

    struct timeval timeout = {.tv_sec = 0, .tv_usec = 0};
    int result = select(maxfd + 1, &readfds, NULL, NULL, &timeout); // Timeout pour avoir une boucle non bloquante
    
    if (result < 0) {
        perror("select");
        return -1;
    }
    
    if (result == 0) {
        return EXIT_SUCCESS; // No data available
    }

    /* Message venant du serveur */
    if (FD_ISSET(sock, &readfds)) {
        int bytes = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes <= 0) {
            printf("Server disconnected\n");
            return EXIT_SUCCESS;
        }

        /* TCP ne préserve pas les frontières de message : on accumule jusqu'au
           '\n' pour ne pas couper une trame arrivée en deux paquets. */
        for (int offset = 0; offset < bytes; offset++) {
            char c = buffer[offset];

            if (c != '\n') {
                if (rx_len >= RX_MAX_LINE) {
                    rx_overflow = 1; // ligne aberrante : on la jette
                    rx_len = 0;
                    continue;
                }
                rx_line[rx_len++] = c;
                continue;
            }

            if (rx_overflow) {
                rx_overflow = 0;
                rx_len = 0;
                continue;
            }

            if (rx_len > 0 && rx_line[rx_len - 1] == '\r') rx_len--; // tolère CRLF
            rx_line[rx_len] = '\0';
            rx_len = 0;

            if (rx_line[0] == '\0') continue;

            // Copier la ligne car on_message utilise strtok qui écraserait notre état
            char* line_copy = strdup(rx_line);
            if (line_copy) {
                // printf("[SERVER] %s\n", line_copy);
                int ret = on_message(context, line_copy);
                free(line_copy);
                if (ret != EXIT_SUCCESS) {
                    return ret;
                }
            }
        }

        return EXIT_SUCCESS;
    }

    return EXIT_SUCCESS;

}

int get_tcp_ping_ms(int sock) {
#ifdef _WIN32
    (void)sock;
    return -1;
#else
    if (sock < 0) {
        return -1;
    }

    struct tcp_info info;
    socklen_t info_len = sizeof(info);
    if (getsockopt(sock, IPPROTO_TCP, TCP_INFO, &info, &info_len) == 0) {
        return (int)(info.tcpi_rtt / 1000U);
    }

    return -1;
#endif
}

int is_tcp_local_server(int sock) {
    if (sock < 0) {
        return 0;
    }

    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    if (getpeername(sock, (struct sockaddr*)&peer_addr, &peer_addr_len) != 0) {
        return 0;
    }

    if (peer_addr.sin_family != AF_INET) {
        return 0;
    }

    Uint32 addr = ntohl(peer_addr.sin_addr.s_addr);
    return ((addr & 0xFF000000U) == 0x7F000000U) ? 1 : 0;
}

int send_tcp(int sock, const char* payload) {
    if (sock < 0 || !payload) return -1;

    /* +1 pour le '\n' de fin de trame : le serveur découpe les messages
       sur ce séparateur, sans lui deux envois consécutifs se collent. */
    size_t total = strlen("CODENAMES ") + strlen(payload) + 2;
    char* message = malloc(total);
    if (!message) {
        perror("malloc");
        return -1;
    }

    snprintf(message, total, "CODENAMES %s\n", payload);
#ifdef _WIN32
    int result = send(sock, message, (int)strlen(message), 0);
#else
    int result = send(sock, message, strlen(message), MSG_DONTWAIT);
#endif
    free((void*)message);

    return result;
}

int close_tcp(int sock) {
    CLOSESOCKET(sock);
#ifdef _WIN32
    if (network_initialized) {
        WSACleanup();
        network_initialized = 0;
    }
#endif
    return EXIT_SUCCESS;
}
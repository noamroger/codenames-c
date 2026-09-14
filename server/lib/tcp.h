/**
 * @file tcp.h
 * @brief Serveur TCP et gestion des connexions clients.
 */

#ifndef TCP_H
#define TCP_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

#include <stddef.h>
#include <time.h>

#define TCP_MAX_CLIENTS 128
#define TCP_BUFFER_SIZE 1024

/** Taille maximale d'une trame applicative (une ligne terminée par '\n'). */
#define TCP_MAX_LINE 1024

/** Nombre maximal de connexions simultanées depuis une même adresse IP. */
#define TCP_MAX_CLIENTS_PER_IP 8

/** Délai (secondes) avant de fermer une socket connectée qui n'a rien envoyé. */
#define TCP_HANDSHAKE_TIMEOUT_SEC 30

/** Drapeaux d'envoi : MSG_NOSIGNAL évite SIGPIPE si le pair a fermé la socket.
 *  Winsock ne connaît pas ce drapeau et ne lève pas de signal. */
#ifdef MSG_NOSIGNAL
#define TCP_SEND_FLAGS MSG_NOSIGNAL
#else
#define TCP_SEND_FLAGS 0
#endif

/** Représente un client TCP.
 * @param id identifiant interne.
 * @param socket descripteur de socket.
 * @param addr adresse réseau du client (struct sockaddr_in).
 * @param rx_buffer tampon d'accumulation pour le cadrage des trames.
 * @param rx_len nombre d'octets actuellement dans rx_buffer.
 * @param rx_overflow 1 si la ligne courante dépasse TCP_MAX_LINE (on la jette).
 * @param connected_at date de connexion, pour le délai de handshake.
 * @param has_spoken 1 dès qu'une trame valide a été reçue.
 */
typedef struct {
    int id;
    int socket;
    struct sockaddr_in addr;
    char rx_buffer[TCP_MAX_LINE + 1];
    size_t rx_len;
    int rx_overflow;
    time_t connected_at;
    int has_spoken;
} TcpClient;

/** Représente le serveur TCP.
 * @param server_socket socket d'écoute.
 * @param port port écouté.
 * @param clients tableau de clients connectés.
 * @param next_client_id générateur d'identifiants.
 */
typedef struct {
    int server_socket;
    int port;
    TcpClient clients[TCP_MAX_CLIENTS];
} TcpServer;

/* Initialisation / destruction */

/** 
 * Crée et initialise un serveur TCP.
 * @param port Port d'écoute.
 * @return Pointeur vers TcpServer, ou NULL en cas d'erreur.
 */
TcpServer* tcp_server_create(int port);

/** 
 * Détruit un serveur TCP et libère ses ressources.
 * @param server Serveur à détruire.
 */
void tcp_server_destroy(TcpServer* server);

/* Forward declaration to avoid circular include with codenames.h */
typedef struct Codenames Codenames;

/**
 * Déconnecte un client du serveur.
 * @param codenames Contexte principal du serveur.
 * @param client Client à déconnecter.
 */
void tcp_disconnect(Codenames* codenames, TcpClient* client);

/** 
 * Tick à appeler dans la boucle principale du serveur pour gérer les événements réseau.
 * @param codenames Contexte principal du serveur.
 */
void tcp_server_tick(Codenames* codenames);

/* Communication */

/** 
 * Envoie un message à un client spécifique.
 * @param codenames Contexte principal du serveur.
 * @param client_id Identifiant du client.
 * @param message Message à envoyer.
 * @return 0 en cas de succès, valeur négative en cas d'erreur.
 */
int tcp_send_to_client(Codenames* codenames, int client_id, const char* message);

/* Callbacks (à surcharger si besoin) */

/**
 * Callback appelé lors de la connexion d'un client.
 * @param codenames Contexte principal du serveur.
 * @param client Client qui vient de se connecter.
 */
void tcp_on_client_connect(Codenames* codenames, TcpClient* client);

/**
 * Callback appelé lors de la déconnexion d'un client.
 * @param codenames Contexte principal du serveur.
 * @param client Client qui vient de se déconnecter.
 */
void tcp_on_client_disconnect(Codenames* codenames, TcpClient* client);

/**
 * Callback appelé lors de la réception d'un message d'un client.
 * @param codenames Contexte principal du serveur.
 * @param client Client ayant envoyé le message.
 * @param message Message reçu.
 */
void tcp_on_client_message(Codenames* codenames, TcpClient* client, char* message);

#endif // TCP_H
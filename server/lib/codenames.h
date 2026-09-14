/**
 * @file codenames.h
 * @brief Structure principale du serveur Codenames.
 */

#ifndef CODENAMES_H
#define CODENAMES_H

#include "tcp.h"
#include "lobby.h"

/**
 * Structure principale du serveur de Codenames.
 * @param tcp gestionnaire du serveur TCP (sockets, clients, etc.).
 * @param lobby gestionnaire des lobbies (parties en attente, joueurs, etc.).
 * @param version version du serveur.
 */
typedef struct Codenames {
    TcpServer* tcp;
    LobbyManager* lobby;
    char version[32];
} Codenames;

#endif // CODENAMES_H
#include "../lib/all.h"

LobbyManager* create_lobby_manager() {
    LobbyManager* manager = malloc(sizeof(LobbyManager));
    if (!manager) return NULL;
    manager->lobbies = list_create();
    if (!manager->lobbies) {
        free(manager);
        return NULL;
    }
    return manager;
}

/** Callback interne pour détruire chaque lobby lors de la destruction du manager. */
static void destroy_lobby_callback(void* data, void* context) {
    Codenames* codenames = (Codenames*)context;
    Lobby* lobby = (Lobby*)data;
    if (lobby) {
        for (int i = 0; i < lobby->nb_players; i++) {
            char msg[16];
            format_to(msg, sizeof(msg), "%d", MSG_LOBBYCLOSED);
            tcp_send_to_client(codenames, lobby->users[i]->id, msg);
            destroy_user(lobby->users[i]);
        }
        destroy_game(lobby->game); // la partie en cours doit être libérée avec le lobby
        chat_clear(&lobby->chat);
        free(lobby);
    }
}

void destroy_lobby_manager(Codenames* codenames, LobbyManager* manager) {
    if (manager) {
        list_foreach(manager->lobbies, destroy_lobby_callback, codenames);
        list_destroy(manager->lobbies, NULL); // données déjà libérées par le callback
        free(manager);
    }
}

/** Extracteur d'ID pour list_next_available_id. */
static int lobby_get_id(void* data) {
    return ((Lobby*)data)->id;
}

Lobby* create_lobby(LobbyManager* manager) {
    if (list_size(manager->lobbies) >= MAX_LOBBIES) {
        return NULL;
    }
    Lobby* lobby = malloc(sizeof(Lobby));
    if (!lobby) return NULL;

    lobby->id = list_next_available_id(manager->lobbies, lobby_get_id);
    lobby->status = LB_STATUS_WAITING;
    lobby->nb_players = 0;
    lobby->owner_id = -1;
    lobby->game = NULL;
    lobby->words_difficulty = WORDS_DIFFICULTY_NORMAL;
    lobby->nb_assassins = 1;
    char* code = generate_code();
    if (!code) {
        free(lobby);
        return NULL;
    }
    snprintf(lobby->code, sizeof(lobby->code), "%s", code);
    free(code);

    if (chat_init(&lobby->chat, CHAT_MAX_MESSAGES) != EXIT_SUCCESS) {
        free(lobby);
        return NULL;
    }

    if (list_add(manager->lobbies, lobby) != EXIT_SUCCESS) {
        chat_clear(&lobby->chat);
        free(lobby);
        return NULL;
    }

    return lobby;
}

int join_lobby(Lobby* lobby, User* user) {
    if (lobby->nb_players >= MAX_USERS) {
        return EXIT_FAILURE;
    }
    lobby->users[lobby->nb_players++] = user;
    return EXIT_SUCCESS;
}

int leave_lobby(Lobby* lobby, User* user) {
    int index = -1;
    // Cherche l'utilisateur dans la liste des joueurs du lobby
    for (int i = 0; i < lobby->nb_players; i++) {
        if (lobby->users[i]->id == user->id) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        return EXIT_FAILURE;
    }
    destroy_user(lobby->users[index]);
    // Décale les utilisateurs restants vers la gauche
    for (int j = index; j < lobby->nb_players - 1; j++) {
        lobby->users[j] = lobby->users[j + 1];
    }
    lobby->nb_players--;
    return EXIT_SUCCESS;
}

int choose_role(Lobby* lobby, User* user, int role, int team) {
    for (int i = 0; i < lobby->nb_players; i++) {
        if (lobby->users[i]->id == user->id) {
            lobby->users[i]->role = role;
            lobby->users[i]->team = team;
            return EXIT_SUCCESS;
        }
    }
    return EXIT_FAILURE;
}

/* Prédicats de recherche pour list_find */

static int predicate_owner_id(void* data, void* context) {
    Lobby* lobby = (Lobby*)data;
    int owner_id = *(int*)context;
    return lobby->owner_id == owner_id;
}

static int predicate_player_id(void* data, void* context) {
    Lobby* lobby = (Lobby*)data;
    int id = *(int*)context;
    for (int j = 0; j < lobby->nb_players; j++) {
        if (lobby->users[j]->id == id) return 1;
    }
    return 0;
}

static int predicate_code(void* data, void* context) {
    Lobby* lobby = (Lobby*)data;
    const char* code = (const char*)context;
    return strcmp(lobby->code, code) == 0;
}

User* find_user_by_id(Lobby* lobby, int id) {
    for (int i = 0; i < lobby->nb_players; i++) {
        if (lobby->users[i]->id == id) {
            return lobby->users[i];
        }
    }
    return NULL;
}

Lobby* find_lobby_by_ownerid(LobbyManager* manager, int owner_id) {
    return (Lobby*)list_find(manager->lobbies, predicate_owner_id, &owner_id);
}

Lobby* find_lobby_by_playerid(LobbyManager* manager, int id) {
    return (Lobby*)list_find(manager->lobbies, predicate_player_id, &id);
}

Lobby* find_lobby_by_code(LobbyManager* manager, const char* code) {
    return (Lobby*)list_find(manager->lobbies, predicate_code, (void*)code);
}

void destroy_lobby(Codenames* codenames, Lobby* lobby) {
    if (lobby) {
        for (int i = 0; i < lobby->nb_players; i++) {
            char msg[16];
            format_to(msg, sizeof(msg), "%d", MSG_LOBBYCLOSED);
            tcp_send_to_client(codenames, lobby->users[i]->id, msg);
            destroy_user(lobby->users[i]);
        }
        list_remove(codenames->lobby->lobbies, lobby);
        destroy_game(lobby->game); // la partie en cours doit être libérée avec le lobby
        chat_clear(&lobby->chat);
        free(lobby);
    }
}

// interactions joueurs
int request_create_lobby(Codenames* codenames, TcpClient* client, char* message, Arguments args) {
    if (!args_require(args, 1)) {
        printf("Invalid create lobby message from client %d: \"%s\"\n", client->id, message);
        send_server_error(codenames, client, "Missing username");
        return EXIT_FAILURE;
    }

    /* Un client déjà dans un lobby ne doit pas pouvoir en créer d'autres :
       sinon quelques requêtes suffisent à saturer les MAX_LOBBIES places. */
    if (find_lobby_by_playerid(codenames->lobby, client->id)) {
        printf("Client %d is already in a lobby\n", client->id);
        send_server_error(codenames, client, "You are already in a lobby");
        return EXIT_FAILURE;
    }

    Lobby* lobby = create_lobby(codenames->lobby);
    if (lobby == NULL) {
        printf("Failed to create lobby\n");
        send_server_error(codenames, client, "Unable to create a lobby right now");
        return EXIT_FAILURE;
    }
    // Le client qui crée le lobby en devient automatiquement le propriétaire et rejoint le lobby
    lobby->owner_id = client->id;
    User* user = create_user(client->id, args.argv[0], client->socket);
    if (user == NULL) {
        printf("Failed to create user for client %d\n", client->id);
        /* Sans ce nettoyage, le lobby resterait dans la liste sans aucun
           joueur : 50 tentatives suffisaient à bloquer le serveur. */
        destroy_lobby(codenames, lobby);
        send_server_error(codenames, client, "Invalid username");
        return EXIT_FAILURE;
    }
    if (join_lobby(lobby, user) != EXIT_SUCCESS) {
        printf("Failed to join lobby for client %d\n", client->id);
        destroy_user(user);
        destroy_lobby(codenames, lobby);
        return EXIT_FAILURE;
    }
    printf("Client %d (%s) created lobby %d\n", client->id, user->name, lobby->id);

    // Informer le client de la création du lobby avec son id et son code
    char reponse[64];
    format_to(reponse, sizeof(reponse), "%d %d %s", MSG_CREATELOBBY, lobby->id, lobby->code);
    tcp_send_to_client(codenames, client->id, reponse);

    return EXIT_SUCCESS;
}

int request_join_lobby(Codenames* codenames, TcpClient* client, char* message, Arguments args) {
    // Handle join lobby
    if (!args_require(args, 2)) {
        printf("Invalid join lobby message from client %d: \"%s\"\n", client->id, message);
        send_server_error(codenames, client, "Invalid join request");
        return EXIT_FAILURE;
    }

    /* Rejoindre deux fois créerait deux User pour le même id, ce qui fausse
       le comptage des joueurs et laisse des entrées fantômes derrière soi. */
    if (find_lobby_by_playerid(codenames->lobby, client->id)) {
        printf("Client %d is already in a lobby\n", client->id);
        send_server_error(codenames, client, "You are already in a lobby");
        return EXIT_FAILURE;
    }

    Lobby* lobby = find_lobby_by_code(codenames->lobby, args.argv[0]);
    if (!lobby) {
        printf("Lobby not found for client %d\n", client->id);
        send_server_error(codenames, client, "Lobby not found");
        return EXIT_FAILURE;
    }

    if (lobby->status != LB_STATUS_WAITING) {
        printf("Client %d tried to join lobby %d while a game is running\n", client->id, lobby->id);
        send_server_error(codenames, client, "This game has already started");
        return EXIT_FAILURE;
    }

    if (lobby->nb_players >= MAX_USERS) {
        send_server_error(codenames, client, "This lobby is full");
        return EXIT_FAILURE;
    }

    User* user = create_user(client->id, args.argv[1], client->socket);
    if (!user) {
        printf("Failed to create user for client %d\n", client->id);
        send_server_error(codenames, client, "Invalid username");
        return EXIT_FAILURE;
    }

    if (join_lobby(lobby, user) != EXIT_SUCCESS) {
        printf("Failed to join lobby %d for client %d\n", lobby->id, client->id);
        destroy_user(user);
        return EXIT_FAILURE;
    }

    printf("Client %d (%s) joined lobby %d\n", client->id, user->name, lobby->id);

    char trame[64];
    format_to(trame, sizeof(trame), "%d %d %s", MSG_JOINLOBBY, lobby->id, lobby->code);
    tcp_send_to_client(codenames, client->id, trame);

    // Envoyer au nouveau joueur la liste des joueurs déjà présents dans le lobby
    for (int i = 0; i < lobby->nb_players; i++) {
        if (lobby->users[i]->id != client->id) {
            char msg[64];
            format_to(msg, sizeof(msg), "%d %d %s %d %d", MSG_PLAYERJOINED, lobby->users[i]->id, lobby->users[i]->name, lobby->users[i]->role, lobby->users[i]->team);
            tcp_send_to_client(codenames, client->id, msg);
        }
    }

    // Notifier les autres joueurs du lobby qu'un nouveau joueur a rejoint
    for (int i = 0; i < lobby->nb_players; i++) {
        if (lobby->users[i]->id != client->id) {
            char msg[64];
            format_to(msg, sizeof(msg), "%d %d %s %d %d", MSG_PLAYERJOINED, user->id, user->name, user->role, user->team);
            tcp_send_to_client(codenames, lobby->users[i]->id, msg);
        }
    }

    return EXIT_SUCCESS;
}

int request_leave_lobby(Codenames* codenames, TcpClient* client, char* message, Arguments args) {
    (void)message; // unused
    (void)args;    // unused

    /* Trouver le lobby où se trouve le client */
    Lobby* lobby = find_lobby_by_playerid(codenames->lobby, client->id);
    if (!lobby) {
        printf("Client %d tried to leave but is not in any lobby\n", client->id);
        return EXIT_FAILURE;
    }

    printf("Client %d left lobby %d\n", client->id, lobby->id);

    /* Quitter volontairement et se déconnecter suivent exactement le même
       chemin : retrait du joueur, destruction du lobby vide, transfert de
       propriété et diffusion du départ. */
    return on_leave(codenames, client);
}

int request_choose_role(Codenames* codenames, TcpClient* client, char* message, Arguments args) {
    // Handle choose role
    if (!args_require(args, 2)) {
        printf("Invalid choose role message from client %d: \"%s\"\n", client->id, message);
        send_server_error(codenames, client, "Invalid role request");
        return EXIT_FAILURE;
    }

    Lobby* lobby = find_lobby_by_playerid(codenames->lobby, client->id);
    if (!lobby) {
        printf("Lobby not found for client %d\n", client->id);
        return EXIT_FAILURE;
    }

    User* user = find_user_by_id(lobby, client->id);
    if (!user) {
        printf("User not found for client %d\n", client->id);
        return EXIT_FAILURE;
    }

    /* Valider les énumérations : des valeurs hors plage se propagent aux
       clients, qui s'en servent pour indexer leurs tableaux d'affichage. */
    int role = atoi((char*)args.argv[0]);
    int team = atoi((char*)args.argv[1]);
    if (role < ROLE_NONE || role > ROLE_AGENT) {
        send_server_error(codenames, client, "Invalid role value");
        return EXIT_FAILURE;
    }
    if (team < TEAM_NONE || team > TEAM_BLACK) {
        send_server_error(codenames, client, "Invalid team value");
        return EXIT_FAILURE;
    }

    // Affecter le rôle et l'équipe à l'utilisateur
    if (choose_role(lobby, user, role, team) != EXIT_SUCCESS) {
        printf("Failed to choose role for client %d\n", client->id);
        return EXIT_FAILURE;
    }
    printf("Client %d (%s) chose role %d team %d in lobby %d\n", client->id, user->name, user->role, user->team, lobby->id);
    
    // affichage des joueurs lobby
    for (int i = 0; i < lobby->nb_players; i++) {
        printf(" - Player %d: %s, role %d, team %d\n", lobby->users[i]->id, lobby->users[i]->name, lobby->users[i]->role, lobby->users[i]->team);
    }

    // On informe les autres joueurs du lobby du choix de rôle/équipe de ce joueur
    char reponse[64];
    format_to(reponse, sizeof(reponse), "%d %d %d %d", MSG_CHOOSE_ROLE, user->id, user->role, user->team);
    for (int i = 0; i < lobby->nb_players; i++) {
        if (lobby->users[i]->id != client->id) { // Pas besoin de s'envoyer à soi-même
            printf("Sending role choice to client %d: %s\n", lobby->users[i]->id, reponse);
            tcp_send_to_client(codenames, lobby->users[i]->id, reponse);
        }
    }
    
    return EXIT_SUCCESS;
}
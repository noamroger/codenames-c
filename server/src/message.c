#include "../lib/all.h"

MessageType fetch_header(char* message) {
    int value = 0;
    /* sscanf renvoie EOF (-1) sur une chaîne vide et 0 si aucune conversion :
       les deux cas doivent donner MSG_UNKNOWN, sinon `header` reste non initialisé. */
    if (!message || sscanf(message, "%d", &value) != 1) return MSG_UNKNOWN;
    return (MessageType)value;
}

int args_require(Arguments args, int needed) {
    return args.argv != NULL && args.argc >= needed;
}

void send_server_error(Codenames* codenames, TcpClient* client, const char* reason) {
    char msg[192];
    format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, reason ? reason : "Invalid request");
    tcp_send_to_client(codenames, client->id, msg);
}

Arguments parse_arguments(char* message) {
    Arguments args = {0};
    char* token = strtok(message, " ");

    while (token != NULL) {
        char** resized_argv = realloc(args.argv, (args.argc + 1) * sizeof(char*));
        if (!resized_argv) {
            free(args.argv);
            args.argv = NULL;
            args.argc = 0;
            break;
        }

        args.argv = resized_argv;
        args.argv[args.argc] = token;
        args.argc++;
        token = strtok(NULL, " ");
    }

    return args;
}

int on_message(Codenames* codenames, TcpClient* client, char* message) {
    MessageType header = fetch_header(message);

    /* Avance après l'en-tête et son espace, sans jamais dépasser le '\0' final. */
    size_t skip = (size_t)number_length((int)header) + 1;
    size_t available = strlen(message);
    message += (skip > available) ? available : skip;

    // printf("[MSG] %d %s\n", header, message);

    Arguments args = parse_arguments(message);
    int status = EXIT_SUCCESS;

    switch (header) {
        case MSG_UNKNOWN: 
            printf("Received unknown message header from client %d: \"%s\"\n", client->id, message);
            break;

        case MSG_CREATELOBBY:
            status = request_create_lobby(codenames, client, message, args);
            break;
        case MSG_JOINLOBBY:
            status = request_join_lobby(codenames, client, message, args);
            break;
        case MSG_LEAVELOBBY:
            status = request_leave_lobby(codenames, client, message, args);
            break;
        case MSG_LOBBYCLOSED: break; // Server -> Client only
        case MSG_CHOOSE_ROLE:
            status = request_choose_role(codenames, client, message, args);
            break;
        case MSG_STARTGAME:
            status = request_start_game(codenames, client, message, args);
            break;
        case MSG_SUBMIT_HINT:
            status = request_submit_hint(codenames, client, message, args);
            break;
        case MSG_PREGUESS:
            status = request_preguess(codenames, client, message, args);
            break;
        case MSG_GUESS_CARD:
            status = request_guess_card(codenames, client, message, args);
            break;
        case MSG_SET_WORDS_DIFFICULTY:
            status = request_set_words_difficulty(codenames, client, message, args);
            break;
        case MSG_SET_NB_ASSASSINS:
            status = request_set_nb_assassins(codenames, client, message, args);
            break;

        case MSG_SENDCHAT:
            status = request_send_chat(codenames, client, message, args);
            break;

        case MSG_REQUESTUUID:
            status = request_uuid(codenames, client, message, args);
            break;
        case MSG_COMPAREVERSION:
            status = on_version_compare(codenames, client, message, args);
            break;
        case MSG_PING:
            if (args.argc >= 1) {
                char response[64];
                format_to(response, sizeof(response), "%d %s", MSG_PING, args.argv[0]);
                tcp_send_to_client(codenames, client->id, response);
            }
            status = EXIT_SUCCESS;
            break;

        default:
            // Les autres types de message sont gérés ailleurs ou sont réservés aux clients.
            break;
    }

    free(args.argv);
    return status;
}

int on_leave(Codenames* codenames, TcpClient* client) {
    // Handle client disconnection
    Lobby* lobby = find_lobby_by_playerid(codenames->lobby, client->id);
    if (!lobby) return EXIT_SUCCESS;

    User* user = find_user_by_id(lobby, client->id);
    if (user) {
        /* Retirer effectivement le joueur : sans cela son User restait dans le
           lobby après la déconnexion, et l'emplacement TCP recyclé donnait au
           client suivant la place, le rôle et l'équipe du précédent. */
        leave_lobby(lobby, user);
    }

    // Le lobby vide n'a plus de raison d'exister
    if (lobby->nb_players == 0) {
        int id = lobby->id;
        destroy_lobby(codenames, lobby);
        printf("Destroyed empty lobby %d after client %d left\n", id, client->id);
        return EXIT_SUCCESS;
    }

    // Transférer la propriété si le partant en était le propriétaire
    if (lobby->owner_id == client->id) {
        lobby->owner_id = lobby->users[0]->id;
        printf("Client %d (%s) is now the owner of lobby %d\n",
               lobby->owner_id, lobby->users[0]->name, lobby->id);
    }

    // Informer les autres joueurs du lobby qu'un joueur a quitté
    char msg[64];
    format_to(msg, sizeof(msg), "%d %d", MSG_PLAYERLEFT, client->id);
    for (int i = 0; i < lobby->nb_players; i++) {
        tcp_send_to_client(codenames, lobby->users[i]->id, msg);
    }

    return EXIT_SUCCESS;
}
#include "../lib/all.h"

static void free_chat_message(void* data) {
    free(data);
}

int chat_init(Chat* chat, int max_messages) {
    if (!chat || max_messages <= 0) return EXIT_FAILURE;

    chat->messages = list_create();
    if (!chat->messages) return EXIT_FAILURE;

    chat->max_messages = max_messages;
    return EXIT_SUCCESS;
}

int chat_push(Chat* chat, const char* message) {
    if (!chat || !chat->messages || !message) return EXIT_FAILURE;

    char* copy = strdup(message);
    if (!copy) return EXIT_FAILURE;

    if (list_add(chat->messages, copy) != EXIT_SUCCESS) {
        free(copy);
        return EXIT_FAILURE;
    }

    while (chat->messages->size > chat->max_messages) {
        ListNode* oldest = chat->messages->head;
        if (!oldest) break;

        chat->messages->head = oldest->next;
        /* La liste maintient un pointeur de queue : il doit suivre quand on
           retire le dernier élément restant, sinon il reste pendouillant. */
        if (chat->messages->tail == oldest) chat->messages->tail = NULL;
        chat->messages->size--;

        free_chat_message(oldest->data);
        free(oldest);
    }

    return EXIT_SUCCESS;
}

const char* chat_get(Chat* chat, int index) {
    if (!chat || !chat->messages) return NULL;
    return (const char*)list_get(chat->messages, index);
}

int chat_size(Chat* chat) {
    if (!chat || !chat->messages) return 0;
    return list_size(chat->messages);
}

void chat_clear(Chat* chat) {
    if (!chat || !chat->messages) return;

    list_destroy(chat->messages, free_chat_message);
    chat->messages = NULL;
    chat->max_messages = 0;
}

int request_send_chat(Codenames* codenames, TcpClient* client, char* message, Arguments args) {
    if (args.argc < 2) {
        printf("Invalid chat message from client %d: \"%s\"\n", client->id, message);
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "Invalid chat message format");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    // Vérifie que le client est bien dans un lobby
    Lobby* lobby = find_lobby_by_playerid(codenames->lobby, client->id);
    if (!lobby) {
        printf("Client %d is not in a lobby\n", client->id);
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "You must be in a lobby to send chat messages");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    const char* sender_name = "Unknown";
    User* sender_user = find_user_by_id(lobby, client->id);
    if (sender_user && sender_user->name && sender_user->name[0] != '\0') {
        sender_name = sender_user->name;
    } else if (args.argv[0] && ((char*)args.argv[0])[0] != '\0') {
        sender_name = (char*)args.argv[0];
    }

    // Pour que le message puisse contenir des espaces, on reconstruit le message à partir de tous les arguments après le premier (le nom de l'expéditeur)
    char rebuilt_message[448];
    rebuilt_message[0] = '\0';
    for (int i = 1; i < args.argc; i++) {
        if (i > 1) {
            strncat(rebuilt_message, " ", sizeof(rebuilt_message) - strlen(rebuilt_message) - 1);
        }
        strncat(rebuilt_message, (char*)args.argv[i], sizeof(rebuilt_message) - strlen(rebuilt_message) - 1);
    }

    // Diffuse le message de chat à tous les joueurs du lobby
    char msg[512];
    format_to(msg, sizeof(msg), "%d %d %s %s", MSG_SENDCHAT, client->id, sender_name, rebuilt_message);
    if (chat_push(&lobby->chat, msg) != EXIT_SUCCESS) {
        printf("Failed to store chat message in lobby %d history\n", lobby->id);
    }
    for (int i = 0; i < lobby->nb_players; i++) {
        tcp_send_to_client(codenames, lobby->users[i]->id, msg);
    }

    return EXIT_SUCCESS;
} 
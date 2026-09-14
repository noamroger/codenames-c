#include "../lib/all.h"

/**
 * Une liste de mots chargée en mémoire.
 * @param path chemin du fichier source.
 * @param words tableau de mots (taille count).
 * @param count nombre de mots effectivement chargés.
 */
typedef struct {
    const char* path;
    char** words;
    int count;
} WordList;

/* Les quatre listes sont chargées une seule fois au démarrage : auparavant
   chaque lancement de partie rouvrait le fichier et réallouait ~500 chaînes. */
static WordList word_lists[] = {
    [WORDS_DIFFICULTY_NORMAL] = { "assets/wordlist.txt",        NULL, 0 },
    [WORDS_DIFFICULTY_HARD]   = { "assets/wordlist_hard.txt",   NULL, 0 },
    [WORDS_DIFFICULTY_INFO]   = { "assets/wordlist_info.txt",   NULL, 0 },
    [WORDS_DIFFICULTY_FREAKY] = { "assets/wordlist_freaky.txt", NULL, 0 },
};

static const int WORD_LIST_COUNT = (int)(sizeof(word_lists) / sizeof(word_lists[0]));

/** Retourne la liste correspondant à une difficulté, ou NULL si hors plage. */
static WordList* word_list_for(WordsDifficulty difficulty) {
    int index = (int)difficulty;
    if (index < 0 || index >= WORD_LIST_COUNT) return NULL;
    return &word_lists[index];
}

/** Charge un fichier de mots en mémoire. Retourne EXIT_SUCCESS ou EXIT_FAILURE. */
static int load_word_list(WordList* list) {
    int capacity = count_words(list->path);
    if (capacity < 0) {
        fprintf(stderr, "game_manager: failed to read %s\n", list->path);
        return EXIT_FAILURE;
    }
    if (capacity == 0) {
        fprintf(stderr, "game_manager: %s contains no word\n", list->path);
        return EXIT_FAILURE;
    }

    FILE* file = fopen(list->path, "r");
    if (!file) {
        perror("Failed to open words file");
        return EXIT_FAILURE;
    }

    char** words = (char**)calloc((size_t)capacity, sizeof(char*));
    if (!words) {
        perror("Failed to allocate memory for words");
        fclose(file);
        return EXIT_FAILURE;
    }

    char buffer[32];
    int count = 0;
    while (count < capacity && fgets(buffer, sizeof(buffer), file)) {
        size_t raw_len = strlen(buffer);
        int truncated = (raw_len > 0 && buffer[raw_len - 1] != '\n');

        buffer[strcspn(buffer, "\n")] = 0; // Remove newline character

        /* Une ligne plus longue que le tampon doit être consommée jusqu'au bout,
           sinon son reste serait compté comme un mot supplémentaire. */
        if (truncated) {
            int c;
            do { c = fgetc(file); } while (c != '\n' && c != EOF);
        }

        if (buffer[0] == '\0') continue; // ignore les lignes vides

        words[count] = strdup(buffer);
        if (!words[count]) {
            perror("Failed to allocate memory for a word");
            for (int j = 0; j < count; j++) free(words[j]);
            free(words);
            fclose(file);
            return EXIT_FAILURE;
        }
        count++;
    }
    fclose(file);

    if (count < GAME_NB_WORDS) {
        fprintf(stderr, "game_manager: %s has only %d words, %d required\n",
                list->path, count, GAME_NB_WORDS);
        for (int j = 0; j < count; j++) free(words[j]);
        free(words);
        return EXIT_FAILURE;
    }

    list->words = words;
    list->count = count;
    printf("Loaded %d words from %s\n", count, list->path);
    return EXIT_SUCCESS;
}

int init_game_manager() {
    for (int i = 0; i < WORD_LIST_COUNT; i++) {
        if (load_word_list(&word_lists[i]) != EXIT_SUCCESS) {
            destroy_game_manager();
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

void destroy_game_manager(void) {
    for (int i = 0; i < WORD_LIST_COUNT; i++) {
        for (int j = 0; j < word_lists[i].count; j++) {
            free(word_lists[i].words[j]);
        }
        free(word_lists[i].words);
        word_lists[i].words = NULL;
        word_lists[i].count = 0;
    }
}

Word* generateWords(int count, Team start_team, WordsDifficulty words_difficulty, int nb_assassins) { //Trouver 25 mots au hasard parmis la liste des mots.
    if (count <= 0) return NULL;

    WordList* list = word_list_for(words_difficulty);
    if (!list || !list->words) {
        fprintf(stderr, "generateWords: word list %d is not loaded\n", (int)words_difficulty);
        return NULL;
    }

    /* Sans assez de mots distincts, la sélection sans remise ne pourrait pas
       aboutir : on refuse la partie proprement plutôt que de boucler. */
    if (list->count < count) {
        fprintf(stderr, "generateWords: word list too small (%d words for %d cards)\n", list->count, count);
        return NULL;
    }

    Word* words = (Word*)calloc((size_t)count, sizeof(Word));
    if (!words) {
        perror("Failed to allocate memory for words");
        return NULL;
    }

    if (nb_assassins < 1) nb_assassins = 1;
    if (nb_assassins > 3) nb_assassins = 3;

    int red_count = count / 3 + (start_team == TEAM_RED ? 1 : 0);
    int blue_count = count / 3 + (start_team == TEAM_BLUE ? 1 : 0);
    int neutral_count = count - red_count - blue_count - nb_assassins;
    if (neutral_count < 0) neutral_count = 0;

    /* Tirage sans remise par mélange partiel de Fisher-Yates : garantit
       `count` indices distincts en temps linéaire, là où le tirage avec rejet
       précédent pouvait retenter indéfiniment. */
    int* indices = (int*)malloc(sizeof(int) * (size_t)list->count);
    if (!indices) {
        perror("Failed to allocate memory for the word index pool");
        free(words);
        return NULL;
    }
    for (int i = 0; i < list->count; i++) indices[i] = i;

    for (int i = 0; i < count; i++) {
        int pick = randint(i, list->count - 1);
        int chosen = indices[pick];
        indices[pick] = indices[i];
        indices[i] = chosen;

        strncpy(words[i].word, list->words[chosen], sizeof(words[i].word) - 1);
        words[i].word[sizeof(words[i].word) - 1] = '\0';
        words[i].team = (i < red_count) ? TEAM_RED :
            (i < red_count + blue_count) ? TEAM_BLUE :
            (i < red_count + blue_count + neutral_count) ? TEAM_NONE : TEAM_BLACK;
        words[i].type = (CardType)(rand() % 4); // Attribue un type de carte aléatoire
        words[i].revealed = 0;
    }

    free(indices);
    return words;
}

void shuffleWords(Word* words, int count) { // Mélange les cartes auxquelles sont attribuées des couleurs rangées dans l'ordre
    for (int i = count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Word temp = words[i];
        words[i] = words[j];
        words[j] = temp;
    }
}

int destroy_game(Game* game) {
    if (!game) return EXIT_FAILURE;

    if (game->words) {
        free(game->words);
        game->words = NULL;
    }

    free(game);
    return EXIT_SUCCESS;
}

int request_start_game(Codenames* codenames, TcpClient* client, char* message, Arguments args) {
    (void)message;
    (void)args;

    // Vérifie que le client est bien dans un lobby
    Lobby* lobby = find_lobby_by_ownerid(codenames->lobby, client->id);
    if (!lobby) {
        printf("Client %d doesn't own a lobby\n", client->id);
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "You must be the lobby owner to start the game");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    // Vérifie que le lobby est prêt à démarrer
    if (lobby->status != LB_STATUS_WAITING) {
        printf("Lobby %d is not ready to start\n", lobby->id);
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "Lobby is not ready to start");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    // Démarre le jeu
    if (lobby->game) {
        destroy_game(lobby->game);
        lobby->game = NULL;
    }

    lobby->status = LB_STATUS_IN_GAME;
    printf("Game started in lobby %d\n", lobby->id);

    Game* game = (Game*)malloc(sizeof(Game));
    if (!game) {
        printf("Failed to allocate memory for game\n");
        return EXIT_FAILURE;
    }

    Team start_team = (rand() % 2 == 0) ? TEAM_RED : TEAM_BLUE;

    // Génère les mots pour le jeu avec la difficulté des mots choisie
    game->words = generateWords(GAME_NB_WORDS, start_team, lobby->words_difficulty, lobby->nb_assassins);
    if (!game->words) {
        printf("Failed to generate words for lobby %d\n", lobby->id);
        destroy_game(game);
        lobby->status = LB_STATUS_WAITING;
        send_server_error(codenames, client, "Failed to start the game");
        return EXIT_FAILURE;
    }
    /* Le mélange doit venir après le contrôle de NULL : sinon un wordlist
       illisible fait planter le serveur au lieu de refuser la partie. */
    shuffleWords(game->words, GAME_NB_WORDS);

    game->nb_words = GAME_NB_WORDS;
    game->state = start_team == TEAM_RED ? GAMESTATE_TURN_RED_SPY : GAMESTATE_TURN_BLUE_SPY;
    game->can_guess = 0;
    lobby->game = game;

    // Envoi de la partie aux joueurs
    for (int i = 0; i < lobby->nb_players; i++) {
        User* user = lobby->users[i];
        char msg[32];
        format_to(msg, sizeof(msg), "%d %d %d", MSG_STARTGAME, game->state, game->nb_words);
        tcp_send_to_client(codenames, user->id, msg);
        // Envoi de chaque mot
        for (int j = 0; j < game->nb_words; j++) {
            format_to(msg, sizeof(msg), "%d %d %s %d %d %d", MSG_WORDDATA, j, game->words[j].word, game->words[j].team, game->words[j].type, game->words[j].revealed);
            tcp_send_to_client(codenames, user->id, msg);
        }
    }

    return EXIT_SUCCESS;
}

int request_submit_hint(Codenames* codenames, TcpClient* client, char* message, Arguments args) {
    (void)message;
    
    // Vérifie que le client est bien dans un lobby
    Lobby* lobby = find_lobby_by_playerid(codenames->lobby, client->id);
    if (!lobby) {
        printf("Client %d is not in a lobby\n", client->id);
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "You must be in a lobby to submit a hint");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    // Vérifie que le lobby est bien en partie
    if (lobby->status != LB_STATUS_IN_GAME || !lobby->game) {
        printf("Lobby %d is not in game\n", lobby->id);
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "No game in progress");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    // Vérifie que c'est bien le tour d'un espion
    if (lobby->game->state != GAMESTATE_TURN_RED_SPY && lobby->game->state != GAMESTATE_TURN_BLUE_SPY) {
        printf("It's not the spy's turn in lobby %d\n", lobby->id);
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "It's not the spy's turn");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    // Vérifie les arguments: nb_hint et hint_word
    if (!args_require(args, 2)) {
        printf("Invalid submit hint from client %d : \"%s\"\n", client->id, message);
        send_server_error(codenames, client, "Invalid hint format");
        return EXIT_FAILURE;
    }

    // Seul l'espion de l'équipe dont c'est le tour peut donner un indice
    if (user_can_act(lobby, client->id, ROLE_SPY) != EXIT_SUCCESS) {
        printf("Client %d is not the active spy in lobby %d\n", client->id, lobby->id);
        send_server_error(codenames, client, "You are not the spy of the active team");
        return EXIT_FAILURE;
    }

    int nb_hint = atoi((char*)args.argv[0]);
    char* hint_word = (char*)args.argv[1];

    /* Borne le nombre de tentatives : `can_guess` en découle directement et
       une valeur négative ou démesurée casserait la logique de tour. */
    if (nb_hint < 0 || nb_hint > lobby->game->nb_words) {
        send_server_error(codenames, client, "Invalid hint count");
        return EXIT_FAILURE;
    }

    printf("Client %d submitted hint: %s (%d)\n", client->id, hint_word, nb_hint);

    // Change le gamestate de SPY à AGENT
    GameState new_state;
    if (lobby->game->state == GAMESTATE_TURN_RED_SPY) {
        new_state = GAMESTATE_TURN_RED_AGENT;
    } else {
        new_state = GAMESTATE_TURN_BLUE_AGENT;
    }
    lobby->game->state = new_state;
    lobby->game->can_guess = nb_hint + 1;

    printf("Game state changed to %d in lobby %d\n", new_state, lobby->id);

    // Diffuse l'indice et le nouveau gamestate à tous les joueurs du lobby
    char msg[128];
    format_to(msg, sizeof(msg), "%d %d %d %s %d", MSG_SUBMIT_HINT, client->id, nb_hint, hint_word, new_state);
    for (int i = 0; i < lobby->nb_players; i++) {
        tcp_send_to_client(codenames, lobby->users[i]->id, msg);
    }

    return EXIT_SUCCESS;
}

Team active_team_of(GameState state) {
    switch (state) {
        case GAMESTATE_TURN_RED_SPY:
        case GAMESTATE_TURN_RED_AGENT:
            return TEAM_RED;
        case GAMESTATE_TURN_BLUE_SPY:
        case GAMESTATE_TURN_BLUE_AGENT:
            return TEAM_BLUE;
        default:
            return TEAM_NONE;
    }
}

int user_can_act(Lobby* lobby, int client_id, UserRole expected_role) {
    if (!lobby || !lobby->game) return EXIT_FAILURE;

    User* user = find_user_by_id(lobby, client_id);
    if (!user) return EXIT_FAILURE;

    Team active = active_team_of(lobby->game->state);
    if (active == TEAM_NONE) return EXIT_FAILURE;

    /* Le serveur fait autorité : ni l'équipe adverse ni un mauvais rôle
       ne doivent pouvoir agir, même si le client envoie une trame valide. */
    if (user->team != active) return EXIT_FAILURE;
    if (user->role != expected_role) return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

int count_remaining_words(Game* game, Team team) {
    int count = 0;
    for (int i = 0; i < game->nb_words; i++) {
        if (game->words[i].team == team && !game->words[i].revealed) {
            count++;
        }
    }
    return count;
}

int request_preguess(Codenames* codenames, TcpClient* client, char* message, Arguments args) {
    (void)message;

    // Vérifie que le client est bien dans un lobby
    Lobby* lobby = find_lobby_by_playerid(codenames->lobby, client->id);
    if (!lobby) {
        printf("Client %d is not in a lobby\n", client->id);
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "You must be in a lobby to pre-guess a card");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    // Vérifie que le lobby est bien en partie
    if (lobby->status != LB_STATUS_IN_GAME || !lobby->game) {
        printf("Lobby %d is not in game\n", lobby->id);
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "No game in progress");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    /* Un en-tête nu donne argc == 0 et argv == NULL : sans ce garde, l'accès
       ci-dessous déréférence NULL et fait tomber tout le serveur. */
    if (!args_require(args, 2)) {
        printf("Invalid preguess from client %d\n", client->id);
        send_server_error(codenames, client, "Invalid preguess format");
        return EXIT_FAILURE;
    }

    // Vérifie que c'est bien le tour d'un agent
    if (lobby->game->state != GAMESTATE_TURN_RED_AGENT && lobby->game->state != GAMESTATE_TURN_BLUE_AGENT) {
        send_server_error(codenames, client, "It's not the agent's turn");
        return EXIT_FAILURE;
    }

    // Seul un agent de l'équipe dont c'est le tour peut pré-sélectionner une carte
    if (user_can_act(lobby, client->id, ROLE_AGENT) != EXIT_SUCCESS) {
        send_server_error(codenames, client, "You are not an agent of the active team");
        return EXIT_FAILURE;
    }

    int word_index = atoi((char*)args.argv[0]);
    int selected = atoi((char*)args.argv[1]);

    if (word_index < 0 || word_index >= lobby->game->nb_words) {
        send_server_error(codenames, client, "Invalid word index");
        return EXIT_FAILURE;
    }
    selected = (selected != 0) ? 1 : 0;

    // Diffuse la carte sélectionné et le nouveau gamestate à tous les joueurs du lobby
    char msg[64];
    format_to(msg, sizeof(msg), "%d %d %d %d", MSG_PREGUESS, word_index, selected, client->id);
    for (int i = 0; i < lobby->nb_players; i++) {
        tcp_send_to_client(codenames, lobby->users[i]->id, msg);
    }

    return EXIT_SUCCESS;

}

int request_guess_card(Codenames* codenames, TcpClient* client, char* message, Arguments args) {
    // Vérifie que le client est bien dans un lobby
    Lobby* lobby = find_lobby_by_playerid(codenames->lobby, client->id);
    if (!lobby) {
        printf("Client %d is not in a lobby\n", client->id);
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "You must be in a lobby to guess a card");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    // Vérifie que le lobby est bien en partie
    if (lobby->status != LB_STATUS_IN_GAME || !lobby->game) {
        printf("Lobby %d is not in game\n", lobby->id);
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "No game in progress");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    User* guessing_user = find_user_by_id(lobby, client->id);

    // Vérifie que c'est bien le tour d'un agent
    if (lobby->game->state != GAMESTATE_TURN_RED_AGENT && lobby->game->state != GAMESTATE_TURN_BLUE_AGENT) {
        printf("It's not the agent's turn in lobby %d\n", lobby->id);
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "It's not the agent's turn");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    // Vérifie les arguments: card_index
    if (!args_require(args, 1)) {
        printf("Invalid guess card from client %d: \"%s\"\n", client->id, message);
        send_server_error(codenames, client, "Invalid card index");
        return EXIT_FAILURE;
    }

    // Seul un agent de l'équipe dont c'est le tour peut révéler une carte
    if (user_can_act(lobby, client->id, ROLE_AGENT) != EXIT_SUCCESS) {
        printf("Client %d is not an active agent in lobby %d\n", client->id, lobby->id);
        send_server_error(codenames, client, "You are not an agent of the active team");
        return EXIT_FAILURE;
    }

    /* Le nom est purement cosmétique : on ignore celui fourni par le client et on
       utilise celui enregistré côté serveur pour éviter l'usurpation d'identité. */
    const char* guessing_name = NULL;
    if ((!guessing_name || guessing_name[0] == '\0') && guessing_user && guessing_user->name && guessing_user->name[0] != '\0') {
        guessing_name = guessing_user->name;
    }
    if (!guessing_name || guessing_name[0] == '\0') {
        guessing_name = "Agent";
    }

    int word_index = atoi((char*)args.argv[0]);
    if (word_index < -1 || word_index >= lobby->game->nb_words) {
        printf("Invalid word index from client %d: %d\n", client->id, word_index);
        send_server_error(codenames, client, "Invalid word index");
        return EXIT_FAILURE;
    }

    if (lobby->game->can_guess <= 0) {
        printf("No guesses left for client %d in lobby %d\n", client->id, lobby->id);
        send_server_error(codenames, client, "No guesses left");
        return EXIT_FAILURE;
    }

    if (word_index == -1) {
        printf("Client %d ended their turn in lobby %d\n", client->id, lobby->id);
        // Change le gamestate de AGENT à SPY
        GameState new_state;
        if (lobby->game->state == GAMESTATE_TURN_RED_AGENT) {
            new_state = GAMESTATE_TURN_BLUE_SPY;
        } else {
            new_state = GAMESTATE_TURN_RED_SPY;
        }
        lobby->game->state = new_state;

        printf("Game state changed to %d in lobby %d\n", new_state, lobby->id);

        // Diffuse le nouveau gamestate à tous les joueurs du lobby
        char msg[128];
        format_to(msg, sizeof(msg), "%d %d %d %s", MSG_GUESS_CARD, -1, new_state, guessing_name);
        for (int i = 0; i < lobby->nb_players; i++) {
            tcp_send_to_client(codenames, lobby->users[i]->id, msg);
        }
        return EXIT_SUCCESS;
    }

    Word* word = lobby->game->words + word_index;

    // Vérifie que la carte n'est pas déjà révélée
    if (word->revealed) {
        printf("Word \"%s\" is already revealed\n", word->word);
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "Word is already revealed");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    // Révèle la carte
    lobby->game->can_guess--;
    word->revealed = 1;

    printf("Word \"%s\" guessed by client %d\n", word->word, client->id);

    Team winner = TEAM_NONE;

    // Change le gamestate de AGENT à SPY
    GameState new_state = lobby->game->state;
    Team current_team = (lobby->game->state == GAMESTATE_TURN_RED_AGENT) ? TEAM_RED : TEAM_BLUE;
    if (lobby->game->can_guess <= 0 || word->team != current_team) {
        if (lobby->game->state == GAMESTATE_TURN_RED_AGENT) {
            new_state = GAMESTATE_TURN_BLUE_SPY;
        } else if (lobby->game->state == GAMESTATE_TURN_BLUE_AGENT) {
            new_state = GAMESTATE_TURN_RED_SPY;
        }
    }
    if (word->team == TEAM_BLACK) {
        new_state = GAMESTATE_ENDED;
        winner = (current_team == TEAM_RED) ? TEAM_BLUE : TEAM_RED;
    }
    if (count_remaining_words(lobby->game, TEAM_RED) == 0) {
        new_state = GAMESTATE_ENDED;
        winner = TEAM_RED;
    }
    if (count_remaining_words(lobby->game, TEAM_BLUE) == 0) {
        new_state = GAMESTATE_ENDED;
        winner = TEAM_BLUE;
    }
    lobby->game->state = new_state;

    if (new_state == GAMESTATE_ENDED) {
        lobby->status = LB_STATUS_WAITING;
        lobby->game->can_guess = 0;
        printf("Lobby %d returned to waiting state after end of game\n", lobby->id);
    }

    printf("Game state changed to %d in lobby %d\n", new_state, lobby->id);

    // Diffuse la carte révélée et le nouveau gamestate à tous les joueurs du lobby
    char msg[128];
    format_to(msg, sizeof(msg), "%d %d %d %d %s", MSG_GUESS_CARD, word_index, new_state, winner, guessing_name);
    for (int i = 0; i < lobby->nb_players; i++) {
        tcp_send_to_client(codenames, lobby->users[i]->id, msg);
    }

    return EXIT_SUCCESS;
}

int request_set_words_difficulty(Codenames* codenames, TcpClient* client, char* message, Arguments args) {
    (void)message;
    
    // Vérifie que le client est bien propriétaire d'un lobby
    Lobby* lobby = find_lobby_by_ownerid(codenames->lobby, client->id);
    if (!lobby) {
        printf("Client %d doesn't own a lobby\n", client->id);
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "You must be the lobby owner to change difficulty");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    // Vérifie que le lobby n'est pas en partie
    if (lobby->status != LB_STATUS_WAITING) {
        printf("Cannot change difficulty while game is in progress in lobby %d\n", lobby->id);
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "Cannot change difficulty while game is in progress");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    // Récupère la difficulté demandée
    if (args.argc < 1) {
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "Missing difficulty argument");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    int words_difficulty = atoi((char*)args.argv[0]);
    if (words_difficulty != WORDS_DIFFICULTY_NORMAL && 
        words_difficulty != WORDS_DIFFICULTY_HARD && 
        words_difficulty != WORDS_DIFFICULTY_INFO && 
        words_difficulty != WORDS_DIFFICULTY_FREAKY
    ) {
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "Invalid difficulty value");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    lobby->words_difficulty = (WordsDifficulty)words_difficulty;
    printf("Lobby %d words difficulty set to %d\n", lobby->id, words_difficulty);

    // Informe tous les joueurs du changement de difficulté
    char msg[32];
    format_to(msg, sizeof(msg), "%d %d", MSG_SET_WORDS_DIFFICULTY, words_difficulty);
    for (int i = 0; i < lobby->nb_players; i++) {
        tcp_send_to_client(codenames, lobby->users[i]->id, msg);
    }

    return EXIT_SUCCESS;
}

int request_set_nb_assassins(Codenames* codenames, TcpClient* client, char* message, Arguments args) {
    (void)message;

    // Vérifie que le client est bien propriétaire d'un lobby
    Lobby* lobby = find_lobby_by_ownerid(codenames->lobby, client->id);
    if (!lobby) {
        printf("Client %d doesn't own a lobby\n", client->id);
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "You must be the lobby owner to change nb_assassins");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    // Vérifie que le lobby n'est pas en partie
    if (lobby->status != LB_STATUS_WAITING) {
        printf("Cannot change nb_assassins while game is in progress in lobby %d\n", lobby->id);
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "Cannot change nb_assassins while game is in progress");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    if (args.argc < 1) {
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "Missing nb_assassins argument");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    int nb_assassins = atoi((char*)args.argv[0]);
    if (nb_assassins < 1 || nb_assassins > 3) {
        char msg[64];
        format_to(msg, sizeof(msg), "%d %s", MSG_SERVER_ERROR, "Invalid nb_assassins value");
        tcp_send_to_client(codenames, client->id, msg);
        return EXIT_FAILURE;
    }

    lobby->nb_assassins = nb_assassins;
    printf("Lobby %d nb_assassins set to %d\n", lobby->id, nb_assassins);

    // Informe tous les joueurs du changement
    char msg[32];
    format_to(msg, sizeof(msg), "%d %d", MSG_SET_NB_ASSASSINS, nb_assassins);
    for (int i = 0; i < lobby->nb_players; i++) {
        tcp_send_to_client(codenames, lobby->users[i]->id, msg);
    }

    return EXIT_SUCCESS;
}
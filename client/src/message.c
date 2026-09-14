#include "../lib/all.h"

MessageType fetch_header(char* message) {
    int value = 0;
    /* sscanf renvoie EOF (-1) sur une chaîne vide et 0 si aucune conversion :
       les deux cas doivent donner MSG_UNKNOWN, sinon `header` reste non initialisé. */
    if (!message || sscanf(message, "%d", &value) != 1) return MSG_UNKNOWN;
    return (MessageType)value;
}

Arguments parse_arguments(char* message) {
    Arguments args = {0};
    char* token = strtok(message, " ");

    while (token != NULL) {
        char** resized = realloc(args.argv, (size_t)(args.argc + 1) * sizeof(char*));
        if (!resized) {
            /* Sans ce contrôle, l'échec de realloc écrasait args.argv par NULL
               puis était déréférencé immédiatement après. */
            free(args.argv);
            args.argv = NULL;
            args.argc = 0;
            return args;
        }
        args.argv = resized;
        args.argv[args.argc] = token;
        args.argc++;
        token = strtok(NULL, " ");
    }
    return args;
}

int args_require(Arguments args, int needed) {
    return args.argv != NULL && args.argc >= needed;
}

static SDL_Color message_hint_bar_team_color(Team team) {
    switch (team) {
        case TEAM_BLUE:
            return (SDL_Color){50, 80, 150, 200};
        case TEAM_RED:
            return (SDL_Color){150, 50, 50, 200};
        default:
            return COL_GRAY;
    }
}

static int message_find_user_slot_by_id(const Lobby* lobby, int user_id) {
    if (!lobby || user_id < 0) return -1;

    for (int i = 0; i < MAX_USERS; i++) {
        User* user = lobby->users[i];
        if (user && user->id == user_id) {
            return i;
        }
    }

    return -1;
}

static int message_find_free_user_slot(const Lobby* lobby) {
    if (!lobby) return -1;

    for (int i = 0; i < MAX_USERS; i++) {
        if (!lobby->users[i]) {
            return i;
        }
    }

    return -1;
}

static int message_update_user_name(User* user, const char* name) {
    if (!user || !name || name[0] == '\0') return EXIT_SUCCESS;

    if (user->name && strcmp(user->name, name) == 0) {
        return EXIT_SUCCESS;
    }

    char* copy = strdup(name);
    if (!copy) return EXIT_FAILURE;

    free(user->name);
    user->name = copy;
    return EXIT_SUCCESS;
}

static User* message_upsert_lobby_user(Lobby* lobby, int user_id, const char* name, UserRole role, Team team) {
    if (!lobby || user_id < 0) return NULL;

    /* Rôle et équipe viennent du réseau et servent ensuite à indexer les
       ressources d'affichage : on les ramène dans leur domaine valide. */
    if (role < ROLE_NONE || role > ROLE_AGENT) role = ROLE_NONE;
    if (team < TEAM_NONE || team > TEAM_BLACK) team = TEAM_NONE;

    int slot = message_find_user_slot_by_id(lobby, user_id);
    if (slot >= 0) {
        User* user = lobby->users[slot];
        if (!user) return NULL;

        if (message_update_user_name(user, name) != EXIT_SUCCESS) {
            return NULL;
        }

        user->role = role;
        user->team = team;
        return user;
    }

    int free_slot = message_find_free_user_slot(lobby);
    if (free_slot < 0) {
        return NULL;
    }

    const char* safe_name = (name && name[0] != '\0') ? name : "Unknown";
    User* created = create_user(user_id, safe_name, role, team);
    if (!created) {
        return NULL;
    }

    lobby->users[free_slot] = created;
    lobby->nb_players++;
    return created;
}

static void message_sync_local_user_in_lobby(AppContext* context) {
    if (!context || !context->lobby || context->player_id < 0) return;

    const char* local_name = (context->player_name && context->player_name[0] != '\0') ? context->player_name : "Unknown";
    if (!message_upsert_lobby_user(context->lobby, context->player_id, local_name, context->player_role, context->player_team)) {
        printf("Failed to synchronize local player %d in lobby user list\n", context->player_id);
    }
}

int on_message(AppContext* context, char* message) {
    MessageType header = fetch_header(message);

    /* Avance après l'en-tête et son espace, sans jamais dépasser le '\0' final. */
    size_t skip = (size_t)number_length((int)header) + 1;
    size_t available = strlen(message);
    message += (skip > available) ? available : skip;

    /* Keep a raw copy of the message (everything after the header)
       so we can print the full argument string later even though
       parse_arguments() will modify `message` with strtok(). */
    char* raw_message = strdup(message);
    Arguments args = parse_arguments(message);
    int status = EXIT_SUCCESS;

    switch (header) {

        case MSG_SERVER_ERROR:
            if (!args_require(args, 1)) {
                printf("Invalid error message from server: \"%s\"\n", message);
                status = EXIT_FAILURE;
                goto cleanup;
            }
            printf("Error from server: %s\n", (char*)args.argv[0]);
            break;
            
        case MSG_UNKNOWN: 
            printf("Received unknown message from server: \"%s\"\n", message);
            break;

        case MSG_INFO:
            if (!args_require(args, 1)) {
            printf("Invalid info message from server: \"%s\"\n", message);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            /* Print the entire argument string (everything after header) */
            printf("Info from server: %s\n", raw_message ? raw_message : "");

            break;

        case MSG_CREATELOBBY: // Confirmation de la création du lobby, avec l'id du lobby créé
            if (!args_require(args, 2)) {
                printf("Invalid create lobby message from server: \"%s\"\n", message);
                status = EXIT_FAILURE;
                goto cleanup;
            }
            struct_lobby_init(context->lobby, atoi((char*)args.argv[0]), (char*)args.argv[1]);
            // Le créateur du lobby en est automatiquement le propriétaire
            context->lobby->owner_id = context->player_id;
            message_sync_local_user_in_lobby(context);
            break;

        case MSG_JOINLOBBY:
            // Handle join lobby
            if (args_require(args, 2)) {
                struct_lobby_init(context->lobby, atoi((char*)args.argv[0]), (char*)args.argv[1]);
                message_sync_local_user_in_lobby(context);
            }
            break;

        case MSG_LEAVELOBBY:
            // Handle leave lobby
            context->lobby->id = -1;
            break;

        case MSG_LOBBYCLOSED:
            // Handle lobby closed
            context->lobby->id = -1;
            break;
        
        case MSG_PLAYERJOINED: {
            if (!args_require(args, 4)) {
                printf("Invalid player joined message from server: \"%s\"\n", message);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            int joined_id = atoi((char*)args.argv[0]);
            const char* joined_name = (char*)args.argv[1];
            UserRole joined_role = (UserRole)atoi((char*)args.argv[2]);
            Team joined_team = (Team)atoi((char*)args.argv[3]);

            if (!message_upsert_lobby_user(context->lobby, joined_id, joined_name, joined_role, joined_team)) {
                printf("Failed to add/update player %d in lobby\n", joined_id);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            printf("Player %d (%s) joined the lobby with role %d and team %d\n", joined_id, joined_name ? joined_name : "Unknown", joined_role, joined_team);
            break;
        }
        
        case MSG_PLAYERLEFT: {
            if (!args_require(args, 1)) {
                printf("Invalid player left message from server: \"%s\"\n", message);
                status = EXIT_FAILURE;
                goto cleanup;
            }
            int player_id = atoi((char*)args.argv[0]);
            for (int i = 0; i < MAX_USERS; i++) {
                if (context->lobby->users[i] && context->lobby->users[i]->id == player_id) {
                    destroy_user(context->lobby->users[i]);
                    context->lobby->users[i] = NULL;
                    context->lobby->nb_players--;
                    break;
                }
            }
            printf("Player %d left the lobby\n", player_id);
            break;
        }

        case MSG_CHOOSE_ROLE: {
            if (!args_require(args, 3)) {
                printf("Invalid choose role message from server : \"%s\"\n", message);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            int target_id = atoi((char*)args.argv[0]);
            UserRole target_role = (UserRole)atoi((char*)args.argv[1]);
            Team target_team = (Team)atoi((char*)args.argv[2]);
            const char* target_name = find_player_by_id(context->lobby, target_id);
            if (!target_name && context->player_id == target_id) {
                target_name = context->player_name;
            }

            if (!message_upsert_lobby_user(context->lobby, target_id, target_name, target_role, target_team)) {
                printf("Failed to update role/team for player %d\n", target_id);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            if (context->player_id == target_id) {
                context->player_role = target_role;
                context->player_team = target_team;
            }

            printf("Player %d chose role %d team %d\n", target_id, target_role, target_team);
            
            break;
        }

        case MSG_STARTGAME: {
            if (!args_require(args, 2)) {
                printf("Invalid start game message from server: \"%s\"\n", message);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            if (context->lobby->game) game_struct_free(context);

            Game* game = (Game*)calloc(1, sizeof(Game));
            if (!game) {
                printf("Failed to allocate memory for game\n");
                status = EXIT_FAILURE;
                goto cleanup;
            }
            game->state = (GameState)atoi((char*)args.argv[0]);
            game->nb_words = atoi((char*)args.argv[1]);

            /* Valeur venue du réseau : sans borne, sizeof(Card) * nb_words
               déborde et renvoie un tampon trop petit pour les MSG_WORDDATA. */
            if (game->nb_words < 1 || game->nb_words > GAME_MAX_WORDS) {
                printf("Invalid word count from server: %d\n", game->nb_words);
                free(game);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            game->current_hint[0] = '\0';
            game->current_hint_count = 0;
            game->winner = TEAM_NONE;
            history_reset(&game->red_history);
            history_reset(&game->blue_history);
            printf("Starting game with state %d and %d words\n", game->state, game->nb_words);
            game->cards = (Card*)calloc((size_t)game->nb_words, sizeof(Card));
            if (!game->cards) {
                printf("Failed to allocate memory for game cards\n");
                free(game);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            context->lobby->game = game;

            printf("Game started with status %d\n", game->state);
            break;
        }

        case MSG_WORDDATA: {
            if (!args_require(args, 5)) {
                printf("Invalid word data message from server: \"%s\"\n", message);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            /* Une grille doit avoir été annoncée par MSG_STARTGAME au préalable. */
            if (!context->lobby || !context->lobby->game || !context->lobby->game->cards) {
                printf("Received word data before the game was started, ignoring\n");
                status = EXIT_FAILURE;
                goto cleanup;
            }

            int wordid = atoi((char*)args.argv[0]);
            char* word = (char*)args.argv[1];
            Team team = (Team)atoi((char*)args.argv[2]);
            CardType type = (CardType)atoi((char*)args.argv[3]);
            int revealed = atoi((char*)args.argv[4]);

            /* Index piloté par le serveur : sans ce contrôle il sert d'écriture
               arbitraire dans le tas. */
            if (wordid < 0 || wordid >= context->lobby->game->nb_words) {
                printf("Invalid card index from server: %d\n", wordid);
                status = EXIT_FAILURE;
                goto cleanup;
            }
            if (team < TEAM_NONE || team > TEAM_BLACK) team = TEAM_NONE;
            if (type < CT_MALE || type > CT_DOG) type = CT_MALE;

            printf("Word data received: %s (Team: %d, Type: %d, Revealed: %d)\n", word, team, type, revealed);

            // Prétraitement qui remet les espaces
            for (int i = 0; word[i] != '\0'; i++) {
                if (word[i] == '_') word[i] = ' ';
            }

            Card* card = &context->lobby->game->cards[wordid];
            /* Copie bornée : le mot vient du réseau et peut dépasser word[32]. */
            snprintf(card->word, sizeof(card->word), "%s", word);
            card->team = team;
            card->type = type;
            card->revealed = (revealed != 0);
            card->selected = False;
            card->is_pressed = False;
            card->is_hovered = False;
            card->display_word_once_revealed = False;

            if (wordid == context->lobby->game->nb_words - 1) {
                context->app_state = APP_STATE_PLAYING;
            }

            break;
        }

        case MSG_SUBMIT_HINT: {
            if (!args_require(args, 4)) {
                printf("Invalid submit hint message from server: \"%s\"\n", message);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            int spy_id = atoi((char*)args.argv[0]);
            int nb_guesses = atoi((char*)args.argv[1]);
            char* hint = (char*)args.argv[2];
            GameState new_state = (GameState)atoi((char*)args.argv[3]);
            GameState previous_state = GAMESTATE_WAITING;

            const char* spy_name = find_player_by_id(context->lobby, spy_id); //Récupérer le nom de l'espion

            if (context->lobby && context->lobby->game) {
                previous_state = context->lobby->game->state;
            }

            Team active_team = history_team_from_agent_state(new_state);

            printf("Hint received from client %d (%s) : %s with %d guesses, new state: %d\n", spy_id, spy_name ? spy_name : "l'espion", hint, nb_guesses, new_state);

            // Stocker l'indice et mettre à jour le gamestate
            if (context->lobby && context->lobby->game) {
                strncpy(context->lobby->game->current_hint, hint, sizeof(context->lobby->game->current_hint) - 1);
                context->lobby->game->current_hint[sizeof(context->lobby->game->current_hint) - 1] = '\0';
                context->lobby->game->current_hint_count = nb_guesses;
                context->lobby->game->state = new_state;

                if (active_team != TEAM_NONE) {
                    int should_start_turn = 0;
                    if (active_team == TEAM_RED && previous_state == GAMESTATE_TURN_RED_SPY) {
                        should_start_turn = 1;
                    } else if (active_team == TEAM_BLUE && previous_state == GAMESTATE_TURN_BLUE_SPY) {
                        should_start_turn = 1;
                    } else {
                        History* history = history_get_for_team(context->lobby->game, active_team);
                        if (!history || history->turn_count <= 0) {
                            should_start_turn = 1;
                        }
                    }

                    if (should_start_turn) {
                        history_start_turn(context, active_team, hint, nb_guesses);
                    } else {
                        history_ensure_turn(context, active_team, hint, nb_guesses);
                    }

                    /* Le serveur fournit le nom de l'espion: on l'applique systématiquement au dernier tour. */
                    History* history = history_get_for_team(context->lobby->game, active_team);
                    if (history && history->turn_count > 0) {
                        history_update_last_turn(history, spy_name, hint, nb_guesses);
                    }
                }

                int is_submitting_spy = 0;
                if (context->player_id >= 0 && context->player_id == spy_id) {
                    is_submitting_spy = 1;
                } else if (context->player_name && spy_name && strcmp(context->player_name, spy_name) == 0) {
                    is_submitting_spy = 1;
                }

                if (!is_submitting_spy) {
                    char hint_feedback[GAME_HINTBAR_TEXT_LEN];
                    format_to(
                        hint_feedback,
                        sizeof(hint_feedback),
                        "Indice reçu de %s",
                        spy_name ? spy_name : "l'espion"
                    );
                    game_hint_bar_set_feedback(
                        context,
                        hint_feedback,
                        message_hint_bar_team_color(active_team),
                        GAME_HINTBAR_PRIORITY_INFO,
                        GAME_HINTBAR_FEEDBACK_INFO_MS
                    );
                }
            }
            
            break;
        }

        case MSG_PREGUESS: {
            if (!args_require(args, 3)) {
                printf("Invalid preguess message from server: \"%s\"\n", message);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            printf("Pre-guess update received: \"%s\"\n", raw_message ? raw_message : "");

            int word_index = atoi((char*)args.argv[0]);
            int selected = atoi((char*)args.argv[1]);
            int playerid = atoi((char*)args.argv[2]);
            (void)playerid; // Utile plus tard

            printf("Pre-guess update received for card %d: selected = %d\n", word_index, selected);

            // Mettre à jour la carte
            if (context->lobby && context->lobby->game && word_index >= 0 && word_index < context->lobby->game->nb_words) {
                Card* card = &context->lobby->game->cards[word_index];
                card->selected = selected;
            }
            
            break;
        }

        case MSG_GUESS_CARD: {
            if (!args_require(args, 2)) {
                printf("Invalid guess card message from server: \"%s\"\n", message);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            /* Ce message n'a de sens qu'avec une partie en cours : les accès
               ci-dessous déréférençaient game sans le vérifier. */
            if (!context->lobby || !context->lobby->game) {
                printf("Received guess card without an active game, ignoring\n");
                status = EXIT_FAILURE;
                goto cleanup;
            }

            int word_index = atoi((char*)args.argv[0]);
            GameState new_state = (GameState)atoi((char*)args.argv[1]);
            const char* guessing_agent_name = NULL;
            if (args_require(args, 4)) {
                guessing_agent_name = (char*)args.argv[3];
            } else if (word_index == -1 && args.argc >= 3) {
                guessing_agent_name = (char*)args.argv[2];
            }
            Team active_team = history_team_from_agent_state(context->lobby->game->state);

            // Partie terminée
            if (args.argc >= 3 && new_state == GAMESTATE_ENDED) {
                context->lobby->game->winner = (Team)atoi((char*)args.argv[2]);

                if (
                    (context->lobby->game->winner == TEAM_RED && context->player_team == TEAM_RED) ||
                    (context->lobby->game->winner == TEAM_BLUE && context->player_team == TEAM_BLUE)
                ) {
                    char nb_win[16];
                    read_property(nb_win, sizeof(nb_win), "WIN_COUNT");
                    int win_count = strcmp(nb_win, "")!=0 ? atoi(nb_win) + 1 : 1;
                    format_to(nb_win, sizeof(nb_win), "%d", win_count);
                    write_property("WIN_COUNT", nb_win);
                }

                char end_feedback[GAME_HINTBAR_TEXT_LEN];
                const char* winner_label = "inconnue";
                if (context->lobby->game->winner == TEAM_BLUE) {
                    winner_label = "bleue";
                } else if (context->lobby->game->winner == TEAM_RED) {
                    winner_label = "rouge";
                }
                format_to(
                    end_feedback,
                    sizeof(end_feedback),
                    "Victoire de l'equipe %s !",
                    winner_label
                );
                game_hint_bar_set_feedback(
                    context,
                    end_feedback,
                    message_hint_bar_team_color(context->lobby->game->winner),
                    GAME_HINTBAR_PRIORITY_INFO,
                    GAME_HINTBAR_FEEDBACK_SUCCESS_MS
                );
                
            } 

            if (new_state != context->lobby->game->state) {
                for (int i = 0; i < context->lobby->game->nb_words; i++) {
                    context->lobby->game->cards[i].selected = False;
                }
            }

            printf(
                "Card guessed: %d by %s, new state: %d\n",
                word_index,
                guessing_agent_name ? guessing_agent_name : "unknown",
                new_state
            );

            // Mettre à jour la carte et le gamestate
            if (context->lobby && context->lobby->game) {
                if (word_index >= 0 && word_index < context->lobby->game->nb_words) {
                    Card* guessed_card = &context->lobby->game->cards[word_index];

                    if (active_team != TEAM_NONE) {
                        history_append_revealed_word(
                            context,
                            active_team,
                            guessed_card->word,
                            guessed_card->team,
                            guessing_agent_name
                        );
                    }

                    guessed_card->revealed = True;
                    guessed_card->is_hovered = False;
                    guessed_card->selected = False;
                } else if (word_index == -1 && active_team != TEAM_NONE) {
                    history_ensure_turn(
                        context,
                        active_team,
                        context->lobby->game->current_hint,
                        context->lobby->game->current_hint_count
                    );

                    char turn_feedback[GAME_HINTBAR_TEXT_LEN];
                    format_to(
                        turn_feedback,
                        sizeof(turn_feedback),
                        "Tour termine par %s",
                        guessing_agent_name ? guessing_agent_name : "un agent"
                    );
                    game_hint_bar_set_feedback(
                        context,
                        turn_feedback,
                        message_hint_bar_team_color(active_team),
                        GAME_HINTBAR_PRIORITY_INFO,
                        GAME_HINTBAR_FEEDBACK_INFO_MS
                    );
                }
                context->lobby->game->state = new_state;
            }
            
            break;
        }

        case MSG_SENDCHAT: {
            if (!args_require(args, 2)) {
                printf("Invalid chat message from server: \"%s\"\n", message);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            int sender_id = -1;
            int message_start = 1;
            char* sender = (char*)args.argv[0];

            if (args_require(args, 3)) {
                char* id_end = NULL;
                long parsed_id = strtol((char*)args.argv[0], &id_end, 10);
                if (id_end && *id_end == '\0' && parsed_id >= 0 && parsed_id <= 2147483647L) {
                    sender_id = (int)parsed_id;
                    sender = (char*)args.argv[1];
                    message_start = 2;
                }
            }

            char chat_message[448];
            chat_message[0] = '\0';
            for (int i = message_start; i < args.argc; i++) {
                if (i > message_start) {
                    strncat(chat_message, " ", sizeof(chat_message) - strlen(chat_message) - 1);
                }
                strncat(chat_message, (char*)args.argv[i], sizeof(chat_message) - strlen(chat_message) - 1);
            }

            char full_message[512];
            if (sender_id >= 0) {
                format_to(full_message, sizeof(full_message), "%d|%s : %s", sender_id, sender, chat_message);
            } else {
                format_to(full_message, sizeof(full_message), "%s : %s", sender, chat_message);
            }
            if (chat_push(&context->lobby->chat, full_message) != EXIT_SUCCESS) {
                printf("Failed to store chat message in lobby history\n");
            }

            printf("%s : %s\n", sender, chat_message);
            
            break;
        }

        case MSG_REQUESTUUID: {
            // Réception de l'UUID généré par le serveur
            if (!args_require(args, 1)) {
                printf("Invalid UUID message from server: \"%s\"\n", message);
                status = EXIT_FAILURE;
                goto cleanup;
            }
            // Stocker l'UUID dans le contexte
            if (context->player_uuid) free(context->player_uuid);
            context->player_uuid = strdup((char*)args.argv[0]);

            // Écrire le fichier data/uuid
            
            FILE* f = fopen("data/uuid", "w");
            if (f) {
                fprintf(f, "NE PAS MODIFIER CE FICHIER !\n%s\n", context->player_uuid);
                fclose(f);
                printf("UUID saved to data/uuid: %s\n", context->player_uuid);
            } else {
                perror("Failed to write data/uuid");
            }
            
            break;
        }

        case MSG_SEND_CLIENT_ID: {
            if (!args_require(args, 1)) {
                printf("Invalid client id message from server: \"%s\"\n", message);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            context->player_id = atoi((char*)args.argv[0]);
            message_sync_local_user_in_lobby(context);
            printf("Assigned client ID: %d\n", context->player_id);

            break;
        }

        case MSG_PING: {
            if (!args_require(args, 1)) {
                break;
            }
            
            Uint32 sent_at = (Uint32)strtoul((char*)args.argv[0], NULL, 10);
            Uint32 now = SDL_GetTicks();
            context->ping_ms = (int)(now - sent_at);
            
            break;
        }

        case MSG_SET_WORDS_DIFFICULTY: {
            if (!args_require(args, 1)) {
                printf("Invalid set words difficulty message from server: \"%s\"\n", message);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            int words_difficulty = atoi((char*)args.argv[0]);
            context->lobby->words_difficulty = (WordsDifficulty)words_difficulty;
            printf("Lobby words difficulty changed to %s\n", words_difficulty == WORDS_DIFFICULTY_HARD ? "hard" : "easy");

            break;
        }

        case MSG_SET_NB_ASSASSINS: {
            if (!args_require(args, 1)) {
                printf("Invalid set nb_assassins message from server: \"%s\"\n", message);
                status = EXIT_FAILURE;
                goto cleanup;
            }

            int nb_assassins = atoi((char*)args.argv[0]);
            if (nb_assassins < 1 || nb_assassins > 3) {
                printf("Invalid nb_assassins value from server: %d\n", nb_assassins);
                break;
            }

            if (context->lobby) {
                context->lobby->nb_assassins = nb_assassins;
            }
            printf("Lobby nb_assassins changed to %d\n", nb_assassins);

            break;
        }

        default:
            printf("Received unhandled message type %d from server: \"%s\"\n", header, message);
            break;
    };

cleanup:
    if (args.argv) free(args.argv);
    if (raw_message) free(raw_message);

    return status;
}
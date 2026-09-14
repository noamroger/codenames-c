#include "../lib/all.h"

/** Passe à 0 sur SIGINT/SIGTERM pour sortir proprement de la boucle principale. */
static volatile sig_atomic_t server_running = 1;

#ifndef _WIN32
static void handle_shutdown_signal(int signum) {
    (void)signum;
    server_running = 0;
}
#endif

int main(int argc, char* argv[]) {

#ifndef _WIN32
    /* Sans cela, écrire vers un client parti brutalement (RST) lève SIGPIPE et
       termine le processus : n'importe quel joueur pouvait tuer le serveur et
       toutes les parties en cours en se déconnectant sèchement. */
    signal(SIGPIPE, SIG_IGN);

    /* Arrêt propre : ferme les lobbies et libère les ressources au lieu de
       laisser le noyau tuer le processus (le code de nettoyage placé après la
       boucle était jusqu'ici inatteignable). */
    signal(SIGINT, handle_shutdown_signal);
    signal(SIGTERM, handle_shutdown_signal);
#endif

    int port = 0;
    // Parse command line arguments
#ifdef _WIN32
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Usage: %s [-p port]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }
#else
    int opt;
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-p port]\n", argv[0]);
                return EXIT_FAILURE;
        }
    }
#endif
    if (port == 0) {
        fprintf(stderr, "Port number is required. Usage: %s [-p port]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Initialisations diverses
    srand(time(NULL));

    /* Sans listes de mots chargées, aucune partie ne peut démarrer : mieux vaut
       refuser de démarrer que planter au premier lancement de partie. */
    if (init_game_manager() != EXIT_SUCCESS) {
        fprintf(stderr, "Failed to load word lists from assets/. Aborting.\n");
        return EXIT_FAILURE;
    }

    Codenames* codenames = malloc(sizeof(Codenames));
    if (codenames == NULL) {
        perror("Failed to create Codenames");
        return EXIT_FAILURE;
    }

    load_version(codenames);

    printf("Starting the game server %s on port %d...\n", codenames->version, port);

    // Démarrer le serveur
    codenames->tcp = tcp_server_create(port);
    if (codenames->tcp == NULL) {
        free(codenames);
        perror("Failed to create TCP server");
        return EXIT_FAILURE;
    }

    codenames->lobby = create_lobby_manager();
    if (codenames->lobby == NULL) {
        tcp_server_destroy(codenames->tcp);
        free(codenames);
        perror("Failed to create LobbyManager");
        return EXIT_FAILURE;
    }

    // Boucle d'execution
    while (server_running) {
        tcp_server_tick(codenames);
    }

    // Cleanup
    printf("\nShutting down, closing lobbies...\n");
    destroy_lobby_manager(codenames, codenames->lobby);
    tcp_server_destroy(codenames->tcp);
    destroy_game_manager();
    free(codenames);

    printf("Server shutting down.\n");

    return EXIT_SUCCESS;
}
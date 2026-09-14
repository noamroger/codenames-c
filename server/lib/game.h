/**
 * @file game.h
 * @brief Logique de la partie de Codenames côté serveur.
 */

#ifndef GAME_H
#define GAME_H

#include "../lib/message.h"

/**
 * Niveaux de difficulté du jeu.
 * @param WORDS_DIFFICULTY_NORMAL Difficulté normale (wordlist.txt).
 * @param WORDS_DIFFICULTY_HARD Difficulté difficile (wordlist_hard.txt).
 * @param WORDS_DIFFICULTY_INFO Difficulté informatique (wordlist_info.txt).
 * @param WORDS_DIFFICULTY_FREAKY Difficulté décalée (wordlist_freaky.txt).
 */
typedef enum WordsDifficulty {
    WORDS_DIFFICULTY_NORMAL,
    WORDS_DIFFICULTY_HARD,
    WORDS_DIFFICULTY_INFO,
    WORDS_DIFFICULTY_FREAKY
} WordsDifficulty;

/**
 * TEAM est utilisé à la fois pour catégoriser les mots dans la grille et pour assigner les joueurs à une équipe.
 * Catégories de mots dans la grille de Codenames.
 * Les mots sont classés en 4 catégories :
 * @param TEAM_NONE mot neutre (aucune équipe).
 * @param TEAM_RED mot appartenant à l'équipe rouge.
 * @param TEAM_BLUE mot appartenant à l'équipe bleue.
 * @param TEAM_BLACK mot assassin (met fin à la partie si révélé).
 */
typedef enum Team {
    TEAM_NONE,
    TEAM_RED,
    TEAM_BLUE,
    TEAM_BLACK
} Team;

/**
 * Types de cartes pour l'affichage (masculin, féminin, etc.). Ne correspond pas à une logique de jeu, mais uniquement à l'affichage.
 * Permet d'avoir des cartes avec des formes différentes selon le type, pour une meilleure lisibilité.
 * @param CT_MALE carte de type masculin.
 * @param CT_FEMALE carte de type féminin.
 * @param CT_CAT carte de type chat.
 * @param CT_DOG carte de type chien.
 */
typedef enum CardType {
    CT_MALE,
    CT_FEMALE,
    CT_CAT,
    CT_DOG,
} CardType;

/**
 * Représente un mot dans la grille de Codenames.
 *
 * @param word texte du mot (terminé par \0).
 * @param team équipe à laquelle le mot appartient (TEAM_*).
 * @param revealed 0 si caché, 1 si révélé.
 */
typedef struct {
    char word[32];
    Team team;
    CardType type;
    int revealed;
} Word;

/**
 * États possibles d'une partie.
 * @param GAMESTATE_WAITING en attente de joueurs / démarrage.
 * @param GAMESTATE_TURN_RED_SPY tour de l'espion rouge.
 * @param GAMESTATE_TURN_RED_AGENT tour de l'agent rouge.
 * @param GAMESTATE_TURN_BLUE_SPY tour de l'espion bleu.
 * @param GAMESTATE_TURN_BLUE_AGENT tour de l'agent bleu.
 * @param GAMESTATE_ENDED partie terminée.
 */
typedef enum GameState {
    GAMESTATE_WAITING,
    GAMESTATE_TURN_RED_SPY,
    GAMESTATE_TURN_RED_AGENT,
    GAMESTATE_TURN_BLUE_SPY,
    GAMESTATE_TURN_BLUE_AGENT,
    GAMESTATE_ENDED
} GameState;

/**
 * Représente une partie de Codenames.
 * @param words tableau dynamique de Word (taille nb_words).
 * @param nb_words nombre de mots dans la grille.
 * @param can_guess Nombre de mots devinables par les agents dans le tour en cours (déterminé par l'indice donné par l'espion).
 * @param state état courant de la partie (GAMESTATE_*).
 */
typedef struct {
    Word* words;
    int nb_words;
    int can_guess;
    GameState state;
} Game;

/** Nombre de mots composant une grille de Codenames. */
#define GAME_NB_WORDS 25

/**
 * Initialise le gestionnaire de parties (structures internes, RNG, etc.).
 * @return EXIT_SUCCESS en cas de succès, EXIT_FAILURE en cas d'erreur.
 */
int init_game_manager();

/**
 * Retourne l'équipe dont c'est le tour pour un état de partie donné.
 * @param state État courant de la partie.
 * @return TEAM_RED, TEAM_BLUE, ou TEAM_NONE si aucune équipe n'est active.
 */
Team active_team_of(GameState state);

/**
 * Libère une partie et sa grille de mots. Tolère un pointeur NULL.
 * @param game Partie à détruire.
 * @return EXIT_SUCCESS si la partie a été libérée, EXIT_FAILURE si game est NULL.
 */
int destroy_game(Game* game);

/**
 * Libère les listes de mots chargées par init_game_manager().
 */
void destroy_game_manager(void);

/**
 * Génère un tableau de mots pour une partie.
 * Les mots sont sélectionnés aléatoirement et associés à une équipe.
 * @param count Le nombre de mots à générer.
 * @param start_team L'équipe qui commence la partie.
 * @param difficulty Niveau de difficulté de la partie.
 * @return Un tableau de Word contenant les mots générés,
 *         ou NULL en cas d'erreur. La gestion mémoire est à la
 *         charge de l'appelant.
 */
Word* generateWords(int count, Team start_team, WordsDifficulty difficulty, int nb_assassins);

/**
 * Mélange un tableau de mots in-place (Fisher-Yates).
 * @param words Tableau de Word à mélanger.
 * @param count Nombre d'éléments dans le tableau.
 */
void shuffleWords(Word* words, int count);

/**
 * Traite la demande de démarrage de partie.
 * @param codenames Contexte principal du serveur.
 * @param client Client TCP ayant envoyé la demande.
 * @param message Message brut reçu du client.
 * @param args Arguments extraits du message.
 * @return EXIT_SUCCESS en cas de succès, EXIT_FAILURE en cas d'erreur.
 */
int request_start_game(Codenames* codenames, TcpClient* client, char* message, Arguments args);

/**
 * Traite la soumission d'un indice par un espion.
 * Le serveur redistribue l'indice à tous les autres joueurs du lobby.
 * @param codenames Contexte principal du serveur.
 * @param client Client TCP ayant envoyé la demande (l'espion).
 * @param message Message brut reçu du client.
 * @param args Arguments extraits du message (nb_hint, hint_word).
 * @return EXIT_SUCCESS en cas de succès, EXIT_FAILURE en cas d'erreur.
 */
int request_submit_hint(Codenames* codenames, TcpClient* client, char* message, Arguments args);

/**
 * Traite la pré-sélection d'une carte par un agent.
 * Le serveur vérifie que c'est bien le tour de l'agent et redistribue la sélection à tous les autres joueurs du lobby.
 * @param codenames Contexte principal du serveur.
 * @param client Client TCP ayant envoyé la demande (l'agent).
 * @param message Message brut reçu du client.
 * @param args Arguments extraits du message (card_index, selected).
 * @return EXIT_SUCCESS en cas de succès, EXIT_FAILURE en cas d'erreur.
 */
int request_preguess(Codenames* codenames, TcpClient* client, char* message, Arguments args);

/**
 * Traite la soumission d'une carte devinée par un agent.
 * Le serveur vérifie la validité de la carte, met à jour l'état du jeu et diffuse les changements à tous les joueurs du lobby.
 * @param codenames Contexte principal du serveur.
 * @param client Client TCP ayant envoyé la demande (l'agent).
 * @param message Message brut reçu du client.
 * @param args Arguments extraits du message (card_index).
 * @return EXIT_SUCCESS en cas de succès, EXIT_FAILURE en cas d'erreur.
 */
int request_guess_card(Codenames* codenames, TcpClient* client, char* message, Arguments args);

/**
 * Traite la demande de changement de difficulté d'un lobby.
 * Seul le propriétaire du lobby peut changer la difficulté.
 * @param codenames Contexte principal du serveur.
 * @param client Client TCP ayant envoyé la demande.
 * @param message Message brut reçu du client.
 * @param args Arguments extraits du message (difficulty: 0=facile, 1=difficile).
 * @return EXIT_SUCCESS en cas de succès, EXIT_FAILURE en cas d'erreur.
 */
int request_set_words_difficulty(Codenames* codenames, TcpClient* client, char* message, Arguments args);
int request_set_nb_assassins(Codenames* codenames, TcpClient* client, char* message, Arguments args);

#endif // GAME_H
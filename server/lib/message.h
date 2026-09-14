/**
 * @file message.h
 * @brief Définition des types de messages et traitement des messages réseau côté serveur.
 */

#ifndef MESSAGE_H
#define MESSAGE_H

#include "../lib/tcp.h"

/**
 * Types de messages harmonisé client/serveur (aussi appelé HEADER) 
 * @param MSG_SERVER_ERROR message d'erreur envoyé par le serveur en cas de problème (ex: format de message invalide, action interdite, etc.). Le message d'erreur est suivi d'une description de l'erreur pour aider au debug. 
 * @param MSG_UNKNOWN message inconnu.
 * @param MSG_INFO message d'information (pour des messages non critiques ou des mises à jour de statut).
 * @param MSG_CREATELOBBY message de création de lobby.
 * @param MSG_JOINLOBBY message de rejoindre un lobby.
 * @param MSG_LEAVELOBBY message de quitter un lobby.
 * @param MSG_LOBBYCLOSED message de fermeture de lobby.
 * @param MSG_PLAYERJOINED message d'un joueur qui a rejoint le lobby.
 * @param MSG_PLAYERLEFT message d'un joueur qui a quitté le lobby.
 * @param MSG_CHOOSE_ROLE message de choix de rôle/équipe dans le lobby (pour informer les autres joueurs en temps réel du choix d'un joueur).
 * @param MSG_STARTGAME message de démarrage de partie. 
 * @param MSG_WORDDATA message de données de mots pour la partie (envoyé par le serveur au démarrage de la partie, contient la liste des mots et leur couleur). 
 * @param MSG_SUBMIT_HINT message de soumission du mot indice et du nombre de mots indices.
 * @param MSG_PREGUESS message de pré-guess (envoyé par le client au serveur pour indiquer la carte sur laquelle le joueur a cliqué, et relayé par le serveur à tous les clients pour indiquer quelle carte est en cours de guess).
 * @param MSG_GUESS_CARD message de guess de carte (envoyé par le client au serveur pour indiquer la carte choisie, et relayé par le serveur à tous les clients pour indiquer quelle carte a été guessée).
 * @param MSG_SENDCHAT message d'envoi de message dans le chat.
 * @param MSG_REQUESTUUID message de demande d'UUID (pour le client de demander son UUID au serveur après connexion, si besoin pour le protocole).
 * @param MSG_COMPAREVERSION message de comparaison de version (pour le client de comparer sa version avec celle du serveur).
 * @param MSG_PING message d'echo client/serveur pour mesurer le ping applicatif en temps réel.
 * @param MSG_SEND_CLIENT_ID message envoyé automatiquement par le serveur à la connexion, contient l'identifiant numérique du client.
 * @param MSG_SET_WORDS_DIFFICULTY message pour changer la difficulté de la partie (0 = facile, 1 = difficile).
 * @param MSG_SET_NB_ASSASSINS message pour changer le nombre d'assassins dans la partie (1..3).
 */
typedef enum MessageType {
    MSG_SERVER_ERROR = -2,
    MSG_UNKNOWN = -1,
    MSG_INFO,
    MSG_CREATELOBBY,
    MSG_JOINLOBBY,
    MSG_LEAVELOBBY,
    MSG_LOBBYCLOSED,
    MSG_PLAYERJOINED,
    MSG_PLAYERLEFT,
    MSG_CHOOSE_ROLE,
    MSG_STARTGAME,
    MSG_WORDDATA,
    MSG_SUBMIT_HINT,
    MSG_PREGUESS,
    MSG_GUESS_CARD,
    MSG_SENDCHAT,
    MSG_REQUESTUUID,
    MSG_COMPAREVERSION,
    MSG_PING,
    MSG_SEND_CLIENT_ID,
    MSG_SET_WORDS_DIFFICULTY,
    MSG_SET_NB_ASSASSINS,
} MessageType;

/**
 * Codes d'erreur de message.
 * @param MSG_ERR_NONE Aucune erreur.
 * @param MSG_ERR_INVALID_FORMAT Format de message invalide.
 * @param MSG_ERR_UNKNOWN_TYPE Type de message inconnu.
 */
typedef enum MessageError {
    MSG_ERR_NONE,
    MSG_ERR_INVALID_FORMAT,
    MSG_ERR_UNKNOWN_TYPE
} MessageError;

/**
 * Structure pour stocker les arguments d'un message.
 * @param argc nombre d'arguments.
 * @param argv tableau de pointeurs vers les arguments.
 */
typedef struct Arguments {
    int argc;
    char** argv;
} Arguments;

/**
 * Extrait les arguments d'un message brut.
 * @param message Message brut reçu du client.
 * @return Structure Arguments contenant les arguments extraits.
 */
Arguments parse_arguments(char* message);

/**
 * Vérifie qu'un message contient au moins `needed` arguments exploitables.
 * À appeler avant tout accès à args.argv[i] : un client peut toujours envoyer
 * un en-tête nu, auquel cas argv vaut NULL et argc vaut 0.
 * @param args Arguments extraits du message.
 * @param needed Nombre minimal d'arguments attendus.
 * @return 1 si les arguments sont utilisables, 0 sinon.
 */
int args_require(Arguments args, int needed);

/* Forward declaration to avoid circular include with codenames.h */
typedef struct Codenames Codenames;

/**
 * Envoie un message d'erreur applicatif (MSG_SERVER_ERROR) à un client.
 * @param codenames Contexte principal du serveur.
 * @param client Client destinataire.
 * @param reason Description courte de l'erreur.
 */
void send_server_error(Codenames* codenames, TcpClient* client, const char* reason);

/**
 * Traite un message entrant côté serveur.
 * @param codenames Contexte principal du serveur.
 * @param client Client TCP ayant envoyé le message.
 * @param message Message brut reçu du client.
 * @return EXIT_SUCCESS en cas de succès, EXIT_FAILURE en cas d'erreur.
 */
int on_message(Codenames* codenames, TcpClient* client, char* message);

/**
 * Traite la déconnexion d'un client.
 * @param codenames Contexte principal du serveur.
 * @param client Client TCP qui s'est déconnecté.
 * @return EXIT_SUCCESS en cas de succès, EXIT_FAILURE en cas d'erreur.
 */
int on_leave(Codenames* codenames, TcpClient* client);

#endif // MESSAGE_H
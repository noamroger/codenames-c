/**
 * @file list.h
 * @brief Implémentation d'une liste chaînée générique.
 */

#ifndef LIST_H
#define LIST_H

#include <stdlib.h>

/**
 * Noeud d'une liste chaînée générique.
 * @param data pointeur vers la donnée stockée (void*).
 * @param next pointeur vers le noeud suivant, ou NULL si dernier.
 */
typedef struct ListNode {
    void* data;
    struct ListNode* next;
} ListNode;

/**
 * Liste chaînée générique avec identifiant unique.
 * @param id identifiant unique de la liste (attribué automatiquement).
 * @param head pointeur vers le premier noeud, ou NULL si vide.
 * @param tail pointeur vers le dernier noeud, pour un ajout en O(1).
 * @param size nombre d'éléments dans la liste.
 */
typedef struct List {
    int id;
    ListNode* head;
    ListNode* tail;
    int size;
} List;

/**
 * Crée une nouvelle liste chaînée vide.
 * Un identifiant unique est attribué automatiquement.
 * @return Pointeur vers la liste créée, ou NULL en cas d'erreur.
 */
List* list_create();

/**
 * Détruit une liste chaînée et libère toutes ses ressources.
 * @param list Liste à détruire.
 * @param free_data Fonction de libération appelée sur chaque donnée,
 *                  ou NULL si les données ne doivent pas être libérées.
 */
void list_destroy(List* list, void (*free_data)(void*));

/**
 * Ajoute un élément en fin de liste.
 * @param list Liste cible.
 * @param data Donnée à ajouter.
 * @return EXIT_SUCCESS en cas de succès, EXIT_FAILURE en cas d'erreur.
 */
int list_add(List* list, void* data);

/**
 * Retire un élément de la liste (comparaison par pointeur).
 * Le noeud est libéré mais la donnée n'est PAS libérée.
 * @param list Liste cible.
 * @param data Pointeur vers la donnée à retirer.
 * @return EXIT_SUCCESS si l'élément a été trouvé et retiré, EXIT_FAILURE sinon.
 */
int list_remove(List* list, void* data);

/**
 * Récupère l'élément à l'index donné.
 * @param list Liste cible.
 * @param index Index (0-based) de l'élément.
 * @return Pointeur vers la donnée, ou NULL si l'index est invalide.
 */
void* list_get(List* list, int index);

/**
 * Recherche un élément dans la liste à l'aide d'un prédicat.
 * @param list Liste cible.
 * @param predicate Fonction retournant 1 si l'élément correspond.
 *                  Reçoit (donnée_du_noeud, contexte).
 * @param context Contexte passé au prédicat (peut être NULL).
 * @return Pointeur vers la première donnée correspondante, ou NULL.
 */
void* list_find(List* list, int (*predicate)(void*, void*), void* context);

/**
 * Retourne la taille de la liste.
 * @param list Liste cible.
 * @return Nombre d'éléments, ou 0 si list est NULL.
 */
int list_size(List* list);

/**
 * Applique une fonction callback sur chaque élément de la liste.
 * @param list Liste cible.
 * @param callback Fonction appelée pour chaque élément.
 *                 Reçoit (donnée_du_noeud, contexte).
 * @param context Contexte passé au callback (peut être NULL).
 */
void list_foreach(List* list, void (*callback)(void*, void*), void* context);

/**
 * Trouve le plus petit identifiant entier >= 0 non utilisé dans la liste.
 * Utilise un extracteur d'ID pour obtenir l'identifiant de chaque élément.
 * @param list Liste cible.
 * @param get_id Fonction qui retourne l'identifiant d'un élément.
 * @return Le plus petit identifiant disponible (>= 0).
 */
int list_next_available_id(List* list, int (*get_id)(void*));

#endif // LIST_H

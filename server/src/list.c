#include "../lib/list.h"
#include <stdio.h>
#include <string.h>

/** Compteur global pour attribuer un identifiant unique à chaque liste. */
static int list_id_counter = 0;

List* list_create() {
    List* list = malloc(sizeof(List));
    if (!list) {
        perror("list_create: malloc failed");
        return NULL;
    }
    list->id = list_id_counter++;
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    return list;
}

void list_destroy(List* list, void (*free_data)(void*)) {
    if (!list) return;
    ListNode* current = list->head;
    while (current) {
        ListNode* next = current->next;
        if (free_data && current->data) {
            free_data(current->data);
        }
        free(current);
        current = next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    free(list);
}

int list_add(List* list, void* data) {
    if (!list) return EXIT_FAILURE;

    ListNode* node = malloc(sizeof(ListNode));
    if (!node) {
        perror("list_add: malloc failed");
        return EXIT_FAILURE;
    }
    node->data = data;
    node->next = NULL;

    /* Le pointeur de queue évite de reparcourir toute la liste à chaque ajout :
       l'historique de chat (100 messages par lobby) passe de O(n^2) à O(n). */
    if (!list->head) {
        list->head = node;
    } else {
        list->tail->next = node;
    }
    list->tail = node;
    list->size++;
    return EXIT_SUCCESS;
}

int list_remove(List* list, void* data) {
    if (!list || !list->head) return EXIT_FAILURE;

    ListNode* current = list->head;
    ListNode* prev = NULL;

    while (current) {
        if (current->data == data) {
            if (prev) {
                prev->next = current->next;
            } else {
                list->head = current->next;
            }
            if (list->tail == current) list->tail = prev;
            free(current);
            list->size--;
            return EXIT_SUCCESS;
        }
        prev = current;
        current = current->next;
    }
    return EXIT_FAILURE;
}

void* list_get(List* list, int index) {
    if (!list || index < 0 || index >= list->size) return NULL;

    ListNode* current = list->head;
    for (int i = 0; i < index; i++) {
        current = current->next;
    }
    return current->data;
}

void* list_find(List* list, int (*predicate)(void*, void*), void* context) {
    if (!list || !predicate) return NULL;

    ListNode* current = list->head;
    while (current) {
        if (predicate(current->data, context)) {
            return current->data;
        }
        current = current->next;
    }
    return NULL;
}

int list_size(List* list) {
    if (!list) return 0;
    return list->size;
}

void list_foreach(List* list, void (*callback)(void*, void*), void* context) {
    if (!list || !callback) return;

    ListNode* current = list->head;
    while (current) {
        ListNode* next = current->next; // sauvegarde au cas où le callback modifie la liste
        callback(current->data, context);
        current = next;
    }
}

int list_next_available_id(List* list, int (*get_id)(void*)) {
    if (!list || !get_id || list->size == 0) return 0;

    /* Construire un tableau de booléens pour marquer les IDs utilisés */
    int max_id = -1;
    ListNode* current = list->head;
    while (current) {
        int id = get_id(current->data);
        if (id > max_id) max_id = id;
        current = current->next;
    }

    /* Allouer un tableau de taille max_id + 2 pour couvrir tous les IDs possibles */
    int range = max_id + 2;
    int* used = calloc(range, sizeof(int));
    if (!used) return list->size; /* fallback */

    current = list->head;
    while (current) {
        int id = get_id(current->data);
        if (id >= 0 && id < range) {
            used[id] = 1;
        }
        current = current->next;
    }

    /* Trouver le premier ID libre */
    int result = 0;
    for (int i = 0; i < range; i++) {
        if (!used[i]) {
            result = i;
            break;
        }
    }

    free(used);
    return result;
}

#include "../lib/all.h"

static int read_line_alloc(FILE* f, char** line, size_t* cap) {
    if (!f || !line || !cap) return -1;

    if (*line == NULL || *cap == 0) {
        *cap = 128;
        *line = malloc(*cap);
        if (!*line) return -1;
    }

    size_t len = 0;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (len + 1 >= *cap) {
            size_t new_cap = (*cap) * 2;
            char* tmp = realloc(*line, new_cap);
            if (!tmp) return -1;
            *line = tmp;
            *cap = new_cap;
        }
        (*line)[len++] = (char)c;
        if (c == '\n') break;
    }

    if (len == 0 && c == EOF) return -1;

    (*line)[len] = '\0';
    return (int)len;
}

int number_length(int n) {
    if (n == 0) return 1;
    int length = 0;
    if (n < 0) {
        length++; // signe negatif a ajouter
        n = -n; // rendre n positif pour le calcul de la longueur des chiffres
    }
    while (n > 0) {
        n /= 10;
        length++;
    }
    return length;
}

int format_to(char *buf, size_t size, const char *fmt, ...) {
    if (!buf || size == 0 || !fmt) return -1;
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, size, fmt, args);
    va_end(args);
    if (ret < 0) return -1;
    return ret;
}

#ifndef _WIN32
char* itoa(int value) {
    static char buffer[20];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return buffer;
}
#endif


char* getRandomUsername(void) {
    FILE* f = fopen("assets/misc/usernames.txt", "r");
    if (!f) return NULL;
    char* line = NULL;
    size_t len = 0;
    int read;
    char** names = NULL;
    size_t count = 0;
    while ((read = read_line_alloc(f, &line, &len)) != -1) {
        while (read > 0 && (line[read - 1] == '\n' || line[read - 1] == '\r')) {
            line[--read] = '\0';
        }
        char* s = strdup(line);
        if (!s) {
            free(line);
            for (size_t i = 0; i < count; i++) free(names[i]);
            free(names);
            fclose(f);
            return NULL;
        }
        char** tmp = realloc(names, sizeof(char*) * (count + 1));
        if (!tmp) {
            free(s);
            free(line);
            for (size_t i = 0; i < count; i++) free(names[i]);
            free(names);
            fclose(f);
            return NULL;
        }
        names = tmp;
        names[count++] = s;
    }
    free(line);
    fclose(f);
    if (count == 0) {
        free(names);
        return NULL;
    }
    int idx = rand() % count;
    char* result = strdup(names[idx]);
    for (size_t i = 0; i < count; i++) free(names[i]);
    free(names);
    return result;
}

int levenshtein(const char* s1, const char* s2) {
    if (!s1 || !s2) return -1;
    
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    
    // Cas triviaux
    if (len1 == 0) return (int)len2;
    if (len2 == 0) return (int)len1;
    
    // Allouer la matrice de distances
    int** matrix = malloc((len1 + 1) * sizeof(int*));
    if (!matrix) return -1;
    
    for (size_t i = 0; i <= len1; i++) {
        matrix[i] = malloc((len2 + 1) * sizeof(int));
        if (!matrix[i]) {
            for (size_t j = 0; j < i; j++) free(matrix[j]);
            free(matrix);
            return -1;
        }
    }
    
    // Initialiser la première colonne et ligne
    for (size_t i = 0; i <= len1; i++) matrix[i][0] = (int)i;
    for (size_t j = 0; j <= len2; j++) matrix[0][j] = (int)j;
    
    // Remplir la matrice
    for (size_t i = 1; i <= len1; i++) {
        for (size_t j = 1; j <= len2; j++) {
            // Comparaison insensible à la casse
            int cost = (tolower((unsigned char)s1[i-1]) == tolower((unsigned char)s2[j-1])) ? 0 : 1;
            
            int del = matrix[i-1][j] + 1;       // Suppression
            int ins = matrix[i][j-1] + 1;       // Insertion
            int sub = matrix[i-1][j-1] + cost;  // Substitution
            
            // Minimum des trois opérations
            int min = del;
            if (ins < min) min = ins;
            if (sub < min) min = sub;
            
            matrix[i][j] = min;
        }
    }
    
    int result = matrix[len1][len2];
    
    // Libérer la mémoire
    for (size_t i = 0; i <= len1; i++) free(matrix[i]);
    free(matrix);
    
    return result;
}

int word_contains(const char* input, const char* card_word) {
    if (!input || !card_word) return 0;
    
    size_t len1 = strlen(input);
    size_t len2 = strlen(card_word);
    
    // Créer des copies en minuscules
    char* lower1 = malloc(len1 + 1);
    char* lower2 = malloc(len2 + 1);
    if (!lower1 || !lower2) {
        free(lower1);
        free(lower2);
        return 0;
    }
    
    for (size_t i = 0; i <= len1; i++) lower1[i] = tolower((unsigned char)input[i]);
    for (size_t i = 0; i <= len2; i++) lower2[i] = tolower((unsigned char)card_word[i]);
    
    int result = (strstr(lower1, lower2) != NULL) || (strstr(lower2, lower1) != NULL);
    
    free(lower1);
    free(lower2);
    
    return result;
}

int valid_hint(const char* hint, Card card_words[NB_WORDS]) {
    if (!hint) return 0;
    int hint_len = (int)strlen(hint);
    if (hint_len <= 3) return 1; // On autorise les indices courts, même s'ils sont proches, pour permettre des indices comme "3" ou "2-1"

    int threshold = (hint_len == 4) ? 2 : 3;

    for (int i = 0; i < NB_WORDS; i++) {
        /* word est un tableau : tester le pointeur était toujours vrai et ne
           filtrait donc jamais les cartes vides. */
        if (card_words[i].word[0] == '\0' || card_words[i].revealed) continue;
        int distance = levenshtein(hint, card_words[i].word);
        int contain = word_contains(hint, card_words[i].word);
        if (distance >= 0 && (distance < threshold || contain)) {
            printf("Hint '%s' is too close to card word '%s' (distance=%d, contain=%d)\n", hint, card_words[i].word, distance, contain);
            return 0; // Hint trop proche ou contient un mot d'une carte
        }
    }

    return 1; // Hint valide
}
#include "../lib/all.h"

FILE* open_properties(const char* mode) {
    FILE* f = fopen("data/player.properties", mode);
    if (!f) {
        perror("Failed to open properties file");
        return NULL;
    }
    return f;
}

int read_property(char* buf, size_t buf_size, const char* property) {
    if (!buf || buf_size == 0) return EXIT_FAILURE;
    buf[0] = '\0'; // l'appelant doit pouvoir tester le résultat même en cas d'échec

    FILE* f = open_properties("r");
    if (!f) return EXIT_FAILURE;

    rewind(f);

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key_buf[128] = {0};
        char val_buf[256] = {0};
        if (sscanf(line, " %127[^=]= %255[^\n]", key_buf, val_buf) == 2) {
            // Trim trailing spaces from key
            size_t klen = strlen(key_buf);
            while (klen > 0 && key_buf[klen - 1] == ' ') key_buf[--klen] = '\0';
            // Trim leading spaces from value
            char* vp = val_buf;
            while (*vp == ' ') vp++;

            if (strcmp(key_buf, property) == 0) {
                /* Copie bornée : la valeur du fichier peut faire 255 octets
                   alors que les appelants passent des tampons de 16. */
                snprintf(buf, buf_size, "%s", vp);
                fclose(f);
                return EXIT_SUCCESS;
            }
        }
    }
    fclose(f);
    return EXIT_FAILURE;
}

int write_property(const char* property, const char* value) {
    // Read all existing properties into memory
    typedef struct { char key[128]; char val[256]; } Entry;
    Entry entries[64];
    int count = 0;
    int found = 0;

    FILE* f = open_properties("r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f) && count < 64) {
            char key_buf[128] = {0};
            char val_buf[256] = {0};
            if (sscanf(line, " %127[^=]= %255[^\n]", key_buf, val_buf) == 2) {
                // Trim trailing spaces from key
                size_t klen = strlen(key_buf);
                while (klen > 0 && key_buf[klen - 1] == ' ') key_buf[--klen] = '\0';
                // Trim leading spaces from value
                char* vp = val_buf;
                while (*vp == ' ') vp++;

                if (strcmp(key_buf, property) == 0) {
                    snprintf(entries[count].key, sizeof(entries[count].key), "%s", key_buf);
                    snprintf(entries[count].val, sizeof(entries[count].val), "%s", value);
                    found = 1;
                } else {
                    snprintf(entries[count].key, sizeof(entries[count].key), "%s", key_buf);
                    snprintf(entries[count].val, sizeof(entries[count].val), "%s", vp);
                }
                count++;
            }
        }
        fclose(f);
    }

    // If property was not found, add it
    if (!found && count < 64) {
        snprintf(entries[count].key, sizeof(entries[count].key), "%s", property);
        snprintf(entries[count].val, sizeof(entries[count].val), "%s", value);
        count++;
    }

    // Rewrite the file with all properties
    f = open_properties("w");
    if (!f) return EXIT_FAILURE;

    for (int i = 0; i < count; i++) {
        fprintf(f, "%s = %s\n", entries[i].key, entries[i].val);
    }

    fflush(f);
    fclose(f);
    return EXIT_SUCCESS;
}
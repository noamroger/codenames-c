#include "../lib/all.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#ifdef _WIN32
#include <bcrypt.h>
#else
#include <sys/random.h>
#endif

/**
 * Remplit un tampon avec des octets cryptographiquement aléatoires.
 * Les codes de lobby et les UUID servent d'identifiants : rand(), dont la
 * graine time(NULL) est devinable, les rendrait énumérables.
 * @param out Tampon de sortie.
 * @param len Nombre d'octets à générer.
 * @return EXIT_SUCCESS si le tampon est rempli, EXIT_FAILURE sinon.
 */
static int secure_random_bytes(unsigned char* out, size_t len) {
    if (!out || len == 0) return EXIT_FAILURE;

#ifdef _WIN32
    if (BCryptGenRandom(NULL, out, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0) {
        return EXIT_SUCCESS;
    }
#else
    size_t filled = 0;
    while (filled < len) {
        ssize_t got = getrandom(out + filled, len - filled, 0);
        if (got <= 0) {
            if (errno == EINTR) continue;
            break;
        }
        filled += (size_t)got;
    }
    if (filled == len) return EXIT_SUCCESS;

    /* Repli portable si getrandom n'est pas disponible sur la plateforme. */
    FILE* urandom = fopen("/dev/urandom", "rb");
    if (urandom) {
        size_t read_bytes = fread(out, 1, len, urandom);
        fclose(urandom);
        if (read_bytes == len) return EXIT_SUCCESS;
    }
#endif

    return EXIT_FAILURE;
}

int randint(int min, int max) {
    // Retourne un entier aléatoire entre min et max inclus
    // Formule : min + rand() / (RAND_MAX / (max - min + 1) + 1)
    if (max <= min) return min; // évite une division par zéro si la plage est vide
    int range = max - min + 1;
    return min + rand() / (RAND_MAX / range + 1);
}

int count_words(const char *filepath) {
    if (!filepath) return -1;
    FILE *f = fopen(filepath, "r");
    if (!f) return -1;

    char buf[4096];
    int count = 0;

    while (fgets(buf, sizeof(buf), f) != NULL) {
        /* vérifier s'il y a au moins un caractère non blanc dans la ligne */
        int has_non_ws = 0;
        for (size_t i = 0; buf[i] != '\0'; ++i) {
            if (!isspace((unsigned char)buf[i])) {
                has_non_ws = 1;
                break;
            }
        }
        if (has_non_ws) count++;

        /* consommer la suite d'une ligne trop longue pour le tampon */
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] != '\n') {
            int c;
            do {
                c = fgetc(f);
            } while (c != '\n' && c != EOF);
        }
    }

    fclose(f);
    return count;
}

int starts_with(const char *str, const char *prefix) {
    if (!str || !prefix) return 0;
    size_t len_prefix = strlen(prefix);
    return strncmp(str, prefix, len_prefix) == 0;
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

char* generate_code() {
    /* Alphabet sans caractères ambigus (I, L, O) pour rester dictable à l'oral.
       Exactement 32 symboles : le modulo sur un octet est donc sans biais.
       32^6 ≈ 1,07 milliard de codes, contre 100 000 auparavant. */
    static const char alphabet[] = "0ABCDEFGHJKMNPQRSTUVWXYZ23456789";
    const size_t alphabet_size = sizeof(alphabet) - 1;

    unsigned char raw[LOBBY_CODE_LEN];
    if (secure_random_bytes(raw, sizeof(raw)) != EXIT_SUCCESS) {
        fprintf(stderr, "generate_code: no secure random source available\n");
        return NULL;
    }

    char buffer[LOBBY_CODE_LEN + 1];
    for (size_t i = 0; i < LOBBY_CODE_LEN; i++) {
        buffer[i] = alphabet[raw[i] % alphabet_size];
    }
    buffer[LOBBY_CODE_LEN] = '\0';

    return strdup(buffer);
}

/**
 * Génère une chaîne UUID v4-like aléatoire (format 8-4-4-4-12).
 */
static int random_uuid_string(char* out, size_t size) {
    const char hex[] = "0123456789abcdef";
    // Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx  (36 chars + '\0')
    if (size < 37) return EXIT_FAILURE;

    /* 16 octets issus du CSPRNG, mis en forme en UUID v4 (RFC 4122). */
    unsigned char raw[16];
    if (secure_random_bytes(raw, sizeof(raw)) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    raw[6] = (unsigned char)((raw[6] & 0x0F) | 0x40); // version 4
    raw[8] = (unsigned char)((raw[8] & 0x3F) | 0x80); // variante RFC 4122

    int pos = 0;
    int byte_index = 0;
    int lengths[] = {8, 4, 4, 4, 12};
    for (int g = 0; g < 5; g++) {
        if (g > 0) out[pos++] = '-';
        for (int i = 0; i < lengths[g]; i += 2) {
            unsigned char b = raw[byte_index++];
            out[pos++] = hex[(b >> 4) & 0x0F];
            out[pos++] = hex[b & 0x0F];
        }
    }
    out[pos] = '\0';
    return EXIT_SUCCESS;
}

/**
 * Vérifie si un UUID existe déjà dans le fichier.
 */
static int uuid_exists_in_file(const char* filepath, const char* uuid) {
    FILE* f = fopen(filepath, "r");
    if (!f) return 0; // fichier n'existe pas => pas de doublon
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        // Supprimer le '\n' en fin de ligne
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (strcmp(line, uuid) == 0) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

char* generate_uuid(const char* uuids_path) {
    if (!uuids_path) return NULL;

    // S'assurer que le répertoire parent (data) existe
    {
        struct stat st = {0};
        if (stat("data", &st) == -1) {
            if (MKDIR_DATA("data") != 0) {
                perror("Warning: could not create 'data' directory");
            }
        }
    }

    // Générer un UUID unique
    char uuid[37];
    int max_attempts = 1000;
    int found = 0;
    for (int i = 0; i < max_attempts; i++) {
        if (random_uuid_string(uuid, sizeof(uuid)) != EXIT_SUCCESS) {
            fprintf(stderr, "generate_uuid: no secure random source available\n");
            return NULL;
        }
        if (!uuid_exists_in_file(uuids_path, uuid)) {
            found = 1;
            break;
        }
    }
    if (!found) {
        fprintf(stderr, "Failed to generate a unique UUID after %d attempts\n", max_attempts);
        return NULL;
    }

    // Ajouter l'UUID au fichier (créer le fichier s'il n'existe pas)
    FILE* f = fopen(uuids_path, "a");
    if (!f) {
        perror("Failed to open uuids file for writing");
        return NULL;
    }
    fprintf(f, "%s\n", uuid);
    fclose(f);

    return strdup(uuid);
}
#include "../lib/all.h"

void load_version(Codenames* codenames) {

    char version[24] = {0};
    FILE* v = fopen("./VERSION", "r");
    if (v) {
        if (fgets(version, sizeof(version), v)) {
            /* Retirer le '\n' final */
            size_t len = strlen(version);
            if (len > 0 && version[len - 1] == '\n') version[len - 1] = '\0';
        } else {
            version[0] = '\0';
        }
        fclose(v);
    } else {
        /* Fallback : 0.0.0 si le fichier est absent */
        snprintf(version, sizeof(version), "%s", "0.0.0");
    }

    snprintf(codenames->version, sizeof(codenames->version), "V%s", version);
}

int on_version_compare(Codenames* codenames, TcpClient* client, char* message, Arguments args) {
    (void)message;

    if (args.argc < 1) {
        printf("Invalid version compare message from client %d\n", client->id);
        return EXIT_FAILURE;
    }

    const char* client_version = args.argv[0];
    printf("Comparing versions: client=%s server=%s\n", client_version, codenames->version);

    if (strcmp(client_version, codenames->version) != 0) {
        printf("Client %d has an outdated version\n", client->id);
        char response[64];
        format_to(response, sizeof(response), "%d Your version is outdated (%s instead of %s)", MSG_INFO, client_version, codenames->version);
        tcp_send_to_client(codenames, client->id, response);
        tcp_disconnect(codenames, client); // Deconnexion
    }

    return EXIT_SUCCESS;

}
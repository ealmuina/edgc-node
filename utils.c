#include "include/utils.h"

void print_log(char *msg) {
    time_t rawtime;
    time(&rawtime);
    char *time = strtok(ctime(&rawtime), "\n");
    printf("[%s] %s\n", time, msg);
}

void add_buffer_to_json(char *buffer, char *json, char *key) {
    char entry[BUFFER_SIZE];
    int len = strlen(json);
    if (len != 1)
        json[len - 1] = ',';
    sprintf(entry, "\"%s\":\"%s\"}", key, buffer);
    strcat(json, entry);
}

void add_file_to_json(char *path, char *json, char *key) {
    char *buffer = malloc(BUFFER_SIZE);

    // Read file content into buffer
    FILE *file = fopen(path, "r");

    if (!file) return;

    int n = fread(buffer, sizeof(char), BUFFER_SIZE, file);
    fclose(file);
    buffer[n - 1] = 0;

    // Replace \n and \t
    for (int i = 0; i < n; ++i) {
        switch (buffer[i]) {
            case '\n': {
                buffer[i] = '|';
                break;
            }
            case '\t': {
                buffer[i] = ' ';
                break;
            }
            default:
                break;
        }
    }

    add_buffer_to_json(buffer, json, key);
    free(buffer);
}

double randn(double mu, double sigma) {
    double U1, U2, W, mult;
    static double X1, X2;
    static int call = 0;

    if (call == 1) {
        call = !call;
        return (mu + sigma * (double) X2);
    }

    do {
        U1 = -1 + ((double) rand() / RAND_MAX) * 2;
        U2 = -1 + ((double) rand() / RAND_MAX) * 2;
        W = pow(U1, 2) + pow(U2, 2);
    } while (W >= 1 || W == 0);

    mult = sqrt((-2 * log(W)) / W);
    X1 = U1 * mult;
    X2 = U2 * mult;

    call = !call;

    return (mu + sigma * (double) X1);
}

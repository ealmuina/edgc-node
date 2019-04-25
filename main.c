#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>

#define PORT 9910
#define BUFFER_SIZE (256 * 1024)  /* 256 KB */

void print_log(char *msg) {
    time_t rawtime;
    time(&rawtime);
    char *time = strtok(ctime(&rawtime), "\n");
    printf("[%s] %s\n", time, msg);
}

void add_file_to_json(char *path, char *json, char *key) {
    char buffer[BUFFER_SIZE], entry[BUFFER_SIZE];

    // Read file content into buffer
    FILE *file = fopen(path, "r");
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

    // Add buffer content to the json object
    int len = strlen(json);
    if (len != 1)
        json[len - 1] = ',';
    sprintf(entry, "\"%s\":\"%s\"}", key, buffer);
    strcat(json, entry);
}

// Driver code 
int main() {
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in servaddr;

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    int broadcastEnable = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
        // Move the last 5 minutes load to buffer
        double loadavg[3];
        getloadavg(loadavg, 3);
        memcpy(buffer, loadavg + 1, sizeof(double));

        // Move the number of processors to buffer
        int processors = get_nprocs();
        memcpy(buffer + sizeof(double), &processors, sizeof(int));

        int offset = sizeof(double) + sizeof(int);
        char *stats = buffer + offset;

        // Read stats in buffer
        sprintf(stats, "{");
        add_file_to_json("/proc/cpuinfo", stats, "cpuinfo");
        add_file_to_json("/proc/meminfo", stats, "meminfo");

        int total_bytes = offset + strlen(stats);
        sendto(sockfd, buffer, total_bytes, 0, (struct sockaddr *) &servaddr, sizeof(servaddr));
        print_log("Statistics broadcast");
        fflush(stdout);
        sleep(1); // broadcast every 5 seconds
    }
#pragma clang diagnostic pop
} 

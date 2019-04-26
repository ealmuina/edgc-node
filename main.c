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
#define REPORT_INTERVAL 5 /* seconds */

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

#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err34-c"

void get_cpu_usage(float *stat) {
    char str[100];
    const char d[2] = " ";
    char *token;
    int i = 0;
    long int sum = 0, idle = 0;

    FILE *fp = fopen("/proc/stat", "r");
    i = 0;
    fgets(str, 100, fp);
    fclose(fp);
    token = strtok(str, d);

    while (token != NULL) {
        token = strtok(NULL, d);
        if (token != NULL) {
            sum += atoi(token);

            if (i == 3)
                idle = atoi(token);

            i++;
        }
    }

    stat[0] = idle;
    stat[1] = sum - idle;
}

#pragma clang diagnostic pop

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

    float cpu_use[2], last_cpu_use[2];
    get_cpu_usage(last_cpu_use);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
        sleep(REPORT_INTERVAL); // broadcast every REPORT_INTERVAL seconds

        // Move the load from the last monitoring interval to buffer
        get_cpu_usage(cpu_use);
        float idle = cpu_use[0] - last_cpu_use[0];
        float running = cpu_use[1] - last_cpu_use[1];
        float running_rate = running / (running + idle);
        memcpy(last_cpu_use, cpu_use, 2 * sizeof(float));
        memcpy(buffer, &running_rate, sizeof(float));

        // Move the number of processors to buffer
        int processors = get_nprocs();
        memcpy(buffer + sizeof(float), &processors, sizeof(int));

        int offset = sizeof(float) + sizeof(int);
        char *stats = buffer + offset;

        // Read stats in buffer
        sprintf(stats, "{");
        add_file_to_json("/proc/cpuinfo", stats, "cpuinfo");
        add_file_to_json("/proc/meminfo", stats, "meminfo");

        int total_bytes = offset + strlen(stats);
        sendto(sockfd, buffer, total_bytes, 0, (struct sockaddr *) &servaddr, sizeof(servaddr));
        sprintf(buffer, "Statistics broadcast. CPU usage in the last %d seconds: %.2f", REPORT_INTERVAL, running_rate);
        print_log(buffer);
        fflush(stdout);
    }
#pragma clang diagnostic pop
} 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
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

// Driver code 
int main() {
    int sockfd;
    char buffer1[BUFFER_SIZE], buffer2[BUFFER_SIZE];
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
        // Read stats in buffer
        FILE *file = fopen("/proc/loadavg", "r");
        int n = fread(buffer1, sizeof(char), BUFFER_SIZE, file);
        buffer1[n - 1] = 0;
        fclose(file);
        sprintf(buffer2, "{\"loadavg\":\"%s\"}", buffer1);

        sendto(sockfd, buffer2, strlen(buffer2), 0, (struct sockaddr *) &servaddr, sizeof(servaddr));
        print_log("Statistics broadcast");
        fflush(stdout);
        sleep(1); // broadcast every 5 seconds
    }
#pragma clang diagnostic pop

    close(sockfd);
    return 0;
} 

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
#include <ifaddrs.h>

#define PORT 9910
#define BUFFER_SIZE (256 * 1024)  /* 256 KB */
#define FIELD_SIZE 1024
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

void get_network_device(char *network_dev) {
    struct ifaddrs *ifaddr;
    struct ifaddrs *ifa;
    getifaddrs(&ifaddr);

    network_dev[0] = 0;

    // look which interface contains the wanted IP.
    // When found, ifa->ifa_name contains the name of the interface (eth0, eth1, ppp0...)
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr) {
            if (AF_INET == ifa->ifa_addr->sa_family) {
                if (ifa->ifa_name && strncmp("eth", ifa->ifa_name, 3) == 0) {
                    strcpy(network_dev, ifa->ifa_name);
                    break;
                }
            }
        }
    }
    freeifaddrs(ifaddr);
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
    servaddr.sin_addr.s_addr = INADDR_BROADCAST;

    // Read the hostname
    char hostname[FIELD_SIZE];
    FILE *fd = fopen("/etc/hostname", "r");
    fread(hostname, sizeof(char), FIELD_SIZE, fd);
    fclose(fd);

    // Read number of processors
    int processors = get_nprocs();

    // Get name of network device
    char network_dev[FIELD_SIZE];
    get_network_device(network_dev);

    float cpu_use[2], last_cpu_use[2];
    get_cpu_usage(last_cpu_use);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
        sleep(REPORT_INTERVAL); // broadcast every REPORT_INTERVAL seconds

        // Add hostname to buffer
        int len = strlen(hostname);
        strcpy(buffer, hostname);
        buffer[len - 1] = 0; // Remove the \n at the end

        int offset = len;
        char *stats = buffer + offset;

        // Move the load from the last monitoring interval to buffer
        get_cpu_usage(cpu_use);
        float idle = cpu_use[0] - last_cpu_use[0];
        float running = cpu_use[1] - last_cpu_use[1];
        float running_rate = running / (running + idle);
        memcpy(last_cpu_use, cpu_use, 2 * sizeof(float));
        memcpy(stats, &running_rate, sizeof(float));
        offset += sizeof(float);

        // Move the number of processors to buffer
        memcpy(buffer + offset, &processors, sizeof(int));
        offset += sizeof(int);
        stats = buffer + offset;

        // Read stats in buffer
        sprintf(stats, "{");
        add_file_to_json("/proc/cpuinfo", stats, "cpuinfo");
        add_file_to_json("/proc/meminfo", stats, "meminfo");
        if (*network_dev) {
            char speed_file[FIELD_SIZE];
            sprintf(speed_file, "/sys/class/net/%s/speed", network_dev);
            add_file_to_json(speed_file, stats, "speed");
        }

        int total_bytes = offset + strlen(stats);
        sendto(sockfd, buffer, total_bytes, 0, (struct sockaddr *) &servaddr, sizeof(servaddr));
        sprintf(buffer, "Statistics broadcast. CPU usage in the last %d seconds: %.2f", REPORT_INTERVAL, running_rate);
        print_log(buffer);
        fflush(stdout);
    }
#pragma clang diagnostic pop
} 

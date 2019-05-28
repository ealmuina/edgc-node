#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <ifaddrs.h>
#include <netdb.h>
#include <pthread.h>

#include "include/flops.h"
#include "include/utils.h"

#define LITE_PORT 9910
#define FULL_PORT 9913
#define REPORT_INTERVAL_MEAN 20 /* seconds */
#define REPORT_INTERVAL_STD 4 /* seconds */

int node_info_size = 0;
char node_info[BUFFER_SIZE];

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

            if (i >= 3)
                idle += atoi(token);

            i++;
        }
    }

    stat[0] = idle;
    stat[1] = sum - idle;
}

double measure_bandwidth(char *server, char *localhost) {
    char buffer[FIELD_SIZE];

    sprintf(buffer, "Measuring bandwidth to domain master '%s'...", server);
    print_log(buffer);

    sprintf(buffer, "mpirun -n 2 -hosts %s,%s ./mpi_bandwidth", localhost, server);
    printf("\t-> %s\n", buffer);
    system(buffer);
}

void build_node_info(char *domain_master, char *hostname) {
    char buffer[FIELD_SIZE];

    // Measure MPI bandwidth
    char bandwidth_file[FIELD_SIZE];
    measure_bandwidth(domain_master, hostname);
    sprintf(bandwidth_file, "bandwidth-%s.txt", hostname);
    sprintf(buffer, "MPI bandwidth stored to file '%s'.", bandwidth_file);
    print_log(buffer);

    // Benchmark CPU
    print_log("Measuring CPU performance...");
    char mflops[FIELD_SIZE];
    sprintf(mflops, "%.4f", measure_mflops());
    print_log("Benchmark completed 'MFLOPS(3)' will be used.");

    // Move the number of processors to node_info
    int processors = get_nprocs();
    memcpy(node_info, &processors, sizeof(int));
    node_info_size = sizeof(int);
    char *stats = node_info + node_info_size;

    // Read stats into node_info
    sprintf(stats, "{");
    add_file_to_json("/proc/cpuinfo", stats, "cpuinfo");
    add_file_to_json("/proc/meminfo", stats, "meminfo");
    add_file_to_json(bandwidth_file, stats, "mpi_bandwidth");
    add_buffer_to_json(mflops, stats, "mflops");

    node_info_size += strlen(stats);
}

void *full_reporter(void *args) {
    int sockfd, connfd;
    socklen_t len;
    struct sockaddr_in servaddr, cli;

    // socket create
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(FULL_PORT);

    bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    listen(sockfd, 5);

    print_log("Full report thread initialized.");

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
        // Send full information every time it is requested
        len = sizeof(cli);
        connfd = accept(sockfd, (struct sockaddr *) &cli, &len);
        write(connfd, node_info, node_info_size);
        print_log("Requested full information of the node.");
    }
#pragma clang diagnostic pop
}

// Driver code 
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: edgc-node <domain master hostname>\n");
        exit(EXIT_FAILURE);
    }

    int sockfd, len;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in servaddr;
    struct hostent *he;

    // Getting domain master address
    if ((he = gethostbyname(argv[1])) == NULL) {  /* get the host info */
        perror("gethostbyname");
        exit(EXIT_FAILURE);
    }

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(LITE_PORT);
    servaddr.sin_addr = *((struct in_addr *) he->h_addr);

    // Read the hostname
    char hostname[FIELD_SIZE];
    FILE *fd = fopen("/etc/hostname", "r");
    len = fread(hostname, sizeof(char), FIELD_SIZE, fd);
    hostname[len - 1] = 0; // Remove the \n at the end
    fclose(fd);

    float cpu_use[2], last_cpu_use[2];
    get_cpu_usage(last_cpu_use);

    // Open file to store load history
    char load_filename[FIELD_SIZE];
    sprintf(load_filename, "load-%s.log", hostname);
    FILE *load_history = fopen(load_filename, "w");
    time_t start; // stopwatch for reports
    time(&start);

    // Create separate thread for giving more details when requested
    build_node_info(argv[1], hostname);
    pthread_t m;
    pthread_create(&m, NULL, full_reporter, NULL);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (1) {
        // Broadcasts interval x~N(REPORT_INTERVAL_MEAN, REPORT_INTERVAL_STD**2)
        int interval = (int) randn(REPORT_INTERVAL_MEAN, REPORT_INTERVAL_STD);
        sleep(interval);

        // Add hostname to buffer
        len = strlen(hostname);
        strcpy(buffer, hostname);

        int offset = len + 1;
        char *stats = buffer + offset;

        // Move the load from the last monitoring interval to buffer
        get_cpu_usage(cpu_use);
        float idle = cpu_use[0] - last_cpu_use[0];
        float running = cpu_use[1] - last_cpu_use[1];
        float load = running / (running + idle);
        memcpy(last_cpu_use, cpu_use, 2 * sizeof(float));
        memcpy(stats, &load, sizeof(float));
        offset += sizeof(float);

        // Log load in load history file
        char entry[FIELD_SIZE];
        time_t now;
        time(&now);
        sprintf(entry, "%ld %.4f\n", now - start, load);
        fwrite(entry, sizeof(char), strlen(entry), load_history);

        sendto(sockfd, buffer, offset, 0, (struct sockaddr *) &servaddr, sizeof(servaddr));
        sprintf(buffer, "Sent to '%s' load in the last %d seconds: %.2f'.", argv[1], interval, load);
        print_log(buffer);

        fflush(load_history);
    }
#pragma clang diagnostic pop
} 

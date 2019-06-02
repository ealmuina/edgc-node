#ifndef EDGC_NODE_UTILS_H
#define EDGC_NODE_UTILS_H

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define BUFFER_SIZE (2 * 1024 * 1024)  /* 2 MB */
#define FIELD_SIZE 1024

void print_log(char *msg);

void add_buffer_to_json(char *buffer, char *json, char *key);

void add_file_to_json(char *path, char *json, char *key);

double randn(double mu, double sigma);

#endif //EDGC_NODE_UTILS_H

#include <iostream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <math.h>
#include "helpers.h"

#define MAX_CLIENTS 100
#define CLI_CMD_LEN 100
#define USER_ID_LEN 10

typedef struct tcp_msg {
    char ip[16];
    int port;
    char topic[51];
    char type[11];
    uint8_t type1;
    char payload[1501];
} tcp_msg;

typedef struct topic{
    char sf;
    char name[51];
    std::vector<tcp_msg> mess;
} topic;

typedef struct subscriber {
    std::string id;
    std::vector <topic> topics;
    int connect;
    int socket;
} subscriber;

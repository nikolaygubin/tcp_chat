#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>

#include <nlohmann/json.hpp>

#define handle_error(msg) \
do {                      \
   perror(msg);           \
   exit(EXIT_FAILURE);    \
} while (0)


enum COMMAND_TYPE {
    REGISTER = 0,
    LOG_IN,
    MESSAGE,
    ABORT,
    SUCCESS,
    DISCONNECT
};

#define COMMAND_KEY "Command"
#define PASSWORD_KEY "Password"
#define USERNAME_KEY "Username"
#define MESSAGE_KEY "Message"
#define REASON_KEY "Reason"
#define STATUS_KEY "Status"
#define CLIENTS_KEY "Clients"
#define IS_CONNECT "Connect"
#define SOCKET_FD  "Socket"

#define CLIENTS_PATH "./data/clients.json"

#define MSG_SIZE 256

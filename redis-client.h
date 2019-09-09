#include <stdlib.h>
#include <string.h>
#include <hiredis.h>
#include <cJSON.h>

typedef struct redis_conf {
    char *host;
    int port;
    char *auth;
} redis_conf;

typedef struct redis_server {
    char *host;
    int port;
} redis_server;

redis_conf *redis_client_init(const char *file);
void redis_client_exec_command(const redis_conf *conf, redis_server *redis, const char *command);
void redic_client_destory(redis_conf *conf);
#include "redis-client.h"

#define REDIS_CLUSTER_MOVED_STR "MOVED "
#define REDIS_CLUSTER_ASK_STR "ASK "

#define redis_client_free(it) if (it) {free(it);it = NULL;}

redis_conf *redis_client_init(const char *file) {
	FILE* f = fopen(file, "r");
	if (!f) {
		printf("Can not find redis.conf, init failed.");
		return NULL;
	}

	const size_t config_str_size = 5120;
	char* config_str = (char*)malloc(sizeof(char) * config_str_size);
	memset(config_str, '0', config_str_size);
	size_t len = fread(config_str, sizeof(char), config_str_size, f);
	fclose(f);
	config_str[len] = '\0';
	cJSON* json = cJSON_Parse(config_str);
	free(config_str);

	const char* error_ptr = cJSON_GetErrorPtr();
	if (error_ptr != NULL || !json) {
		cJSON_Delete(json);
		printf("mrcp.conf parse failed: [%s]", error_ptr);
		return NULL;
	}

	const cJSON* redis = cJSON_GetObjectItemCaseSensitive(json, "redis");
	const cJSON* redis_host = cJSON_GetObjectItemCaseSensitive(redis, "host");
	const cJSON* redis_port = cJSON_GetObjectItemCaseSensitive(redis, "port");
	const cJSON* redis_auth = cJSON_GetObjectItemCaseSensitive(redis, "auth");
	
	redis_conf *conf = NULL;
	if (cJSON_IsString(redis_host) && redis_host->valuestring != NULL && cJSON_IsNumber(redis_port) && redis_port->valueint > 0) {
		conf = malloc(sizeof(redis_conf));
		size_t len = strlen(redis_host->valuestring) + 1; // plus 1 for '\0'
		conf->host = malloc(len);
		memcpy(conf->host, redis_host->valuestring, len);
		conf->host[len] = '\0';
		conf->port = redis_port->valueint;
		printf("redis [%s:%d]", conf->host, conf->port);
		if (cJSON_IsString(redis_auth) && redis_auth->valuestring != NULL) {
			len = strlen(redis_auth->valuestring) + 1;
			if (len > 0) {
				conf->auth = malloc(len);
				memcpy(conf->auth, redis_auth->valuestring, len);
				conf->auth[len] = '\0';
			}
		}
	}

	cJSON_Delete(json);
	
	return conf;
}


void redis_client_exec_command(const redis_conf *conf, redis_server *redis, const char *command) {
	if (!redis) {
		redis = malloc(sizeof(redis_server));
		redis->host = conf->host;
		redis->port = conf->port;
	}
	
	printf("redis_client_exec_command to |%s| |%d| |%s|", redis->host, redis->port, command);

	redisContext *c = redisConnect(redis->host, redis->port);
	if (c == NULL || c->err) {
		if (c) {
			printf("Error: %s", c->errstr);
			redisFree(c);
		} else {
			printf("Can't allocate redis context");
		}
		return;
	}
	
	if (conf->auth) {
		//apt_log(SYNTH_LOG_MARK,APT_PRIO_INFO,"redis AUTH %s", channel->demo_engine->synth_conf.redis_auth);
		redisReply *auth_reply = (redisReply *)redisCommand(c, "AUTH %s", conf->auth);
		if (NULL != auth_reply) {
			if (REDIS_REPLY_ERROR == auth_reply->type)
				printf("Auth failed %s", auth_reply->str);
			freeReplyObject(auth_reply);
		}
	}
	
	redisReply *reply = (redisReply *)redisCommand(c, command);
	
	if (NULL != reply) {
		if (REDIS_REPLY_ERROR == reply->type) {
			printf("failed to get unit select result from redis, error message:%s", reply->str);
			// MOVED 394 10.250.140.89:7000 or MOVED 1147 10.250.140.89:7001 etc.
			char *redirect = NULL;
			if (strstr(reply->str, REDIS_CLUSTER_MOVED_STR) != NULL)
				redirect = REDIS_CLUSTER_MOVED_STR;
			else if (strstr(reply->str, REDIS_CLUSTER_ASK_STR) != NULL)
				redirect = REDIS_CLUSTER_ASK_STR;

			if (redirect) {
				char *space_pos = strchr(reply->str + strlen(redirect), ' ');
				if (space_pos != NULL) {
					size_t space_offset = space_pos - reply->str + 1;
					char *p = strchr(reply->str, ':');
					if (p != NULL) {
						const size_t colon_offset = p - reply->str + 1;
						unsigned int redis_host_len = colon_offset - space_offset - 1;
						redis_client_free(redis->host);
						redis->host = malloc(redis_host_len + 1);
						memcpy(redis->host, reply->str + space_offset, redis_host_len);
						redis->host[redis_host_len] = '\0';

						char new_port[5] = {0};
						unsigned int len = strlen(reply->str) - colon_offset;
						memcpy(new_port, reply->str + colon_offset, len);
						new_port[len] = '\0';
						redis->port = atoi(new_port);
						redis_client_exec_command(conf, redis, command);
					}
				}
			}
		}
		freeReplyObject(reply);
    }
	redisFree(c);
	
	if (redis)
		redis_client_free(redis);
}

void redic_client_destory(redis_conf *conf) {
	redis_client_free(conf->host);
	redis_client_free(conf->auth);
	redis_client_free(conf);
}

int main() {
	redis_conf *conf = redis_client_init("redis.conf");
	redis_client_exec_command(conf, NULL, "SET mykey islove");
	redic_client_destory(conf);
	return 0;
}

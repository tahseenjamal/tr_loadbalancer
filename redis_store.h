#ifndef REDIS_STORE_H
#define REDIS_STORE_H

int redis_get_tr_id(const char *key);
int redis_setnx_tr_id(const char *key, int tr_id, int ttl_sec);
int redis_set_tr_id(const char *key, int tr_id, int ttl_sec);

#endif

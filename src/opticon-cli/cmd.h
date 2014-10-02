#ifndef _OPTICONDB_CMD_H
#define _OPTICONDB_CMD_H 1

#include <time.h>

typedef struct optinfo {
    const char  *tenant;
    const char  *key;
    const char  *path;
    const char  *name;
    const char  *host;
    const char  *meter;
    const char  *type;
    const char  *description;
    const char  *unit;
    const char  *level;
    const char  *match;
    const char  *value;
    const char  *weight;
    time_t       time;
    int          json;
    const char  *api_url;
    const char  *keystone_url;
    const char  *keystone_token;
    const char  *opticon_token;
} optinfo;

extern optinfo OPTIONS;

int cmd_tenant_list (int argc, const char *argv[]);
int cmd_tenant_get_metadata (int argc, const char *argv[]);
int cmd_tenant_set_metadata (int argc, const char *argv[]);
int cmd_watcher_set (int argc, const char *argv[]);
int cmd_watcher_delete (int argc, const char *argv[]);
int cmd_watcher_list (int argc, const char *argv[]);
int cmd_meter_create (int argc, const char *argv[]);
int cmd_meter_delete (int argc, const char *argv[]);
int cmd_meter_list (int argc, const char *argv[]);
int cmd_tenant_delete (int argc, const char *argv[]);
int cmd_tenant_create (int argc, const char *argv[]);
int cmd_host_list (int argc, const char *argv[]);
int cmd_get_record (int argc, const char *argv[]);

#endif
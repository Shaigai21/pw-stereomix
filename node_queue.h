#pragma once

#include <stdbool.h>

#include <spa/utils/result.h>
#include <spa/utils/string.h>
#include <spa/utils/defs.h>

#include <pipewire/pipewire.h>
#include <pipewire/filter.h>

struct link_queue
{
    struct spa_list link;
    struct pw_properties *props;
};

struct object
{
    struct spa_list link;

    uint32_t id;
#define OBJECT_ANY 0
#define OBJECT_NODE 1
#define OBJECT_PORT 2
#define OBJECT_LINK 3
    uint32_t type;
    struct pw_properties *props;
    uint32_t extra[2];
    struct pw_proxy *proxy;
    bool exists;
};

struct data
{
    struct pw_main_loop *loop;

    const char *opt_remote;
    bool opt_verbose;
    struct object *in_node;
    struct object *out_node;
    struct pw_properties *props;
    struct pw_proxy *proxy;

    struct pw_context *context;

    struct pw_core *core;
    struct spa_hook core_listener;

    struct pw_registry *registry;
    struct spa_hook registry_listener;

    struct spa_list objects;
    struct spa_list queue;

    struct object discord;
    struct spa_hook link_listener;

    const char **node_names;
    int node_name_count;

    const char *discord_node;

    int sync;
    int link_done;
    bool monitoring;
    bool list_inputs;
    bool list_outputs;
    const char *prefix;

    bool fstg; // marks if the search loop for discord node is running
};

int stereomix_run(const char **node_list, int list_size, const char *dst_name);
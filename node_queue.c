#include "node_queue.h"
#include <stdio.h>
#include <signal.h>
#include <getopt.h>


static bool
str_find(const char *name, const char **node_names, int size)
{
    for (size_t i = 0; i < size; i++) {
        if (spa_streq(name, node_names[i])) {
            return true;
        }
    }
    return false;
}

static void
do_quit(void *userdata, int signal_number)
{
    struct data *data = userdata;
    pw_main_loop_quit(data->loop);
}

static void
link_proxy_error(void *data, int seq, int res, const char *message)
{
    if (res != -EEXIST) {
        exit(1);
    }
}

static const struct pw_proxy_events link_proxy_events = {
    PW_VERSION_PROXY_EVENTS,
    .error = link_proxy_error,
};

static void
create_link_from_queue(struct data *data)
{
    struct link_queue *used = spa_list_first(&data->queue, struct link_queue, link);

    data->proxy = pw_core_create_object(data->core, "link-factory", PW_TYPE_INTERFACE_Link, PW_VERSION_LINK,
                                        &used->props->dict, 0);

    if (!data->proxy) {
        exit(2);
    }

    spa_zero(data->link_listener);
    pw_proxy_add_listener(data->proxy, &data->link_listener, &link_proxy_events, data);
    data->sync = pw_core_sync(data->core, PW_ID_CORE, data->sync);

    spa_list_remove(&used->link);
    pw_properties_free(used->props);
    free(used);
}

static struct object *
find_object(struct data *data, uint32_t type, uint32_t id)
{
    struct object *o;
    spa_list_for_each(o, &data->objects, link) if ((type == OBJECT_ANY || o->type == type) && o->id == id) return o;
    return NULL;
}

static struct object *
find_node_port(struct data *data, struct object *node, enum pw_direction direction, const char *port_id)
{
    struct object *o;

    spa_list_for_each(o, &data->objects, link)
    {
        const char *o_port_id;
        if (o->type != OBJECT_PORT) {
            continue;
        }
        if (o->extra[1] != node->id) {
            continue;
        }
        if (o->extra[0] != direction) {
            continue;
        }
        if (!(o_port_id = pw_properties_get(o->props, PW_KEY_PORT_ID))) {
            continue;
        }
        if (spa_streq(o_port_id, port_id)) {
            return o;
        }
    }

    return NULL;
}

static char *
node_name(char *buffer, int size, struct object *n)
{
    const char *name;
    buffer[0] = '\0';
    if (!(name = pw_properties_get(n->props, PW_KEY_NODE_NAME))) {
        return buffer;
    }
    snprintf(buffer, size, "%s", name);
    return buffer;
}

static void
create_link(struct data *data)
{
    struct link_queue *link_obj = calloc(1, sizeof(struct link_queue));
    link_obj->props = pw_properties_copy(data->props);
    spa_list_append(&data->queue, &link_obj->link);
    if (!data->proxy) {
        create_link_from_queue(data);
    }
}

static int
do_link_ports(struct data *data)
{
    if (data->in_node && data->out_node) {
        int i;
        char port_id[32];

        for (i = 0;; i++) {
            snprintf(port_id, sizeof(port_id), "%d", i);

            struct object *port_out = find_node_port(data, data->out_node, PW_DIRECTION_OUTPUT, port_id);
            struct object *port_in = find_node_port(data, data->in_node, PW_DIRECTION_INPUT, port_id);

            if (!port_out && !port_in) {
                return -ENOENT;
            } else if (!port_in) {
                return -ENOENT;
            } else if (!port_out) {
                return -ENOENT;
            }
            pw_properties_setf(data->props, PW_KEY_LINK_OUTPUT_PORT, "%u", port_out->id);
            pw_properties_setf(data->props, PW_KEY_LINK_INPUT_PORT, "%u", port_in->id);

            create_link(data);
        }
    }

    return 0;
}

static void
registry_event_global(void *data, uint32_t id, uint32_t permissions, const char *type, uint32_t version,
                      const struct spa_dict *props)
{
    struct data *d = data;
    uint32_t t, extra[2];
    struct object *obj;
    const char *str;

    if (!props) {
        return;
    }

    spa_zero(extra);
    if (spa_streq(type, PW_TYPE_INTERFACE_Node)) {
        t = OBJECT_NODE;
    } else if (spa_streq(type, PW_TYPE_INTERFACE_Port)) {
        t = OBJECT_PORT;
        if (!(str = spa_dict_lookup(props, PW_KEY_PORT_DIRECTION))) {
            return;
        }
        if (spa_streq(str, "in")) {
            extra[0] = PW_DIRECTION_INPUT;
        } else if (spa_streq(str, "out")) {
            extra[0] = PW_DIRECTION_OUTPUT;
        } else {
            return;
        }
        if (!(str = spa_dict_lookup(props, PW_KEY_NODE_ID))) {
            return;
        }
        extra[1] = atoi(str);
    } else if (spa_streq(type, PW_TYPE_INTERFACE_Link)) {
        t = OBJECT_LINK;
        if (!(str = spa_dict_lookup(props, PW_KEY_LINK_OUTPUT_PORT))) {
            return;
        }
        extra[0] = atoi(str);
        if (!(str = spa_dict_lookup(props, PW_KEY_LINK_INPUT_PORT))) {
            return;
        }
        extra[1] = atoi(str);
    } else {
        return;
    }

    obj = calloc(1, sizeof(*obj));
    obj->type = t;
    obj->id = id;
    obj->props = pw_properties_new_dict(props);
    obj->proxy = NULL;
    obj->exists = true;
    memcpy(obj->extra, extra, sizeof(extra));
    spa_list_append(&d->objects, &obj->link);

    char name_buff[512];

    if (d->fstg) {
        if (!d->discord.exists && spa_streq(type, PW_TYPE_INTERFACE_Port) && obj->extra[0] == PW_DIRECTION_INPUT) {
            struct object *o;
            spa_list_for_each(o, &d->objects, link)
            {
                if (o->type == OBJECT_NODE && o->id == obj->extra[1] &&
                    spa_streq(node_name(name_buff, 512, o), d->discord_node)) {
                    d->discord = *o;
                    break;
                }
            }
        }
    } else {
        if (spa_streq(type, PW_TYPE_INTERFACE_Node) &&
            str_find(node_name(name_buff, 512, obj), d->node_names, d->node_name_count)) {
            d->in_node = &d->discord;
            d->out_node = obj;
            do_link_ports(d);
        } else if (spa_streq(type, PW_TYPE_INTERFACE_Port)) {
            struct object *o;
            spa_list_for_each(o, &d->objects, link)
            {
                if (o->type == OBJECT_NODE && o->id == obj->extra[1] &&
                    str_find(node_name(name_buff, 512, o), d->node_names, d->node_name_count)) {
                    d->out_node = o;
                    d->in_node = &d->discord;
                    do_link_ports(d);
                    break;
                }
            }
        }
    }
}

static void
registry_event_global_remove(void *data, uint32_t id)
{
    struct data *d = data;
    struct object *obj;

    if (!(obj = find_object(d, OBJECT_ANY, id))) {
        return;
    }

    if (obj->id == d->discord.id) {
        do_quit(data, 0);
    }

    spa_list_remove(&obj->link);
    if (obj->proxy) {
        pw_proxy_destroy(obj->proxy);
    }
    pw_properties_free(obj->props);
    free(obj);
}

static const struct pw_registry_events registry_events = {PW_VERSION_REGISTRY_EVENTS, .global = registry_event_global,
                                                          .global_remove = registry_event_global_remove};

static void
on_core_done(void *data, uint32_t id, int seq)
{
    struct data *d = data;
    if (d->sync == seq && d->fstg) {
        pw_main_loop_quit(d->loop);
    } else if (d->sync == seq) {
        spa_hook_remove(&d->link_listener);
        uint32_t obj_id = pw_proxy_get_bound_id(d->proxy);
        struct object *o = NULL;
        bool found = false;
        spa_list_for_each(o, &d->objects, link)
        {
            if (o->id == obj_id) {
                found = true;
                break;
            }
        }
        if (found) {
            o->proxy = d->proxy;
        } else {
            pw_proxy_destroy(d->proxy);
        }
        d->proxy = NULL;
        if (!spa_list_is_empty(&d->queue)) {
            create_link_from_queue(d);
        }
    }
}

static void
on_core_error(void *data, uint32_t id, int seq, int res, const char *message)
{
    struct data *d = data;

    pw_log_error("error id:%u seq:%d res:%d (%s): %s", id, seq, res, spa_strerror(res), message);

    if (id == PW_ID_CORE && res == -EPIPE) {
        pw_main_loop_quit(d->loop);
    }
}

static const struct pw_core_events core_events = {PW_VERSION_CORE_EVENTS, .done = on_core_done, .error = on_core_error};

int
stereomix_run(const char **node_list, int list_size, const char *dst_name)
{
    pw_init(NULL, NULL);
    struct data data = {.node_name_count = list_size, .node_names = node_list, .discord_node = dst_name};
    int res = 0;

    data.proxy = NULL;
    data.link_done = 0;
    spa_list_init(&data.queue);

    spa_list_init(&data.objects);

    setlinebuf(stdout);

    data.props = pw_properties_new(NULL, NULL);
    if (!data.props) {
        fprintf(stderr, "can't create properties: %m\n");
        res = -1;
        goto exit;
    }

    pw_properties_set(data.props, PW_KEY_OBJECT_LINGER, "false");

    data.loop = pw_main_loop_new(NULL);
    if (!data.loop) {
        fprintf(stderr, "can't create mainloop: %m\n");
        res = -1;
        goto exit;
    }
    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGINT, do_quit, &data);
    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGTERM, do_quit, &data);

    data.context = pw_context_new(pw_main_loop_get_loop(data.loop), NULL, 0);
    if (!data.context) {
        fprintf(stderr, "can't create context: %m\n");
        res = -1;
        goto exit;
    }

    data.core = pw_context_connect(data.context, pw_properties_new(PW_KEY_REMOTE_NAME, data.opt_remote, NULL), 0);
    if (!data.core) {
        fprintf(stderr, "can't connect: %m\n");
        res = -1;
        goto exit;
    }

    pw_core_add_listener(data.core, &data.core_listener, &core_events, &data);

    data.registry = pw_core_get_registry(data.core, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(data.registry, &data.registry_listener, &registry_events, &data);

    data.sync = pw_core_sync(data.core, PW_ID_CORE, data.sync);
    data.fstg = true;
    pw_main_loop_run(data.loop);
    if (data.discord.exists) {
        data.fstg = false;
        pw_main_loop_run(data.loop);
    }

    if (data.proxy) {
        pw_proxy_destroy(data.proxy);
    }

    while (!spa_list_is_empty(&data.queue)) {
        struct link_queue *o = spa_list_first(&data.queue, struct link_queue, link);
        spa_list_remove(&o->link);
        pw_properties_free(o->props);
        free(o);
    }

    while (!spa_list_is_empty(&data.objects)) {
        struct object *o = spa_list_first(&data.objects, struct object, link);
        spa_list_remove(&o->link);
        pw_properties_free(o->props);
        if (o->proxy) {
            pw_proxy_destroy(o->proxy);
        }
        free(o);
    }

exit:
    pw_properties_free(data.props);
    spa_hook_remove(&data.registry_listener);
    pw_proxy_destroy((struct pw_proxy *) data.registry);
    spa_hook_remove(&data.core_listener);
    pw_core_disconnect(data.core);
    pw_context_destroy(data.context);
    pw_main_loop_destroy(data.loop);
    pw_deinit();

    return res;
}

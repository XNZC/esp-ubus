#include "ubus.h"

#include <libubus.h>
#include <libubox/blobmsg_json.h>
#include <signal.h>
#include <unistd.h>

/* --- */

int ubus_start_server();
void ubus_cleanup();
void update_ports(serial_port* p);

static int handle_on_off(int select, struct blob_attr *msg, struct ubus_request_data *req);
static serial_port* find_port(char* name);
static void server_main();
static void send_reply(char* text, struct ubus_request_data *req);

static int devices_handler(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req, const char *method, struct blob_attr *msg);
static int on_handler(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req, const char *method, struct blob_attr *msg);
static int off_handler(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req, const char *method, struct blob_attr *msg);
static int get_handler(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req, const char *method, struct blob_attr *msg);

enum {
    OF_PORT,
    OF_PIN,
    __OF_MAX
};

enum {
    G_PORT,
    G_SENSOR,
    G_PIN,
    G_MODEL,
    __GET_MAX
};

static const struct blobmsg_policy off_on_policy[__OF_MAX] = {
    [OF_PORT] = { .name = "port", .type = BLOBMSG_TYPE_STRING },
    [OF_PIN] = { .name = "pin", .type = BLOBMSG_TYPE_INT32 },
};

static const struct blobmsg_policy get_policy[__GET_MAX] = {
    [G_PORT] = { .name = "port", .type = BLOBMSG_TYPE_STRING },
    [G_SENSOR] = {.name= "sensor", .type = BLOBMSG_TYPE_STRING},
    [G_PIN] = { .name = "pin", .type = BLOBMSG_TYPE_INT32 },
    [G_MODEL] = {.name = "model", .type = BLOBMSG_TYPE_STRING},
};

static const struct ubus_method methods[] = {
    UBUS_METHOD_NOARG("devices", devices_handler),
    UBUS_METHOD("off", off_handler, off_on_policy),
    UBUS_METHOD("on", on_handler, off_on_policy),
    UBUS_METHOD("get", get_handler, get_policy),
};

static struct ubus_object_type object_type = UBUS_OBJECT_TYPE("esp.service", methods);
static struct ubus_object object = {
    .name = "esp.service",
    .type = &object_type,
    .methods = methods,
    .n_methods = ARRAY_SIZE(methods),
};

static struct ubus_context* ctx;
static struct blob_buf b;
serial_port* ports = NULL;

/* --- */

static int devices_handler(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req, const char *method, struct blob_attr *msg) {
    (void) obj;
    (void) method;
    (void) msg;
    
    update_port_list(&ports);
    blob_buf_init(&b, 0);

    void* arr = blobmsg_open_array(&b, "ports");

    if (ports != NULL) {

        for (serial_port* port = ports; port != NULL; port = port->next){
            void* table = blobmsg_open_table(&b, "port");
            int vid;
            int pid;
            
            int result = get_vendor_product_ids(port, &vid, &pid);
            if (result == -1) {
                continue;
            }

            char vid_c[128];
            char pid_c[128];

            snprintf(vid_c, 128, "%x", vid);
            snprintf(pid_c, 128, "%x", pid);

            blobmsg_add_string(&b, "port", port->port);
            
            blobmsg_add_string(&b, "VID", vid_c);
            blobmsg_add_string(&b, "PID", pid_c);

            blobmsg_close_table(&b, table);
        }
        
    }

    blobmsg_close_array(&b, arr);
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);

    return 0;
}

static serial_port* find_port(char* name) {
    for (serial_port* port = ports; port != NULL; port = port->next) {
        if (strcmp(port->port, name) == 0) {
            return port;
        }
    }

    return NULL;
}

static void send_reply(char* text, struct ubus_request_data *req) {
    blob_buf_init(&b, 0);
    blobmsg_add_json_from_string(&b, text);
    ubus_send_reply(ctx, req, b.head);
    blob_buf_free(&b);
}

static int handle_on_off(int select, struct blob_attr *msg, struct ubus_request_data *req) {
    if (ports == NULL) {return UBUS_STATUS_UNKNOWN_ERROR;}
    
    struct blob_attr* tb[__OF_MAX];
    blobmsg_parse(off_on_policy, __OF_MAX, tb, blobmsg_data(msg), blob_len(msg));
    
    if (!tb[OF_PIN] || !tb[OF_PORT]) {
        return UBUS_STATUS_INVALID_ARGUMENT;
    }

    char* port = blobmsg_get_string(tb[OF_PORT]);
    uint32_t pin = blobmsg_get_u32(tb[OF_PIN]);
    serial_port* sp = find_port(port);

    if (sp == NULL) {
        return UBUS_STATUS_INVALID_ARGUMENT;
    }

    blob_buf_init(&b, 0);

    if (select == 0) {
        blobmsg_add_string(&b, "action", "on");
    }else {
        blobmsg_add_string(&b, "action", "off");
    }
    
    blobmsg_add_u32(&b, "pin", pin);
    
    char response[256];
    char* str = blobmsg_format_json(b.head, 1);
    send_command(sp, str, response);
    

    free(str);
    blob_buf_free(&b);

    send_reply(response, req);

    return 0;
}

static int on_handler(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req, const char *method, struct blob_attr *msg) {
    (void) obj;
    (void) method;
    (void) ctx;

    update_port_list(&ports);
    return handle_on_off(0, msg, req);
}

static int off_handler(struct ubus_context *ctx, struct ubus_object *obj,struct ubus_request_data *req, const char *method, struct blob_attr *msg) {
    (void) obj;
    (void) method;
    (void) ctx;
    
    update_port_list(&ports);
    return handle_on_off(1, msg, req);
}

static int get_handler(struct ubus_context *ctx, struct ubus_object *obj, struct ubus_request_data *req, const char *method, struct blob_attr *msg) {             
    (void) obj;
    (void) method;
    (void) ctx;

    update_port_list(&ports);
    if (ports == NULL) {return UBUS_STATUS_UNKNOWN_ERROR;}
    
    struct blob_attr* tb[__GET_MAX];
    blobmsg_parse(get_policy, __GET_MAX, tb, blobmsg_data(msg), blob_len(msg));
    
    if (!tb[G_PORT] || !tb[G_PIN] || !tb[G_MODEL] || !tb[G_SENSOR]) {
        return UBUS_STATUS_INVALID_ARGUMENT;
    }

    char* port = blobmsg_get_string(tb[G_PORT]);
    uint32_t pin = blobmsg_get_u32(tb[G_PIN]);
    char* model = blobmsg_get_string(tb[G_MODEL]);
    char* sensor = blobmsg_get_string(tb[G_SENSOR]);

    serial_port* sp = find_port(port);

    if (sp == NULL) {
        return UBUS_STATUS_INVALID_ARGUMENT;
    }

    blob_buf_init(&b, 0);

    blobmsg_add_string(&b, "action", "get");
    blobmsg_add_string(&b, "sensor", sensor);
    blobmsg_add_u32(&b, "pin", pin);
    blobmsg_add_string(&b, "model", model);

    char response[256];
    char* str = blobmsg_format_json(b.head, 1);
    send_command(sp, str, response);

    free(str);
    blob_buf_free(&b);

    send_reply(response, req);

    return 0;
}

static void server_main(){
    int ret;

    ret = ubus_add_object(ctx, &object);
    if (ret) {
        fprintf(stderr, "Failed to add object: %s\n", ubus_strerror(ret));
    }

    uloop_run();
}

int ubus_start_server() {
    update_port_list(&ports);
    
    uloop_init();
    signal(SIGPIPE, SIG_IGN);
    
    ctx = ubus_connect(NULL);
    if (!ctx) {
        fprintf(stderr, "Failed to connect to ubus\n");
        return -1;
    }

    ubus_add_uloop(ctx);
    server_main();

    return 0;
}

void ubus_cleanup() {
    ubus_free(ctx);
    uloop_done();
    free_port_list(ports);
}
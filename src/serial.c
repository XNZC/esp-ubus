#include "serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- */

#define VID 0x10C4
#define PID 0xEA60

#define TIMEOUT 2000

static inline int check_err(enum sp_return result);
static inline int is_esp_device(struct sp_port *port);
static inline int configure_port(struct sp_port* port);
static inline int device_exists(serial_port** first, struct sp_port* port);
static inline int struct_exists(serial_port* port, struct sp_port **port_list);
static inline void insert_port(serial_port **first, serial_port **current, struct sp_port *port);
static inline void remove_port(serial_port** first, serial_port* port);

int get_vendor_product_ids(serial_port* port, int* vendor_id, int* product_id);
int update_port_list(serial_port** first);
int send_command(serial_port *port, char* command, char* response);
void free_port_list(serial_port *first);

/* --- */

static inline int is_esp_device(struct sp_port *port)
{
    if (port == NULL){
        return -1;
    }

    int usb_vid, usb_pid;
    enum sp_return result = sp_get_port_usb_vid_pid(port, &usb_vid, &usb_pid);
    if (result != SP_OK){
        return -1;
    }

    if (usb_pid == PID && usb_vid == VID){
        return 0;
    }

    return -1;
}

static inline int device_exists(serial_port** first, struct sp_port* port){
    for (serial_port* p = (*first); p != NULL; p = p->next){
        if (strcmp(p->port, sp_get_port_name(port)) == 0) {
            return 0;
        }
    }

    return -1;
}

static inline int struct_exists(serial_port* port, struct sp_port **port_list) {
    if (port == NULL || port_list == NULL) {return -1;}

    int exists = 0;

    for (int i = 0; port_list[i] != NULL; i++) {
        struct sp_port *spport = port_list[i];

        if (strcmp(port->port, sp_get_port_name(spport)) == 0) {
            exists = 1;
            break;
        }
    }

    return exists;
}

static inline void remove_port(serial_port** first, serial_port* port) {
    if (port == NULL) {return;}

    if (port == (*first) && port->next != NULL) {
        (*first) = port->next;
        goto exit;
    }else if (port == *(first)){
        (*first) = NULL;
        goto exit;
    }

    for (serial_port* c = (*first); c != NULL; c = c->next){
        if (c->next != port) {
            continue;
        }

        if (port->next == NULL) {
            c->next = NULL;
            break;
        }

        c->next = port->next;
        break;
    }

exit:
    free(port);
}

static inline void insert_port(serial_port **first, serial_port **current, struct sp_port *port)
{
    if (port == NULL){
        return;
    }

    serial_port *tmp = (serial_port *)malloc(sizeof(serial_port));
    if (tmp == NULL){
        return;
    }

    tmp->next = NULL;

    strncpy(tmp->port, sp_get_port_name(port), 128);

    if ((*first) == NULL){
        (*current) = tmp;
        (*first) = tmp;
        return;
    }

    (*current)->next = tmp;
    (*current) = tmp;
}


int update_port_list(serial_port** first)
{
    struct sp_port **port_list;
    enum sp_return result = sp_list_ports(&port_list);

    if (result != SP_OK){
        return -1;
    }

    serial_port *last = (*first);

    while (last != NULL) {
        serial_port* tmp = last;

        int exists = struct_exists(tmp, port_list);
        if (exists == 0) {
            remove_port(first, tmp);
        }

        if (last->next != NULL) {
            last = last->next;
        }else {
            break;
        }
    }

    for (int i = 0; port_list[i] != NULL; i++){
        struct sp_port *port = port_list[i];

        if (is_esp_device(port) != 0){
            continue;
        }

        if (device_exists(first, port) == -1) {
            insert_port(first, &last, port);
        }
    }

    sp_free_port_list(port_list);

    return 0;
}

int send_command(serial_port *port, char* command, char* response)
{
    if (port == NULL || command == NULL){
        return -1;
    }

    struct sp_port *p_port;
    sp_get_port_by_name(port->port, &p_port);
    int result = configure_port(p_port);

    if (result != 0){
        return -1;
    }

    int size = strlen(command);
    result = check_err(sp_blocking_write(p_port, command, size, TIMEOUT));
    if (result != size) {
        printf("Timed out, %d/%d bytes sent.\n", result, size);
        return -1;
    }

    result = check_err(sp_blocking_read(p_port, response, 256, TIMEOUT));
    response[result] = '\0';

    result = check_err(sp_close(p_port));
    if (result != SP_OK) {
        printf("Failed to close port");
    }

    sp_free_port(p_port);
    return 0;
}

int get_vendor_product_ids(serial_port* port, int* vendor_id, int* product_id){
    if (port == NULL){
        return -1;
    }

    struct sp_port* spport;
    enum sp_return result = sp_get_port_by_name(port->port, &spport);
    if (result != SP_OK){
        return -1;
    }

    int usb_vid, usb_pid;
    result = sp_get_port_usb_vid_pid(spport, &usb_vid, &usb_pid);
    if (result != SP_OK){
        sp_free_port(spport);
        return -1;
    }

    (*vendor_id) = usb_vid;
    (*product_id) = usb_pid;

    sp_free_port(spport);
    return 0;
}

void free_port_list(serial_port *first)
{
    serial_port *tmp = first;
    while (first != NULL){
        tmp = first->next;

        free(first);

        first = tmp;
    }
}

static inline int check_err(enum sp_return result)
{
    char *error_message;
    switch (result){
    case SP_ERR_ARG:
        printf("Error: Invalid argument.\n");
        return result;
    case SP_ERR_FAIL:
        error_message = sp_last_error_message();
        printf("Error: Failed: %s\n", error_message);
        sp_free_error_message(error_message);
        return result;
    case SP_ERR_SUPP:
        printf("Error: Not supported.\n");
        return result;
    case SP_ERR_MEM:
        printf("Error: Couldn't allocate memory.\n");
        return result;
    case SP_OK:
    default:
        return result;
    }
}

static inline int configure_port(struct sp_port* port){
    int ok = check_err(sp_open(port, SP_MODE_READ_WRITE));
    if (ok != SP_OK) {return -1;}

    ok = check_err(sp_set_baudrate(port, 9600));
    if (ok != SP_OK) {return -1;}

    ok = check_err(sp_set_bits(port, 8));
    if (ok != SP_OK) {return -1;}

    ok = check_err(sp_set_parity(port, SP_PARITY_NONE));
    if (ok != SP_OK) {return -1;}

    ok = check_err(sp_set_stopbits(port, 1));
    if (ok != SP_OK) {return -1;}

    ok = check_err(sp_set_flowcontrol(port, SP_FLOWCONTROL_NONE));
    if (ok != SP_OK) {return -1;}

    return 0;
}
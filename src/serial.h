#ifndef SERIAL_H
#define SERIAL_H

#include <libserialport.h>

typedef struct serial_portT{
    char port[128];
    struct serial_portT* next;
} serial_port;

int get_vendor_product_ids(serial_port* port, int* vendor_id, int* product_id);
int update_port_list(serial_port** first);
void free_port_list(serial_port* first);
int send_command(serial_port* port, char* command, char* response);

#endif /*SERIAL_H*/
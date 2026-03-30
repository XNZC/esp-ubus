#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "ubus.h"
#include "serial.h"


int main(){
    int result = ubus_start_server();
    if (result == -1) {
        return -1;
    }

    ubus_cleanup();

    return 0;
}
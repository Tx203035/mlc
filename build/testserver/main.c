#include <stdio.h>

#include "mlc.h"
#include "cycle.h"
#include "session.h"
#include "core/log.h"

int on_recv(session_t* s,const char *data,uint32_t len, void *userdata)
{
    log_info(NULL,"session recv data, data=[%s]\n", data);
    int td = 0;
    sscanf(data, "testdata=%d", &td);
    return len;
}
int on_status(session_t *s,uint8_t state, void *userdata)
{
    log_info(NULL,"state=%d",state);
    return 0;
}
int on_connect(session_t *s, void *data)
{
    log_info(NULL,"");
    return 0;
}

int main(int argc, char const *argv[])
{
    cycle_conf_t conf = {0,};
    conf.connection_n = 10;
    conf.debugger = 1;
    mlc_addr_conf_t listen_addr = {"0.0.0.0",8888};
    cycle_t* cycle = cycle_create(&conf,1);
    cycle->sl = session_listener_create(cycle, &listen_addr);
    assert(cycle);
    session_listener_set_handler(cycle->sl, NULL, on_connect, on_recv, on_status);
    return cycle_process(cycle, 100);
}

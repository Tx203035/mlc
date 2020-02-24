#include <stdio.h>

#include "cycle.h"
#include "core/socket_util.h"
#include "core/log.h"
#include "tunnel.h"



mlc_tunnel_conf_t tun_conf = {
    1,
    {
        "0.0.0.0",
        7777,
    },
    {
        "127.0.0.1",
        8388
    },
};


int main(int argc, char const *argv[])
{
    cycle_conf_t cycle_conf ={0,};
    cycle_t *cycle = cycle_create(&cycle_conf, 1);
    assert(cycle);
    mlc_tunnel_t *tun = mlc_tunnel_create(cycle, &tun_conf);
    int ret = cycle_process(cycle, 10);
    return ret;
}

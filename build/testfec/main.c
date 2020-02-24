#include <stdio.h>

#include "mlc.h"
#include "fec.h"
#include "core/encoding.h"
#include "core/chain.h"
#include "connection.h"
#include "cycle.h"

static chain_t *chain_array[100];
int chain_array_cur = 0;

void show_chain(chain_t *chain, const char *tag)
{
    pkg_debug(chain, tag);
    printf("%s\n", chain_to_data(chain)->fec.data);
}

int output(chain_t *chain, void *t)
{
    show_chain(chain, "send");
    chain_retain_one(chain, mlc_chain_mask_debug);
    chain_array[chain_array_cur++] = chain;
    return 0;
}
int input(chain_t *chain, void *t)
{
    show_chain(chain, "recv");
    return 0;
}

chain_t *fec_data_chain_alloc(mlc_pool_t *pool, size_t fec_data_len)
{
    //assert(fec_data_len >= 17);
    chain_t *chain = chain_alloc(pool, chain_len_from_fec(fec_data_len), MLC_PKG_MAX_SIZE, mlc_chain_mask_fec_write);

    mlc_pkg_data_t *data = chain_to_data(chain);
    data->pkg.type = MLC_PKG_TYPE_DATA;
    data->pkg.len = chain_to_connection_len(chain);

    mlc_pkg_fec_t *fec_pkg = chain_to_fec(chain);
    fec_pkg->len = fec_data_len;

    return chain;
}

int main(int argc, char const *argv[])
{
    cycle_conf_t conf = {0,};
    cycle_t *cycle = cycle_create(&conf,0);

    fec_pcb_t *fec = fec_create(cycle,NULL, 2, 1);
    fec->output = output;

    fec_pcb_t *fec_rcv = fec_create(cycle,NULL, 3, 1);
    fec_rcv->input = input;

    for (int i=0; i < 24; i++)
    {
        char send_str[100];
        sprintf(send_str, "%d%d%d", i, i + 1, i + 2);
        size_t datalen = strlen(send_str) + 1;

        chain_t *chain = fec_data_chain_alloc(cycle->pool, datalen);

        mlc_pkg_fec_t * fec_pkg = chain_to_fec(chain);
        strcpy(fec_pkg->data, send_str);

        fec_send(fec, chain, 1);
    }

    printf("start recv\n");

    for (int i = 0; i < 36; i++)
    {
        if (i % 3 == 0)
        {
            continue;
        }
        chain_t *chain = chain_array[i];
        fec_input(fec_rcv, chain);
    }

    fec_release(fec);
    fec_release(fec_rcv);
    fec_global_release();
    printf("test end\n");
    return 0;
}

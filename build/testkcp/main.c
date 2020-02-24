#include <stdio.h>

#include "mlc.h"
#include "kcp.h"
#include "core/encoding.h"
#include "core/chain.h"
#include "connection.h"
#include "fec.h"
#include "session.h"
#include "cycle.h"

static chain_t *chain_array_s[1000];
int chain_array_cur_s = 0;
static chain_t *chain_array_c[1000];
int chain_array_cur_c = 0;

void show_chain(chain_t *chain, const char *tag, void *t)
{
    char tagstr[100];
    sprintf(tagstr,"%p:%s",t,tag);
    pkg_debug(chain, tagstr);
    printf("%s:%s\n", tagstr ,chain_to_data(chain)->kcp.data);
}

int output(chain_t *chain, void *t)
{
    chain_retain_one(chain,mlc_chain_mask_debug);
    show_chain(chain,"send",t);
    if ((int64_t)t == 1)
    {
        chain_array_s[chain_array_cur_s++] = chain;
    }
    else
    {
        chain_array_c[chain_array_cur_c++] = chain;
    }
    return 0;
}

int input(chain_t *chain, void *t)
{
    show_chain(chain,"recv",t);
    return 0;
}

chain_t *kcp_data_chain_alloc(mlc_pool_t *pool, size_t data_len)
{
    chain_t *c = chain_alloc(pool, mlc_offset_of(mlc_pkg_data_t, kcp.data) + data_len, MLC_PKG_MAX_SIZE, 0);

    mlc_pkg_data_t *data = chain_to_data(c);
    //data->kcp.len = data_len;
    data->pkg.type = MLC_PKG_TYPE_DATA;
    data->pkg.len = chain_to_connection_len(c);

    return c;
}

int main(int argc, char const *argv[])
{
    cycle_conf_t conf = {0};
    cycle_t *cycle = cycle_create(&conf,0);
    kcp_pcb_t *s = kcp_create(cycle,NULL);
    s->output = output;
    s->input = input;
    s->data = (void *)1;

    kcp_pcb_t *c = kcp_create(cycle,NULL);
    c->input = input;
    c->output = output;
    c->data = (void *)0;

    uint32_t t_s = 10000;
    kcp_update(s,t_s);
    kcp_update(c,t_s);
    int test_n = 20;
    int cur_s = 0;
    int cur_c = 0;
    for (int i = 0; i < test_n; i++)
    {
        char testdata[1000] = {0,};
        sprintf(testdata, "[%d]%d,%d", i, i + 1, i + 2);
        size_t data_len = strlen(testdata) + 1;
        chain_t *chain = kcp_data_chain_alloc(cycle->pool, data_len);
        char *p = chain_to_data(chain)->kcp.data;
        strcpy(p, testdata);
        kcp_send(s, chain, 1);
        kcp_update(s, t_s);
        t_s += 50;
        kcp_update(c, t_s);
        t_s += 50;
        while (cur_s < chain_array_cur_s)
        {
            if(cur_s % 9 <= 7){
                kcp_input(c, chain_array_s[cur_s]);
            }
            cur_s++;
        }
        while(cur_c < chain_array_cur_c)
        {
            kcp_input(s, chain_array_c[cur_c++]);
        }
    }

    // kcp_release(s);
    // kcp_release(c);
    printf("test end");
    return 0;
}

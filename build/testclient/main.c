#include <stdio.h>

#include "mlc.h"
#include "cycle.h"
#include "core/socket_util.h"
#include "core/util.h"
#include "core/log.h"
#include "session.h"
#include "event_timer.h"

char test_send_str[1000];
session_t *s = NULL;
int seq = 1000;
int need_close = 0;
static logger_t logger = {0,};
static logger_t *l = &logger;

void gen_test_str(char *test_send_str,int cur_seq,uint64_t t)
{
    char random_len_str[10000];
    size_t rand_len = 200;//rand() % 1000;
    for(size_t i = 0; i < rand_len ; i++)
    {
       random_len_str[i] = (uint8_t)(rand() % (127-32)) + 32;
    }
    random_len_str[rand_len] = 0;
    sprintf(test_send_str, "testdata=%d,%"PRIu64",%s", cur_seq,t,random_len_str);
    log_info(l,"[send data]testdata=%d len=%d",cur_seq,rand_len);
}


int on_send(session_t* s)
{
    if (s && s->state==mlc_state_connected && s->send_busy==0)
    {
        gen_test_str(test_send_str,++seq,mlc_clock64_ms());
        session_data_send(s, (void*)test_send_str, strlen(test_send_str) + 1);
    }
    return 0;
}

int on_recv(session_t *s,const char *data,uint32_t len, void *userdata)
{
    uint64_t t = 0;
    int seq = 0;
    sscanf(data,"testdata=%d,%"PRIu64",",&seq,&t);
    uint64_t delay = mlc_clock64_ms() - t;
    log_info(l,"[recv data len=%"PRIu64"delay=%llu]%s",len,delay,data);
    return 0;
}

int on_status_change(session_t *s,uint8_t state, void *userdata)
{
    log_info(l,"[session %p status change]state=%d",s,state);
    if (state == mlc_state_closed) 
    {
		need_close = 1;
        //session_graceful_close(s, 0);
        
    }
    return 0;
}


int main(int argc, char const *argv[])
{
    int cnt = get_all_iface_name();
    log_info(l,"inet cnt=%d",cnt);
    cycle_conf_t conf = {0,};
    conf.connection_n = 10;
    conf.debugger = 1;
    cycle_t* cycle = cycle_create(&conf,0);
    assert(cycle);
    s = session_create_client_by_url(cycle,"mlc://127.0.0.1:8888");
    session_set_receive_function(s, on_recv);
    session_set_status_change_function(s, on_status_change);
    session_connect(s);

    event_timer_t *send_timer = event_timer_create(cycle->timer_hub, s, (event_timer_handler_pt)on_send);
    event_timer_start(send_timer, 200, 1);

	for (int i = 0;; i++)
	{
		if (i % 2000 == 0)
		{
			log_info(l,"main loop %d", i);
			if (need_close)
			{
				break;
			}
		}
		cycle_step(cycle, 10);
	}
	session_close(s, 0);
	cycle_destroy(cycle);
	return 0;
}

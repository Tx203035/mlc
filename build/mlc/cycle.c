#include "cycle.h"
#include "fec.h"
#include "connection.h"
#include "core/palloc.h"
#include "core/log.h"
#include "event.h"
#include "session.h"
#include "core/util.h"
#include "core/rs.h"
#include "debugger.h"
#include "event_timer.h"
#include "core/pool.h"
#include "core/tpool.h"
#include "sinfo.h"



static int cycle_init_conf(cycle_conf_t *conf)
{
#define SET_DEFAULT_V(x,v) if(x==0){ x = v;}
    SET_DEFAULT_V(conf->connection_n, 2048);
    SET_DEFAULT_V(conf->backlog,1024);
    SET_DEFAULT_V(conf->heartbeat_check_ms, 5000);
    SET_DEFAULT_V(conf->heartbeat_timeout_ms, 15000);
    SET_DEFAULT_V(conf->connect_timeout_ms, 10000);
    SET_DEFAULT_V(conf->lost_timeout_ms, 30000);
    SET_DEFAULT_V(conf->mtu, MLC_DEFAULT_MTU);
    SET_DEFAULT_V(conf->rs.data_shard,10);
    SET_DEFAULT_V(conf->rs.parity_shard,3);
    SET_DEFAULT_V(conf->pool_block_cnt, 10240);
    SET_DEFAULT_V(conf->pool_block_size, 4096);
    SET_DEFAULT_V(conf->pool_small_block_cnt,2560);
    SET_DEFAULT_V(conf->pool_small_block_size,1280);
    SET_DEFAULT_V(conf->session_write_buffer_size, 10*1024);
    SET_DEFAULT_V(conf->session_write_queue_size, 2000);
#undef SET_DEFAULT_V
    return 0;
}



cycle_t *cycle_create(const cycle_conf_t *conf,int is_server)
{
#ifdef WINDOWS
	int iResult;
	WSADATA wsaData;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		return NULL;
	}
#endif

#ifndef WINDOWS
	signal(SIGPIPE, SIG_IGN);
#endif
    fec_global_init();
    cycle_t *cycle = (cycle_t *)malloc(sizeof(cycle_t));
    assert(cycle);
    memset(cycle, 0, sizeof(cycle_t));
    cycle_init_loggers(cycle);
    log_warn(L_SYS(cycle),"cycle_create connect_n=%d\n", conf->connection_n);
    cycle->is_server = is_server;
    if (conf){
        memcpy(&cycle->conf, conf, sizeof(cycle_conf_t));
    }
    cycle_init_conf(&cycle->conf);
    const cycle_conf_t *cf = &cycle->conf;
    cycle->pool = mlc_create_pool(cf->pool_block_size,cf->pool_block_cnt);
    cycle->pool_small = mlc_create_pool(cf->pool_small_block_size,cf->pool_small_block_cnt);
    if (cycle->pool == NULL || cycle->pool_small == NULL) {
        return NULL;
    }

    int ret = mlc_io_mult_init(cycle);
    if (ret < 0)
    {
        return NULL;
    }
    cycle->time_last = 0;
    cycle->time_now = mlc_clock64();
    cycle->time_now_ms = cycle->time_now / 1000;
    cycle->timer_hub = event_timer_hub_create(cycle);
    assert(cycle->timer_hub);
    cycle->cm = tpool_create(cf->connection_n, sizeof(connection_t), cycle,
                             (tpool_init_func)connection_init,
                             (tpool_destroy_func)connection_release,
                             (tpool_reset_func)connection_reset);
    printf("tpool_create connection");
    assert(cycle->cm);
    cycle->sm = tpool_create(cf->connection_n, sizeof(session_t),
                             cycle, (tpool_init_func)session_init, (tpool_destroy_func)session_close, (tpool_reset_func)session_reset);
    printf("tpool_create session");

    assert(cycle->sm);
    if (cycle->sm == NULL || cycle->cm == NULL)
    {
        return NULL;
    }
    cycle->sc = sinfo_collect_create(cycle);
    if(!cycle->sc)
    {
        return NULL;
    }
    
#ifndef WINDOWS
    if (conf->debugger) {
        create_debugger(cycle);
    }
#endif
    
    printf("create_connection_ctx done\n");
    return cycle;
}

int cycle_destroy(cycle_t *cycle)
{
	if (cycle != NULL)
	{
        sinfo_collect_destroy(cycle->sc);
        cycle_destroy_loggers(cycle);
		free(cycle);
	}
	return 0;
}


int cycle_step(cycle_t *cycle,int wait_ms)
{
    tpool_update(cycle->sm);
    tpool_update(cycle->cm);
    sinfo_collect_update(cycle->sc);
    cycle->time_last = cycle->time_now;
    cycle->time_now = mlc_clock64();
    cycle->time_now_ms = cycle->time_now / 1000;
    event_timer_hub_process(cycle->timer_hub);
    int ret = mlc_io_mult_process(cycle,wait_ms,0);
    if (ret < 0) {
        log_error(L_SYS(cycle),"io mult process err ret=%d",ret);
        return ret;
    }
    if (ret==0 && cycle->handler_idle)
    {
        cycle->handler_idle(cycle);
    }
    return 0;
}

int cycle_pause(int pause)
{
	return 0;
}

int cycle_trace(cycle_t *cycle)
{
    logger_t *l = L_SYS(cycle);
    log_trace(l,"[cycle]-------------cycle trace--------------");
    mlc_pool_trace(l,cycle->pool);
    mlc_pool_trace(l,cycle->pool_small);
    tpool_trace(l,cycle->sm);
    tpool_trace(l,cycle->cm);
    sinfo_collect_trace(l,cycle->sc);
    statistic_t *stat = &cycle->stat;
    log_trace(l,"statistic: sent=%llu,recv=%llu",stat->sent,stat->recv);
    log_trace(l,"[cycle]-------------cycle trace_end-----------");
    return 0;
}

int cycle_process(cycle_t *cycle, int step_wait_ms)
{
    for(int i=0;;i++)
    {
        if (i % 2000 == 0)
        {
            log_info(L_SYS(cycle),"main loop %d", i);
            cycle_trace(cycle);
        }
        int ret = cycle_step(cycle, step_wait_ms);
        if (ret < 0)
        {
        }
    }
    return 0;
}



int cycle_init_loggers(cycle_t *cycle)
{
  	for(int i=0; i< LOGGER_MAX; i++)
	{
		logger_t *l = malloc(sizeof(logger_t));
        memset(l,0,sizeof(*l));
        cycle->loggers[i] = l;
	}  
    log_set_quiet(cycle->loggers[LOGGER_KCP],1);
    log_set_quiet(cycle->loggers[LOGGER_FEC],1);
    log_set_quiet(cycle->loggers[LOGGER_SESSION],0);
    log_set_quiet(cycle->loggers[LOGGER_CONN],1);
	return 0;
}

int cycle_destroy_loggers(cycle_t *cycle)
{
	for(int i=0; i< LOGGER_MAX; i++)
	{
		logger_t *l = cycle->loggers[i];
		free(l);
	}
    return 0;
}


void cycle_set_logger_output(cycle_t *cycle,log_output_function output)
{
    log_set_output(&glogger,output);
    for(int i=0; i< LOGGER_MAX; i++)
	{
        log_set_output(cycle->loggers[i],output);
	}  
}

void cycle_set_logger_level(cycle_t *cycle,int level)
{
    log_set_level(&glogger,level);
    for(int i=0; i< LOGGER_MAX; i++)
	{
        log_set_level(cycle->loggers[i],level);
	}          
}
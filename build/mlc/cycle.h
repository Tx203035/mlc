#ifndef _CONNECTION_CONTEXT_H_
#define _CONNECTION_CONTEXT_H_

#include "mlc.h"

typedef int (*cycle_idle_handler)(cycle_t *c);

struct kevent_env_s;
struct select_env_s;
struct epoll_env_s;

enum 
{
    LOGGER_SYS,
    LOGGER_CONN,
    LOGGER_SESSION,
    LOGGER_FEC,
    LOGGER_KCP,
    LOGGER_MAX,
};

#define L_SYS(cycle) (cycle->loggers[LOGGER_SYS])
#define L_CON(cycle) (cycle->loggers[LOGGER_CONN])
#define L_SES(cycle) (cycle->loggers[LOGGER_SESSION])
#define L_FEC(cycle) (cycle->loggers[LOGGER_FEC])
#define L_KCP(cycle) (cycle->loggers[LOGGER_KCP])

typedef struct statistic_s
{
    uint64_t sent;        
    uint64_t recv;        
}statistic_t;

struct cycle_s
{
    struct cycle_conf_s conf;
    int is_server;
    tpool_t *cm;
    tpool_t *sm;
    sinfo_collect_t *sc;

    uint64_t time_now;
    uint64_t time_now_ms;
    uint64_t time_last;
    cycle_idle_handler handler_idle;

    int ep;
    mlc_pool_t *pool;
    mlc_pool_t *pool_small;
    event_timer_hub_t *timer_hub;
    session_listener_t *sl;
    union
    {
        struct kevent_env_s *kevent_env;
        struct select_env_s *select_env;
        struct epoll_env_s *epoll_env;
    };
    logger_t *loggers[LOGGER_MAX];
    statistic_t stat;
};

MLC_API cycle_t *cycle_create(const cycle_conf_t *conf, int is_server);
MLC_API int cycle_destroy(cycle_t *cycle);
MLC_API int cycle_step(cycle_t *cycle, int wait_ms);
MLC_API int cycle_pause(int pause);
MLC_API int cycle_process(cycle_t *cycle, int step_wait_ms);
MLC_API int cycle_trace(cycle_t *cycle);
MLC_API void cycle_set_logger_output(cycle_t *cycle,log_output_function output);
MLC_API void cycle_set_logger_level(cycle_t *cycle,int level);
int cycle_init_loggers(cycle_t *cycle);
int cycle_destroy_loggers(cycle_t *cycle);



#endif
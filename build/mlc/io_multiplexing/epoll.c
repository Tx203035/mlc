#include "event.h"
#include "cycle.h"
#include "core/core.h"
#include "core/log.h"

#ifdef MLC_USE_EPOLL
static int epoll_init(cycle_t *cycle);
static int epoll_done(cycle_t *cycle) { return 0; };
static int epoll_process(cycle_t *cycle, int timer, unsigned flags);
static int epoll_add_event(cycle_t *cycle, event_t *ev, unsigned flags);
static int epoll_mod_event(cycle_t *cycle, event_t *ev, unsigned flags);
static int epoll_del_event(cycle_t *cycle, event_t *ev);

typedef struct epoll_env_s
{
    struct epoll_event *event_list;
    int nevents;
}epoll_env_t;


io_multiplexing_action_t mlc_io_multiplexing_actions = {
    epoll_init,
    epoll_done,
    epoll_process,
    epoll_add_event,
    epoll_del_event,
    epoll_mod_event,
};

static int epoll_init(cycle_t *cycle)
{
    cycle_conf_t *conf = &cycle->conf;

    if (cycle->ep <= 0)
    {
        int ep = epoll_create(conf->connection_n);
        if (ep == -1)
        {
            log_fatal(NULL,"epoll_create() failed");
            return -1;
        }
        log_info(NULL,"epoll_created=%d", ep);
        cycle->ep = ep;
    }
    epoll_env_t *env = malloc(sizeof(epoll_env_t));
    memset(env,0,sizeof(*env));
    cycle->epoll_env = env;
    env->event_list = (struct epoll_event *)malloc(sizeof(struct epoll_event) * conf->connection_n);
    env->nevents = (int)conf->connection_n;
    return 0;
}

int epoll_process(cycle_t *cycle, int timer, unsigned flags)
{
    epoll_env_t *env = cycle->epoll_env;
    struct epoll_event *event_list = env->event_list;
    int nfds = epoll_wait(cycle->ep, event_list, env->nevents, timer);
    if (nfds == -1)
    {
        log_fatal(NULL,"cycle_step ret=%d", nfds);
        return nfds;
    }

    for (int i = 0; i < nfds; i++)
    {
        struct epoll_event *ee = event_list + i; 
        event_t *evt = ee->data.ptr;
        if (evt && evt->fd > 0) //maybe fd has been closed by other process
        {
            //log_info(NULL,"EPOLL fd=%d, ee->events=%d",evt->fd, ee->events);
            if (ee->events & EPOLLIN)
            {
                //log_info(NULL,"EPOLLIN fd=%d",evt->fd);
                evt->read_ready = 1;
            }
            if (ee->events & EPOLLOUT)
            {
                log_info(NULL,"EPOLLOUT fd=%d",evt->fd);
                evt->write_ready = 1;
            }
            assert(evt->fd>0);
            int ret = event_process(evt);
            if (ret < 0)
            {
                log_fatal(NULL,"fd[%d] process event error ret=%d", evt->fd, ret);
            }
            evt->read_ready = 0;
            evt->write_ready = 0;
        }
    }
    return nfds;
}

int epoll_mod_event(cycle_t *cycle, event_t *evt, unsigned flags)
{
    if (evt->fd <= 0)
    {
        log_fatal(NULL,"err fd=%d",evt->fd);
        return -1;
    }
    evt->flags = flags;
    struct epoll_event ev;
    ev.events = EPOLLET;
    if (flags & MLC_EVENT_READ)
    {
        ev.events |= EPOLLIN;
    }
    if (flags & MLC_EVENT_WRITE)
    {
        ev.events |= EPOLLOUT;
    }
    ev.data.fd = evt->fd;
    ev.data.ptr = evt;
    //log_info(NULL,"epoll_mod_event");
    int ret = epoll_ctl(cycle->ep, EPOLL_CTL_MOD, evt->fd, &ev);
    // log_info(NULL,"epoll add fd[%d] ", evt->fd);
    if (ret < 0)
    {
        log_fatal(NULL,"epoll add fd[%d] error", evt->fd);
    }
    return ret;

}

int epoll_add_event(cycle_t *cycle, event_t *evt, unsigned flags)
{
    if (evt->fd <= 0)
    {
        log_fatal(NULL,"err fd=%d",evt->fd);
        return -1;
    }
    evt->flags = flags;

    struct epoll_event ev;
    ev.events = EPOLLET;
    if (flags & MLC_EVENT_READ)
    {
        ev.events |= EPOLLIN;
    }
    if (flags & MLC_EVENT_WRITE)
    {
        ev.events |= EPOLLOUT;
    }
    ev.data.fd = evt->fd;
    ev.data.ptr = evt;
    int ret = epoll_ctl(cycle->ep, EPOLL_CTL_ADD, evt->fd, &ev);
    // log_info(NULL,"epoll add fd[%d] ", evt->fd);
    if (ret < 0)
    {
        log_fatal(NULL,"epoll add fd[%d] error", evt->fd);
    }
    return ret;
}

int epoll_del_event(cycle_t *cycle, event_t *evt)
{
    if (evt->fd <= 0)
    {
        return -1;
    }

    struct epoll_event ev;
    int ret = epoll_ctl(cycle->ep, EPOLL_CTL_DEL, evt->fd, &ev);
    // log_info(NULL,"epoll del fd[%d] ", evt->fd);
    if (ret < 0)
    {
        log_fatal(NULL,"epoll del fd[%d] error", evt->fd);
    }
    return ret;
}

#endif

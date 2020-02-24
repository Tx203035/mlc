#include "event.h"
#include "cycle.h"
#include "core/core.h"
#include "core/log.h"

#ifdef MLC_USE_KQUEUE
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

static int kqueue_init(cycle_t *cycle);
static int kqueue_process(cycle_t *cycle,int timer,unsigned flags);
static int kqueue_add_event(cycle_t *cycle, event_t *ev, unsigned flags);
static int kqueue_mod_event(cycle_t *cycle, event_t *ev, unsigned flags);
static int kqueue_del_event(cycle_t *cycle, event_t *ev);
static int kqueue_done(cycle_t *cycle)
{
    return 0;
}

typedef struct kevent_env_s
{
    int nevents;
    struct kevent *event_list;
}kevent_env_t;


io_multiplexing_action_t mlc_io_multiplexing_actions = {
    kqueue_init,
    kqueue_done,
    kqueue_process,
    kqueue_add_event,
    kqueue_del_event,
    kqueue_mod_event
};

static struct kevent *kqueue_create_event_list(int count)
{
    size_t len = count * sizeof(struct kevent);
    struct kevent *ret = malloc(len);
    assert(ret);
    memset(ret, 0, len);
    return ret;
}

int kqueue_init(cycle_t *cycle)
{
    cycle_conf_t *conf = &cycle->conf;

    if (cycle->ep <= 0)
    {
        int ep = kqueue();
        if (ep == -1)
        {
            log_fatal(NULL,"failed");
            return -1;
        }
        log_info(NULL,"kqueue_created=%d", ep);
        cycle->ep = ep;
    }
    if (!cycle->kevent_env)
    {
        kevent_env_t *env = malloc(sizeof(kevent_env_t));
        memset(env,0,sizeof(*env));
        cycle->kevent_env = env;
        env->event_list = kqueue_create_event_list(conf->connection_n);
        env->nevents = conf->connection_n;
    }
    
    return 0;
}

int kqueue_process(cycle_t *cycle,int timer,unsigned int flags)
{
    struct timespec ts,*tp;
    if (timer >= 0) {
        /* code */
        ts.tv_sec = timer / 1000;
        ts.tv_nsec = timer % 1000 * 1000000;
        tp = &ts;
    }
    else
    {
        tp = NULL;
    }
    kevent_env_t *env = cycle->kevent_env;
    struct kevent *event_list = env->event_list;

    int events = kevent(cycle->ep, NULL, 0, event_list, (int)env->nevents, tp);
    if (events == MLC_EINTR)
    {
        /* code */
        log_fatal(NULL,"kevent int");
        return 0;
    }
    if (events < 0) {
        log_fatal(NULL,"kevent ret=%d",events);
        return events;
    }

    for(int i = 0; i < events; i++)
    {
        struct kevent *kqev = &event_list[i];
        event_t *ev = (event_t *)kqev->udata;
        unsigned long instance = (unsigned long)ev & 1;
        ev = (event_t *)((unsigned long)ev & (unsigned long)~1);

        if (ev->instance != instance) {
            log_info(NULL,"kevent process stail event %p",ev);
            continue;
        }
        if(ev->fd<=0){
            continue;
        }

        switch (kqev->filter)
        {
        case EVFILT_READ:
            // log_info(NULL,"KEVENT READ fd=%d",ev->fd);
            ev->read_ready = 1;
            break;
        case EVFILT_WRITE:
            // log_info(NULL,"KEVENT WRITE fd=%d",ev->fd);
            ev->write_ready = 1;
            break;
        default:
            break;
        }
        int ret = event_process(ev);
        if (ret >= 0) {
            if (ev->oneshot) {
                ev->active = 0;
            }
        }
        ev->read_ready = 0;
        ev->write_ready = 0;
    }

    return 0;
}



int kqueue_set_event(cycle_t *cycle, event_t *ev,unsigned flags,short kevent_flags)
{
    struct kevent change_list[2];
    struct timespec ts = {0,0};
    struct kevent *change;

    change = &change_list[0];
    short read_flags = kevent_flags;
    read_flags |= (flags & MLC_EVENT_READ) ? EV_ENABLE : EV_DISABLE;
    EV_SET(change, ev->fd, EVFILT_READ, read_flags, 0, 0, ev);

    change = &change_list[1];
    short write_flags = kevent_flags;
    write_flags |= (flags & MLC_EVENT_WRITE) ? EV_ENABLE : EV_DISABLE;
    EV_SET(change, ev->fd, EVFILT_WRITE, write_flags,0, 0, ev);
    
    int ret = kevent(cycle->ep, change_list, 2, NULL, 0, &ts);
    if (ret < 0) {
        log_fatal(NULL,"kevent add err ret=%d",ret);
        return ret;
    }
    return 0;
}

int kqueue_add_event(cycle_t *cycle,event_t *ev,unsigned flags)
{
    int ret = kqueue_set_event(cycle, ev, flags, EV_ADD | EV_CLEAR);
    if (ret >=0) {
        ev->flags = flags;
        ev->active = 1;
    }
    return ret;
}

int kqueue_mod_event(cycle_t *cycle,event_t *ev,unsigned flags)
{
    int ret = kqueue_set_event(cycle, ev, flags, EV_ADD | EV_CLEAR);
    if (ret >=0) {
        ev->flags = flags;
        ev->active = 1;
    }
    return ret;
}


int kqueue_del_event(cycle_t *cycle,event_t *ev)
{
    ev->active = 0;
    return kqueue_set_event(cycle,ev,ev->flags,EV_DELETE);
}

#endif



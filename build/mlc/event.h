#ifndef __EVENT_H__
#define __EVENT_H__

#include "mlc.h"

enum
{
    MLC_EVENT_READ = 1,
    MLC_EVENT_WRITE = 1 << 1,
    MLC_EVENT_ERR = 1 << 2,
    MLC_EVENT_READWRITE = MLC_EVENT_READ | MLC_EVENT_WRITE,
    
};

struct io_multiplexing_action_s
{
    int (*init)(cycle_t *cycle);
    int (*done)(cycle_t *cycle);
    int (*process_events)(cycle_t *cycle, int timer, unsigned int flags);
    int (*add)(cycle_t *cycle,event_t *ev, unsigned int flags);
    int (*del)(cycle_t *cycle,event_t *ev);
    int (*mod)(cycle_t *cycle, event_t *ev, unsigned int flags);
    // int (*enable)(event_t *ev, int event, unsigned int flags);
    // int (*disable)(event_t *ev, int event, unsigned int flags);
    // int (*add_conn)(connection_t *c);
    // int (*del_conn)(connection_t *c, unsigned int flags);
};

typedef struct io_multiplexing_action_s io_multiplexing_action_t;
extern io_multiplexing_action_t mlc_io_multiplexing_actions;

#define mlc_io_mult_process mlc_io_multiplexing_actions.process_events
#define mlc_io_mult_add_event mlc_io_multiplexing_actions.add
#define mlc_io_mult_del_event mlc_io_multiplexing_actions.del
#define mlc_io_mult_init mlc_io_multiplexing_actions.init
#define mlc_io_mult_done mlc_io_multiplexing_actions.done
#define mlc_io_mult_mod mlc_io_multiplexing_actions.mod



typedef int (*event_handler_pt)(void *data);

struct event_s
{
    void *data;
    int fd;
    unsigned flags;
    event_handler_pt handler_read;
    event_handler_pt handler_write;

    unsigned read_ready : 1;
    unsigned write_ready : 1;


    unsigned write : 1;
    unsigned read : 1;

    unsigned accept : 1;

    /* used to detect the stale events in kqueue and epoll */
    unsigned instance : 1;

    /*
     * the event was passed or would be passed to a kernel;
     * in aio mode - operation was posted.
     */
    unsigned active : 1;

    unsigned disabled : 1;

    /* the ready event; in aio mode 0 means that no operation can be posted */
    unsigned ready : 1;

    unsigned oneshot : 1;

    /* aio operation is complete */
    unsigned complete : 1;

    unsigned eof : 1;
    unsigned error : 1;

    unsigned timedout : 1;
    unsigned timer_set : 1;

    unsigned delayed : 1;

    unsigned deferred_accept : 1;

    /* the pending eof reported by kqueue, epoll or in aio chain operation */
    unsigned pending_eof : 1;

    unsigned posted : 1;

    unsigned closed : 1;

    /* to test on worker exit */
    unsigned channel : 1;
    unsigned resolver : 1;

    unsigned cancelable : 1;

    unsigned index;
};

int event_process(event_t *evt);
event_t *create_event(void *data, event_handler_pt handler_read, event_handler_pt handler_write);
int event_reset(event_t* evt);

#endif
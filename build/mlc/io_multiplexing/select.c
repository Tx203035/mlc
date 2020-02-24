#include "event.h"
#include "cycle.h"
#include "core/core.h"
#include "core/log.h"

#ifdef MLC_USE_SELECT
static int select_init(cycle_t *cycle);
static int select_done(cycle_t *cycle) { return 0; }
static int select_process(cycle_t *cycle, int timer, unsigned flags);
static int select_add_event(cycle_t *cycle, event_t *ev, unsigned flags);
static int select_mod_event(cycle_t *cycle, event_t *ev, unsigned flags);
static int select_del_event(cycle_t *cycle, event_t *ev);

struct fd_event
{
    int fd;
    event_t *ev;
	uint8_t write_able;
	uint8_t read_able;
};

typedef struct select_env_s
{
    fd_set rset;
    fd_set wset;

    struct fd_event *event_list;
    int event_max;
    int event_cnt;
    int maxfd;
}select_env_t;


static struct fd_event *get_fd_event(select_env_t *env,int fd)
{
    for (int i = 0; i < env->event_cnt; i++)
    {
        if (env->event_list[i].fd == fd)
        {
            return &env->event_list[i];
        }
    }
    return NULL;
}

static struct fd_event *add_fd_event(select_env_t *env,int fd)
{
    if (env->event_cnt >= env->event_max)
    {
        return NULL;
    }
    return &env->event_list[env->event_cnt++];
}

static void del_fd_event(select_env_t *env,int fd)
{
    for (int i = 0; i < env->event_cnt; i++)
    {
        if (env->event_list[i].fd == fd)
        {
            env->event_list[i] = env->event_list[env->event_cnt--];
        }
    }
}

io_multiplexing_action_t mlc_io_multiplexing_actions =
{
    select_init,
    select_done,
    select_process,
    select_add_event,
    select_del_event,
    select_mod_event,
};

static int select_init(cycle_t *cycle)
{
    assert(cycle->select_env == NULL);
    select_env_t *env = malloc(sizeof(select_env_t));
    memset(env,0,sizeof(*env));
    cycle->select_env = env;
    FD_ZERO(&env->rset);
    FD_ZERO(&env->wset);

    cycle->ep = 1;

    if (!env->event_list)
    {
        cycle_conf_t *conf = &cycle->conf;
		uint32_t total_size = sizeof(struct fd_event) * conf->connection_n;
        env->event_list = (struct fd_event *)malloc(total_size);
        env->event_max = conf->connection_n;
		memset(env->event_list, 0, total_size);
    }

    return 0;
}

int select_process(cycle_t *cycle, int timer, unsigned flags)
{
    struct timeval tv;
    tv.tv_sec = timer / 1000;
    tv.tv_usec = (timer % 1000) * 1000;

    select_env_t *env = cycle->select_env;

    fd_set trset, twset;
    FD_ZERO(&trset);
    FD_ZERO(&twset);
    trset = env->rset;
    twset = env->wset;

    int nrfds = select(env->maxfd, &trset, NULL, NULL, &tv);
	int nwfds = select(env->maxfd, NULL, &twset, NULL, NULL);
    if (nrfds > 0 || nwfds > 0)
    {
        for (int i = 0; i < env->event_cnt; i++)
        {
            struct fd_event *fdevt = &env->event_list[i];
            event_t *evt = fdevt->ev;
            int fd = fdevt->fd;
            if(fd<=0)
            {
                log_info(NULL,"evt has been del");
                continue;   
            }

            if ( (evt->flags & MLC_EVENT_READ) && FD_ISSET(fd, &trset))
            {
                if(!fdevt->read_able)
                {
                    evt->read_ready = 1;
                }
                else
                {
                    //ET ignore
                }
            }
            else
            {
                fdevt->read_able = 0;
            }
            

            if ((evt->flags & MLC_EVENT_WRITE) && FD_ISSET(fd, &twset))
            {
				if (!fdevt->write_able)
				{
					evt->write_ready = 1;
				}
				else
				{
					//ET ignore
				}
            }
			else
			{
				fdevt->write_able = 0;
			}

            int ret = event_process(evt);
            if (ret < 0)
            {
                log_fatal(NULL,"process event error ret=%d", ret);
            }

            evt->read_ready = 0;
            evt->write_ready = 0;
        }
    }
	else
	{
		Sleep(timer);
	}

    return 0;
}
int select_mod_event(cycle_t *cycle, event_t *evt, unsigned flags)
{
    select_env_t *env = cycle->select_env;
    struct fd_event *fd_evt = get_fd_event(env,evt->fd);
    assert(fd_evt);
    assert(fd_evt->fd = evt->fd);
    if (fd_evt == NULL)
    {
        return 0;
    }
    evt->flags = flags;
	fd_evt->write_able = 0;
    fd_evt->read_able = 0;
    evt->read_ready = 0;
    evt->write_ready = 0;;
    return 0;
}
int select_add_event(cycle_t *cycle, event_t *evt, unsigned flags)
{
    select_env_t *env = cycle->select_env;
    struct fd_event *fd_evt = get_fd_event(env,evt->fd);
    if (fd_evt == NULL)
    {
        fd_evt = add_fd_event(env,evt->fd);
        if (fd_evt == NULL)
        {
            return -1;
        }
    }
	evt->flags = flags;
    fd_evt->fd = evt->fd;
    fd_evt->ev = evt;
	fd_evt->write_able = 0;
    fd_evt->read_able = 0;
    evt->read_ready = 0;
    evt->write_ready = 0;

    if (evt->fd + 1 > env->maxfd)
    {
        env->maxfd = evt->fd + 1;
    }

    // if (flags & MLC_EVENT_READ)
    {
        FD_SET(evt->fd, &env->rset);
    }

    // if (flags & MLC_EVENT_WRITE)
    {
        FD_SET(evt->fd, &env->wset);
    }
	return 0;
}

int select_del_event(cycle_t *cycle, event_t *ev)
{
    select_env_t *env = cycle->select_env;
    del_fd_event(env,ev->fd);
    FD_CLR(ev->fd, &env->rset);
    FD_CLR(ev->fd, &env->wset);
    return 0;
}

#endif
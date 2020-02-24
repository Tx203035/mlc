#include "debugger.h"
#include "cycle.h"
#include "event.h"
#include "string.h"
#include "core/log.h"
#include "core/chain.h"
#include "core/socket_util.h"
#include "fec.h"
#include "kcp.h"
#include "session.h"
#include "connection.h"
#include "core/encoding.h"

static int debugger_cycle(debugger_cmd_t *cmd,cycle_t *cycle, chain_t *chain);
static int debugger_root_filter(debugger_cmd_t *cmd, cycle_t *cycle, chain_t *chain);
static event_t *g_debugger_event = NULL;
static chain_t *g_debugger_chain = NULL;
static chain_t *g_debugger_chain_record = NULL;



debugger_cmd_t g_debugger_cycle = {
    "cycle",
    debugger_cycle,
    {NULL,}
};


debugger_cmd_t g_debugger_root = {
    "",
    debugger_root_filter,
    {&g_debugger_cycle,}
};


static inline int str_startswith(const char *str, const char *pre)
{
    return strncmp(pre, str, strlen(pre)) == 0;
}

static inline void trim_nouse_char_in_chain(chain_t *chain)
{
    char *p = chain->data_start;
    for (;;++p)
    {
        char c = *p;
        if(c != ' ' && c != '\t')
            break;
    }
    //chain->data_start = p;
}
static int str_ends_with(const char *str,char e)
{
    size_t len = strlen(str);
    return len>0 && str[len-1] == e;
}

int debugger_execute(debugger_cmd_t *cmd, cycle_t *cycle, chain_t *chain)
{
    if (!str_startswith(chain->data_start, cmd->cmd))
    {
        return -1;
    }
    //chain->data_start = (char *)chain->data_start + strlen(cmd->cmd);
    if (cmd->handler)
    {
        trim_nouse_char_in_chain(chain);
        int ret = cmd->handler(cmd, cycle, chain);
        if (ret < 0)
        {
            return ret;
        }
    }
    for (size_t i = 0; i < MLC_MAX_DEBUGGER_SUB_CMD; i++)
    {
        debugger_cmd_t *sub = cmd->sub[i];
        if (sub) {
            int ret = debugger_execute(sub,cycle,chain);
            if (ret>=0) {
                break;
            }
        }
        else
        {
            break;
        }
        
    }
    return 0;
}

int on_debugger_input(cycle_t *cycle)
{
    if (g_debugger_chain == NULL)
    {
        g_debugger_chain =
            chain_alloc(cycle->pool, 0, MLC_PKG_MAX_SIZE, mlc_chain_mask_debug);
        memset(g_debugger_chain->data_start, 0, g_debugger_chain->capacity);
    }
    int ret;
    chain_t *c = g_debugger_chain;
    int leftlen = c->capacity - c->len;
    char *p = c->data_start + c->len;
eintr: 
    ret = read(STDIN_FILENO, p, leftlen);
    log_info(NULL,"debugger recv len=%d", ret);
    if (ret < 0)
    {
        int err = mlc_errno;
        if (err == MLC_EINTR)
        {
            log_info(NULL,"interrupt");
            goto eintr;
        }
        else 
        {
            return ret;
        }
    }
    c->len += ret;

    ret = debugger_execute(&g_debugger_root, cycle ,c);
    if (ret==0)
    {
        chain_release(g_debugger_chain,0);
        g_debugger_chain = NULL;
    }
    return 0;
}


int create_debugger(cycle_t *cycle)
{
    assert(cycle->ep);
    g_debugger_event = create_event(cycle, (event_handler_pt)on_debugger_input, NULL);
    g_debugger_event->active = 1;
    assert(g_debugger_event);
    g_debugger_event->fd = STDIN_FILENO;
    mlc_io_mult_add_event(cycle,g_debugger_event,MLC_EVENT_READ);
    return 0;
}

int debugger_root_filter(debugger_cmd_t *cmd, cycle_t *cycle, chain_t *chain)
{
    if (!str_ends_with(chain->data_start, '\n'))
    {
        return 1;
    }
    return 0;
}

int debugger_cycle(debugger_cmd_t *cmd, cycle_t *cycle, chain_t *chain)
{
    log_info(NULL,"%s", chain->data_start);
    return 0;
}

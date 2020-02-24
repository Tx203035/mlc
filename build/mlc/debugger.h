#ifndef _MLC_DEBUGER_H_H
#define _MLC_DEBUGER_H_H

#include "mlc.h"

#define MLC_MAX_DEBUGGER_LEN 1024
#define MLC_MAX_DEBUGGER_SUB_CMD 100

typedef int (*debugger_handler)(debugger_cmd_t *cmd,cycle_t *cycle, chain_t *chain);



struct debugger_cmd_s
{
    const char *cmd;
    debugger_handler handler;
    debugger_cmd_t *sub[MLC_MAX_DEBUGGER_SUB_CMD];
    event_t *evt;
};

int create_debugger(cycle_t *cycle);


#endif
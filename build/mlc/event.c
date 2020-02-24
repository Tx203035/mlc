#include "event.h"
#include "core/log.h"

event_t *create_event(void *data, event_handler_pt handler_read, event_handler_pt handler_write)
{
    event_t *evt = malloc(sizeof(event_t));
    if (evt==NULL)
    {
        return NULL;
    }
    assert(evt);
    memset(evt, 0, sizeof(event_t));
    evt->data = data;
    evt->handler_read = handler_read;
    evt->handler_write = handler_write;
    return evt;
}

int event_reset(event_t *evt)
{
    evt->active = 0;
    evt->handler_read = NULL;
    evt->handler_write = NULL;
    return 0;
}


int event_process(event_t *evt)
{
    // log_info(NULL,"readready:%d,writeready:%d",evt->read_ready,evt->write_ready);
    int ret = 0;
    if (evt->read_ready && evt->handler_read)
    {
        ret = evt->handler_read(evt->data);
        if (ret < 0 && ret != MLC_AGAIN)
        {
            //log_fatal(NULL,"handler read event error ret=%d", ret);
            return ret;
        }
    }

    if (evt->write_ready && evt->handler_write)
    {
        ret = evt->handler_write(evt->data);
        if (ret < 0 && ret != MLC_AGAIN)
        {
            //log_fatal(NULL,"handler write event error ret=%d", ret);
            return ret;
        }
    }
    return 0;
}
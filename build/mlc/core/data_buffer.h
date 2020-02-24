#ifndef _MLC_DATA_BUFFER_H_
#define _MLC_DATA_BUFFER_H_

#include "core.h"

typedef struct data_large_s {
    struct data_large_s *next;
    void *alloc;
    uint8_t data[0];
} data_large_t;

typedef struct data_node_s {
    data_buffer_t *next;
    uint8_t *last;
    uint8_t *end;
    uint16_t failed;
} data_node_t;

struct data_buffer_s {
    logger_t *log;
    struct data_buffer_s *current;
    data_large_t *large;
    data_node_t d;
    uint32_t max;
};

data_buffer_t *data_buffer_create(uint32_t size, logger_t *log);
int data_buffer_destroy(data_buffer_t *buf);
int data_buffer_reset(data_buffer_t *buf);
void *data_buffer_alloc(data_buffer_t *buf, uint32_t size);

#endif
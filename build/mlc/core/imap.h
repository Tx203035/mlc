/** 
 * Copyright (c) 2014 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _MLC_I_MAP_H
#define _MLC_I_MAP_H

#include <string.h>
#include "core.h"

#define MAP_VERSION "0.1.0"

struct imap_node_t;
typedef struct imap_node_t imap_node_t;
typedef int32_t imap_key_t;

struct imap_node_t {
  imap_key_t hash;
  imap_node_t *next;
  void *value;
};

typedef struct {
  imap_node_t **buckets;
  unsigned nbuckets, nnodes;
  mlc_pool_t *pool;
} imap_base_t;

typedef struct {
  unsigned bucketidx;
  imap_node_t *node;
} imap_iter_t;


// only support ptr type
#define imap_t(T)\
  struct { imap_base_t base; /*T *ref; T tmp;*/ }


#define imap_init(m,p)\
  memset(m, 0, sizeof(*(m))); (m)->base.pool = p


#define imap_deinit(m)\
  imap_deinit_(&(m)->base)


#define imap_get(m, key)\
  ( imap_get_(&(m)->base, key) )


#define imap_set(m, key, value)\
  ( imap_set_(&(m)->base, key, value) )


#define imap_remove(m, key)\
  imap_remove_(&(m)->base, key)


#define imap_iter(m)\
  imap_iter_()


#define imap_next(m, iter)\
  imap_next_(&(m)->base, iter)

#define imap_set_size(m,nbuckets)\
  imap_set_size_(&(m)->base,nbuckets)

void imap_deinit_(imap_base_t *m);
void *imap_get_(imap_base_t *m, imap_key_t key);
int imap_set_(imap_base_t *m, imap_key_t key, void *value);
void imap_remove_(imap_base_t *m, imap_key_t key);
imap_iter_t imap_iter_(void);
void *imap_next_(imap_base_t *m, imap_iter_t *iter);
int imap_set_size_(imap_base_t *m, int nbuckets);


// typedef imap_t(void*) imap_void_t;
// typedef imap_t(char*) imap_str_t;
// typedef imap_t(int) imap_int_t;

#endif

/** 
 * Copyright (c) 2014 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdlib.h>
#include <string.h>
#include "imap.h"
#include "pool.h"




static imap_node_t *imap_newnode(imap_base_t *m, imap_key_t key, void *value) {
  imap_node_t *node;
  node = mlc_palloc(m->pool,sizeof(*node));
  if (!node) return NULL;
  node->hash = key;
  node->value = value;
  return node;
}


static inline int imap_bucketidx(imap_base_t *m, imap_key_t hash) {
  /* If the implementation is changed to allow a non-power-of-2 bucket count,
   * the line below should be changed to use mod instead of AND */
  return hash & (m->nbuckets - 1);
}


static void imap_addnode(imap_base_t *m, imap_node_t *node) {
  int n = imap_bucketidx(m, node->hash);
  node->next = m->buckets[n];
  m->buckets[n] = node;
}


static int imap_resize(imap_base_t *m, int nbuckets) {
  imap_node_t *nodes, *node, *next;
  imap_node_t **buckets;
  int i; 
  /* Chain all nodes together */
  nodes = NULL;
  i = m->nbuckets;
  while (i--) {
    node = (m->buckets)[i];
    while (node) {
      next = node->next;
      node->next = nodes;
      nodes = node;
      node = next;
    }
  }
  /* Reset buckets */
  buckets = realloc(m->buckets, sizeof(*m->buckets) * nbuckets);
  if (buckets != NULL) {
    m->buckets = buckets;
    m->nbuckets = nbuckets;
  }
  if (m->buckets) {
    memset(m->buckets, 0, sizeof(*m->buckets) * m->nbuckets);
    /* Re-add nodes to buckets */
    node = nodes;
    while (node) {
      next = node->next;
      imap_addnode(m, node);
      node = next;
    }
  }
  /* Return error code if realloc() failed */
  return (buckets == NULL) ? -1 : 0;
}


static imap_node_t **imap_getref(imap_base_t *m, imap_key_t key) {
  imap_key_t hash = key;
  imap_node_t **next;
  if (m->nbuckets > 0) {
    next = &m->buckets[imap_bucketidx(m, hash)];
    while (*next) {
      if ((*next)->hash == hash) {
        return next;
      }
      next = &(*next)->next;
    }
  }
  return NULL;
}


void imap_deinit_(imap_base_t *m) {
  imap_node_t *next, *node;
  int i;
  i = m->nbuckets;
  while (i--) {
    node = m->buckets[i];
    while (node) {
      next = node->next;
      mlc_pfree(m->pool,node);
      node = next;
    }
  }
  free(m->buckets);
}


void *imap_get_(imap_base_t *m, imap_key_t key) {
  imap_node_t **next = imap_getref(m, key);
  return next ? (*next)->value : NULL;
}


int imap_set_(imap_base_t *m, imap_key_t key, void *value) {
  int n, err;
  imap_node_t **next, *node;
  /* Find & replace existing node */
  next = imap_getref(m, key);
  if (next) {
    (*next)->value = value;
    return 1;
  }
  /* Add new node */
  node = imap_newnode(m, key, value);
  if (node == NULL) goto fail;
  if (m->nnodes >= m->nbuckets) {
    n = (m->nbuckets > 0) ? (m->nbuckets << 1) : 1;
    err = imap_resize(m, n);
    if (err) goto fail;
  }
  imap_addnode(m, node);
  m->nnodes++;
  return 0;
  fail:
  if (node) mlc_pfree(m->pool, node);
  return -1;
}


void imap_remove_(imap_base_t *m, imap_key_t key) {
  imap_node_t *node;
  imap_node_t **next = imap_getref(m, key);
  if (next) {
    node = *next;
    *next = (*next)->next;
    mlc_pfree(m->pool,node);
    m->nnodes--;
  }
}


imap_iter_t imap_iter_(void) {
  imap_iter_t iter;
  iter.bucketidx = -1;
  iter.node = NULL;
  return iter;
}


void *imap_next_(imap_base_t *m, imap_iter_t *iter) {
  if (iter->node) {
    iter->node = iter->node->next;
    if (iter->node == NULL) goto nextBucket;
  } else {
    nextBucket:
    do {
      if (++iter->bucketidx >= m->nbuckets) {
        return NULL;
      }
      iter->node = m->buckets[iter->bucketidx];
    } while (iter->node == NULL);
  }
  return iter->node->value;
}

int imap_set_size_(imap_base_t *m, int nbuckets) {
  int n;
  for(n = 1; n<nbuckets; n = n<<1){}
  return imap_resize(m,n);
}
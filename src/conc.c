#include <stdlib.h>
#include "queue.h"
#include <omp.h>

// node definition
typedef struct node {
  value_t value;
  struct node *next;
} node;

// queue definition
typedef struct queue {
  node *head;
  node *tail;
  node **freelists;
  int max_threads;
  omp_lock_t lock;
} queue;

// create queue
queue* create() {
  queue *q = (queue*)malloc(sizeof(queue));
  if (!q) { return NULL; }  // buy more RAM
  return q;
}

// initialize queue
int init(queue *q) {
  q->max_threads = omp_get_max_threads();
  q->freelists = (node**)calloc(q->max_threads, sizeof(node*));
  if (q->freelists == NULL) { return QUEUE_NOMEM; }  // buy more RAM
  node *n = (node*)malloc(sizeof(node));
  if (n == NULL) {
    free(q->freelists);
    return QUEUE_NOMEM;
  }  // buy more RAM
  n->next = NULL;
  q->head = n;
  q->tail = n;
  omp_init_lock(&q->lock);
  return QUEUE_OK;
}

// enqueue in queue
int enq(value_t v, queue *q) {
  omp_set_lock(&q->lock);
  int id = omp_get_thread_num();
  node *n;
  if (q->freelists[id] == NULL) {
    n = (node*)malloc(sizeof(node));
    if (n == NULL) {  // buy more RAM
      omp_unset_lock(&q->lock);
      return QUEUE_NOMEM;
    }
  } else {
    n = q->freelists[id];
    q->freelists[id] = n->next;
  }
  n->next = NULL;
  n->value = v;
  q->tail->next = n;
  q->tail = n;
  omp_unset_lock(&q->lock);
  return QUEUE_OK;
}

// enqueue in queue (including statistics)
int enq_stats(value_t v, queue *q, stats *s) {
  omp_set_lock(&q->lock);
  int id = omp_get_thread_num();
  node *n;
  if (q->freelists[id] == NULL) {
    n = (node*)malloc(sizeof(node));
    if (n == NULL) {  // buy more RAM
      omp_unset_lock(&q->lock);
      return QUEUE_NOMEM;
    }
  } else {
    n = q->freelists[id];
    q->freelists[id] = n->next;
    s->freelist_len--;
  }
  n->next = NULL;
  n->value = v;
  q->tail->next = n;
  q->tail = n;
  omp_unset_lock(&q->lock);
  return QUEUE_OK;
}

// dequeue from queue
int deq(value_t *v, queue *q) {
  omp_set_lock(&q->lock);
  int id = omp_get_thread_num();
  node *old;
  node *new;
  old = q->head;
  new = old->next;
  if (new == NULL) {
    omp_unset_lock(&q->lock);
    return QUEUE_EMPTY;
  }
  *v = new->value;
  q->head = new;
  old->next = q->freelists[id];
  q->freelists[id] = old;
  omp_unset_lock(&q->lock);
  return QUEUE_OK;
}

// dequeue from queue (including statistics)
int deq_stats(value_t *v, queue *q, stats *s) {
  omp_set_lock(&q->lock);
  int id = omp_get_thread_num();
  node *old;
  node *new;
  old = q->head;
  new = old->next;
  if (new == NULL) {
    omp_unset_lock(&q->lock);
    return QUEUE_EMPTY;
  }
  *v = new->value;
  q->head = new;
  old->next = q->freelists[id];
  q->freelists[id] = old;
  s->freelist_len++;
  if (s->freelist_len > s->freelist_max) {
    s->freelist_max = s->freelist_len;
  }
  s->freelist_insert++;
  omp_unset_lock(&q->lock);
  return QUEUE_OK;
}

// length of queue
int len(queue *q) {
  node *n = q->head;
  n = n->next;
  int c = 0;
  while (n != NULL) {
    c++;
    n = n->next;
  }
  return c;
}

// destroy queue
void destroy(queue *q) {
  node *n = q->head;
  while (n != NULL) {
    node *next = n->next;
    free(n);
    n = next;
  }

  for (int i = 0; i < q->max_threads; i++) {
    n = q->freelists[i];
    while (n != NULL) {
      node *next = n->next;
      free(n);
      n = next;
    }
  }
  free(q->freelists);
  omp_destroy_lock(&q->lock);
  free(q);
}


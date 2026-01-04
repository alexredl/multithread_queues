#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>
#include "queue.h"
#include <omp.h>

#define CAS atomic_compare_exchange_weak // weak|strong

// stamped node pointer
typedef uint64_t snode_ptr;

// stamp
typedef uint16_t stamp_t;

// node definition
typedef struct node {
  value_t value;
  _Atomic(snode_ptr) snext;
} node;

// stamp node
static snode_ptr stamp(node *n, stamp_t stamp) {
  return ((snode_ptr)stamp << 48) | ((snode_ptr)n & 0x0000FFFFFFFFFFFF);
}

// get stamp from stamped node
static stamp_t get_stamp(snode_ptr sn) {
  return (stamp_t)(sn >> 48);
}

// get node from stamped node
static node *get_node(snode_ptr sn) {
  return (node*)(sn & 0x0000FFFFFFFFFFFF);
}

// queue definition
typedef struct queue {
  _Atomic(snode_ptr) head;
  _Atomic(snode_ptr) tail;
  _Atomic(snode_ptr) *freelists;
  int max_threads;
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
  q->freelists = (_Atomic(snode_ptr)*)malloc(sizeof(_Atomic(snode_ptr)) * q->max_threads);
  if (q->freelists == NULL) { return QUEUE_NOMEM; }  // buy more RAM
  for (int i = 0; i < q->max_threads; i++) {
    atomic_store(&q->freelists[i], stamp(NULL, 0));
  }
  node *n = (node*)malloc(sizeof(node));
  if (n == NULL) {
    destroy(q);
    return QUEUE_NOMEM;
  }  // buy more RAM
  atomic_store(&n->snext, stamp(NULL, 0));
  atomic_store(&q->head, stamp(n, 0));
  atomic_store(&q->tail, stamp(n, 0));
  return QUEUE_OK;
}

// enqueue in queue
int enq(value_t v, queue *q) {
  int id = omp_get_thread_num();
  snode_ptr sn = atomic_load(&q->freelists[id]);
  node *n = get_node(sn);
  if (n == NULL) {
    n = (node*)malloc(sizeof(node));
    if (n == NULL) { return QUEUE_NOMEM; }  // buy more RAM
  } else {
    atomic_store(&q->freelists[id], atomic_load(&n->snext));
  }
  n->value = v;
  atomic_store(&n->snext, stamp(NULL, 0));

  while(1) {
    snode_ptr stail = atomic_load(&q->tail);
    node *tail = get_node(stail);
    snode_ptr snext = atomic_load(&tail->snext);
    node *next = get_node(snext);

    if (next == NULL) {
      if (CAS(&tail->snext, &snext, stamp(n, get_stamp(snext) + 1))) {
        CAS(&q->tail, &stail, stamp(n, get_stamp(stail) + 1));
        return QUEUE_OK;
      }
    } else {
      CAS(&q->tail, &stail, stamp(next, get_stamp(stail) + 1));
    }
  }
}

// enqueue in queue (including statistics)
int enq_stats(value_t v, queue *q, stats *s) {
  int id = omp_get_thread_num();
  snode_ptr sn = atomic_load(&q->freelists[id]);
  node *n = get_node(sn);
  if (n == NULL) {
    n = (node*)malloc(sizeof(node));
    if (n == NULL) { return QUEUE_NOMEM; }  // buy more RAM
  } else {
    s->freelist_len--;
    atomic_store(&q->freelists[id], atomic_load(&n->snext));
  }
  n->value = v;
  atomic_store(&n->snext, stamp(NULL, 0));

  while(1) {
    snode_ptr stail = atomic_load(&q->tail);
    node *tail = get_node(stail);
    snode_ptr snext = atomic_load(&tail->snext);
    node *next = get_node(snext);

    if (next == NULL) {
      if (CAS(&tail->snext, &snext, stamp(n, get_stamp(snext) + 1))) {
        s->cas_succ++;
        if (CAS(&q->tail, &stail, stamp(n, get_stamp(stail) + 1))) { s->cas_succ++; } else { s->cas_fail++; }
        return QUEUE_OK;
      } else { s->cas_fail++; }
    } else {
      if (CAS(&q->tail, &stail, stamp(next, get_stamp(stail) + 1))) { s->cas_succ++; } else { s->cas_fail++; }
    }
  }
}

// dequeue from queue
int deq(value_t *v, queue *q) {
  int id = omp_get_thread_num();
  while(1) {
    snode_ptr shead = atomic_load(&q->head);
    node *head = get_node(shead);
    snode_ptr stail = atomic_load(&q->tail);
    node *tail = get_node(stail);
    snode_ptr snext = atomic_load(&head->snext);
    node *next = get_node(snext);
    if (shead != atomic_load(&q->head) || stail != atomic_load(&q->tail)) { continue; }
    if (head == tail) {
      if (next == NULL) { return QUEUE_EMPTY; }
      CAS(&q->tail, &stail, stamp(next, get_stamp(stail) + 1));
    } else if (next != NULL) {
      *v = next->value;
      if (CAS(&q->head, &shead, stamp(next, get_stamp(shead) + 1))) {
        atomic_store(&head->snext, atomic_load(&q->freelists[id]));
        atomic_store(&q->freelists[id], shead);
        return QUEUE_OK;
      }
    }
  }
}

// dequeue in queue (including statistics)
int deq_stats(value_t *v, queue *q, stats *s) {
  int id = omp_get_thread_num();
  while(1) {
    snode_ptr shead = atomic_load(&q->head);
    node *head = get_node(shead);
    snode_ptr stail = atomic_load(&q->tail);
    node *tail = get_node(stail);
    snode_ptr snext = atomic_load(&head->snext);
    node *next = get_node(snext);
    if (shead != atomic_load(&q->head) || stail != atomic_load(&q->tail)) { continue; }
    if (head == tail) {
      if (next == NULL) { return QUEUE_EMPTY; }
      if (CAS(&q->tail, &stail, stamp(next, get_stamp(stail) + 1))) { s->cas_succ++; } else { s->cas_fail++; }
    } else if (next != NULL) {
      *v = next->value;
      if (CAS(&q->head, &shead, stamp(next, get_stamp(shead) + 1))) {
        s->cas_succ++;
        atomic_store(&head->snext, atomic_load(&q->freelists[id]));
        atomic_store(&q->freelists[id], shead);
        s->freelist_len++;
        if (s->freelist_len > s->freelist_max) {
          s->freelist_max = s->freelist_len;
        }
        s->freelist_insert++;
        return QUEUE_OK;
      } else { s->cas_fail++; }
    }
  }
}

// length of queue
int len(queue *q) {
  node *n = get_node(atomic_load(&q->head));
  n = get_node(atomic_load(&n->snext));
  int c = 0;
  while (n != NULL) {
    c++;
    n = get_node(atomic_load(&n->snext));
  }
  return c;
}

// destroy queue
void destroy(queue *q) {
  node *n = get_node(atomic_load(&q->head));
  while (n != NULL) {
    node *next = get_node(atomic_load(&n->snext));
    free(n);
    n = next;
  }

  for (int i = 0; i < q->max_threads; i++) {
    n = get_node(atomic_load(&q->freelists[i]));
    while (n != NULL) {
      node *next = get_node(atomic_load(&n->snext));
      free(n);
      n = next;
    }
  }
  free(q->freelists);
  free(q);
}


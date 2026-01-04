#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>

typedef int value_t;

// node definition
typedef struct node node;

// queue definition
typedef struct queue queue;

// statistics definition
typedef struct {
  double duration;

  long enq_succ;
  long enq_fail;
  long deq_succ;
  long deq_fail;

  long freelist_insert;
  long freelist_len;
  long freelist_max;

  long cas_succ;
  long cas_fail;
} stats;

// combine different statistics to one
static stats comb_stats(stats *ss, int len) {
  stats s = {0};
  for (int i = 0; i < len; i++) {
    s.duration += ss[i].duration;
    s.enq_succ += ss[i].enq_succ;
    s.enq_fail += ss[i].enq_fail;
    s.deq_succ += ss[i].deq_succ;
    s.deq_fail += ss[i].deq_fail;
    s.freelist_insert += ss[i].freelist_insert;
    if (ss[i].freelist_max > s.freelist_max) {
      s.freelist_max = ss[i].freelist_max;
    }
    s.cas_succ += ss[i].cas_succ;
    s.cas_fail += ss[i].cas_fail;
  }
  s.duration /= len;
  return s;
}

// printing of statistics
static void print_stats(stats *s) {
  printf("STATS:\n");
  printf(" duration: %f sec\n", s->duration);
  printf(" enq_succ: %ld\n", s->enq_succ);
  printf(" enq_fail: %ld\n", s->enq_fail);
  printf(" deq_succ: %ld\n", s->deq_succ);
  printf(" deq_fail: %ld\n", s->deq_fail);
  printf(" freelist_insert: %ld\n", s->freelist_insert);
  printf(" freelist_max: %ld\n", s->freelist_max);
  printf(" cas_succ: %ld\n", s->cas_succ);
  printf(" cas_fail: %ld\n", s->cas_fail);
}

// queue return codes
#define QUEUE_OK    0
#define QUEUE_EMPTY 1
#define QUEUE_NOMEM 2

// explain quque return codes
static const char* q_error(int code) {
  switch(code) {
    case QUEUE_OK:    return "Successful";
    case QUEUE_EMPTY: return "Queue empty";
    case QUEUE_NOMEM: return "Out of memory";
    default:          return "Unknown";
  }
}

// create queue
queue* create();

// initialize queue
int init(queue *q);

// enqueue in queue
int enq(value_t v, queue *q);

// enqueue in queue (including statistics)
int enq_stats(value_t v, queue *q, stats *s);

// dequeue from queue
int deq(value_t *v, queue *q);

// dequeue from queue (including statistics)
int deq_stats(value_t *v, queue *q, stats *s);

// length of queue
int len(queue *q);

// destroy queue
void destroy(queue *q);

#endif


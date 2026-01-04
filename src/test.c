#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include "queue.h"
#include <omp.h>

// test sequential implementaion
int test_seq(const int N) {
  printf("Doing sequential tests...\n");

  queue *q = create();
  value_t v;

  int ret = init(q);
  if (ret != QUEUE_OK) {
    printf(" ERROR on init(): %s\n", q_error(ret));
    destroy(q);
    return 1;
  }

  for (int i = 0; i < N; i++) {
    ret = enq((value_t)i, q);
    if (ret != QUEUE_OK) {
      printf(" ERROR on enq(%d): %s\n", i, q_error(ret));
      destroy(q);
      return 1;
    }
  }

  if (len(q) != N) {
    printf(" ERROR: queue length should be %d (!= %d)\n", N, len(q));
    destroy(q);
    return 1;
  }

  printf(" Enqueue test passed\n");

  for (int i = 0; i < N; i++) {
    ret = deq(&v, q);
    if (ret != QUEUE_OK) {
      printf(" ERROR on enq(%d): %s\n", i, q_error(ret));
      destroy(q);
      return 1;
    }
    if (v != (value_t)i) {
      printf(" ERROR: enq(%d) and deq(%d) do not match\n", i, v);
      return 1;
    }
  }

  if (len(q) != 0) {
    printf(" ERROR: queue length should be 0 (!= %d)\n", len(q));
    destroy(q);
    return 1;
  }

  printf(" Dequeue test passed\n");

  for (int i = 0; i < N; i++) {
    ret = deq(&v, q);
    if (ret != QUEUE_EMPTY) {
      printf(" ERROR on enq(%d): %s\n", i, q_error(ret));
      destroy(q);
      return 1;
    }
  }

  if (len(q) != 0) {
    printf(" ERROR: queue length should be 0 (!= %d)\n", len(q));
    destroy(q);
    return 1;
  }

  printf(" Empty dequeue test passed\n");

  for (int i = 0; i < N; i++) {
    ret = enq((value_t)i, q);
    if (ret != QUEUE_OK) {
      printf(" ERROR on enq(%d): %s\n", i, q_error(ret));
      destroy(q);
      return 1;
    }
    ret = deq(&v, q);
    if (ret != QUEUE_OK) {
      printf(" ERROR on deq(): %s\n", q_error(ret));
      destroy(q);
      return 1;
    }
    if (v != (value_t)i) {
      printf(" ERROR: enq(%d) and deq(%d) do not match\n", i, v);
      destroy(q);
      return 1;
    }
  }

  if (len(q) != 0) {
    printf(" ERROR: queue length should be 0 (!= %d)\n", len(q));
    destroy(q);
    return 1;
  }

  printf(" Enqueue-Dequeue test passed\n");

  ret = deq(&v, q);
  if (ret != QUEUE_EMPTY) {
    printf(" ERROR: deq() should return QUEUE_EMPTY, returned %s\n", q_error(ret));
    destroy(q);
    return 1;
  }

  destroy(q);

  printf(" All sequential tests passed\n");
  return 0;
}

// test concurrent implementaion
int test_conc(const int N) {
  printf("Doing concurrent tests...\n");

  queue *q = create();
  value_t v;

  int ret = init(q);
  if (ret != QUEUE_OK) {
    printf(" ERROR on init(): %s\n", q_error(ret));
    destroy(q);
    return 1;
  }

  #pragma omp parallel for
  for (int i = 0; i < N; i++) {
    int r = enq((value_t)i, q);
    if (r != QUEUE_OK) {
      #pragma omp critical
      printf(" ERROR on enq(): %s\n", q_error(r));
    }
  }

  if (len(q) != N) {
    printf(" ERROR: queue length should be %d (!= %d)\n", N, len(q));
    destroy(q);
    return 1;
  }

  printf(" Enqueue test passed\n");

  for (int i = 0; i < N; i++) {
    value_t v;
    int r = deq(&v, q);
    if (r != QUEUE_OK) {
      printf(" ERROR on deq(): %s\n", q_error(r));
    }
  }

  if (len(q) != 0) {
    printf(" ERROR: queue length should be 0 (!= %d)\n", len(q));
    destroy(q);
    return 1;
  }

  printf(" Sequential dequeue after parallel enqueue test passed\n");

  #pragma omp parallel for
  for (int i = 0; i < N; i++) {
    int r = deq(&v, q);
    if (r != QUEUE_EMPTY) {
      #pragma omp critical
      printf(" ERROR on enq(%d): %s\n", i, q_error(r));
    }
  }

  if (len(q) != 0) {
    printf(" ERROR: queue length should be 0 (!= %d)\n", len(q));
    destroy(q);
    return 1;
  }

  printf(" Empty dequeue test passed\n");

  for (int i = 0; i < N; i++) {
    int r = enq((value_t)i, q);
    if (r != QUEUE_OK) {
      printf(" ERROR on enq(): %s\n", q_error(r));
    }
  }

  if (len(q) != N) {
    printf(" ERROR: queue length should be %d (!= %d)\n", N, len(q));
    destroy(q);
    return 1;
  }

  #pragma omp parallel for
  for (int i = 0; i < N; i++) {
    value_t v;
    int r = deq(&v, q);
    if (r != QUEUE_OK) {
      #pragma omp critical
      printf(" ERROR on deq(): %s\n", q_error(r));
    }
  }

  if (len(q) != 0) {
    printf(" ERROR: queue length should be 0 (!= %d)\n", len(q));
    destroy(q);
    return 1;
  }

  while (deq(&v, q) != QUEUE_EMPTY) {}

  printf(" Parallel dequeue after sequential enqueue test passed\n");

  #pragma omp parallel for
  for (int i = 0; i < N; i++) {
    int r = enq((value_t)i, q);
    if (r != QUEUE_OK) {
      #pragma omp critical
      printf(" ERROR on enq(): %s\n", q_error(r));
    }
  }

  if (len(q) != N) {
    printf(" ERROR: queue length should be %d (!= %d)\n", N, len(q));
    destroy(q);
    return 1;
  }

  #pragma omp parallel for
  for (int i = 0; i < N; i++) {
    value_t v;
    int r = deq(&v, q);
    if (r != QUEUE_OK) {
      #pragma omp critical
      printf(" ERROR on deq(): %s\n", q_error(r));
    }
  }

  if (len(q) != 0) {
    printf(" WARNING: queue length should be 0 (!= %d)\n", len(q));
    destroy(q);
    return 1;
  }

  while (deq(&v, q) != QUEUE_EMPTY) {}

  printf(" Parallel dequeue after parallel enqueue test passed\n");

  #pragma omp parallel for
  for (int i = 0; i < N; i++) {
    int r = enq((value_t)i, q);
    if (r != QUEUE_OK) {
      #pragma omp critical
      printf(" ERROR on enq(): %s\n", q_error(r));
    }
  }

  if (len(q) != N) {
    printf(" ERROR: queue length should be %d (!= %d)\n", N, len(q));
    destroy(q);
    return 1;
  }

  int *vs = calloc(N+1, sizeof(int));
  while((ret = deq(&v, q)) == QUEUE_OK) {
    if (v < 0 || v > N) {
      printf(" ERROR: deq(%d) out of range\n", v);
    } else {
      vs[v]++;
    }
  }

  for(int i = 0; i < N; i++) {
    if (vs[i] != 1) {
      printf(" ERROR: value %d occurs %d times\n", i, vs[i]);
      free(vs);
      destroy(q);
      return 1;
    }
  }
  free(vs);

  if (len(q) != 0) {
    printf(" ERROR: queue length should be 0 (!= %d)\n", len(q));
    destroy(q);
    return 1;
  }

  printf(" Sequential dequeue after parallel enqueue value check test passed\n");

  #pragma omp parallel for
  for (int i = 0; i < N; i++) {
    if (omp_get_thread_num() % 2) {
      int r = enq((value_t)i, q);
      if (r != QUEUE_OK) {
        #pragma omp critical
        printf(" ERROR on enq(): %s\n", q_error(r));
      }
    } else {
      value_t v;
      deq(&v, q);
    }
  }

  while (deq(&v, q) != QUEUE_EMPTY) {}

  if (len(q) != 0) {
    printf(" ERROR: queue length should be 0 (!= %d)\n", len(q));
    destroy(q);
    return 1;
  }

  printf(" Some enque, some deque parallel test passed\n");

  #pragma omp parallel for
  for (int i = 0; i < N; i++) {
    int r = enq((value_t)i, q);
    if (r != QUEUE_OK) {
      #pragma omp critical
      printf(" ERROR on enq(): %s\n", q_error(r));
    }

    value_t v;
    r = deq(&v, q);
    if (r != QUEUE_OK) {
      #pragma omp critical
      printf(" ERROR on deq(): %s\n", q_error(r));
    }
  }

  if (len(q) != 0) {
    printf(" WARNING: queue length should be 0 (!= %d)\n", len(q));
    destroy(q);
    return 1;
  }

  while (deq(&v, q) != QUEUE_EMPTY) {}

  printf(" Enqueue-Dequeue test passed\n");

  ret = deq(&v, q);
  if (ret != QUEUE_EMPTY) {
    printf(" ERROR: deq() should return QUEUE_EMPTY, returned %s\n", q_error(ret));
    destroy(q);
    return 1;
  }

  destroy(q);

  printf(" All concurrent tests passed\n");
  return 0;
}

// test sequential and concurrent implementaion and print some test/usage
int main(int argc, char** argv) {
  // decrease malloc arena count; otherwise nebula cannot do shit for > 50 threads
  mallopt(M_ARENA_MAX, 1);

  // get number of threads
  if (argc > 1) {
    int threads = atoi(argv[1]);
    int max_threads = omp_get_max_threads();
    if (max_threads < threads) {
      threads = max_threads;
      printf("Limiting threads to %d instead of requested %d\n", max_threads, threads);
    }
    omp_set_num_threads(threads);
  }
  printf("Running with %d threads\n", omp_get_max_threads());

  // example usage of queue
  printf("Doing example...\n");
  queue *q = create();
  value_t v;

  init(q);

  const int N = 10;

  printf(" Enqueuing: ");
  for (int i = 0; i < N; i++) {
    enq((value_t)i, q);
    printf("%d ", i);
  }
  printf("\n");

  printf(" Queue length: %d\n", len(q));

  printf(" Dequeuing: ");
  while(deq(&v, q) == 0) {
    printf("%d ", v);
  }
  printf("\n");

  printf(" Queue length: %d\n", len(q));

  destroy(q);

  // testing queue
  const int T = 1E6;
  printf("Testing with %d elements\n", T);
  test_seq(T);
  test_conc(T);

  return 0;
}


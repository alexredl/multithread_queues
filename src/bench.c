#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include "queue.h"
#include <omp.h>

// threaded worker with fixed number of enqueue and dequeue batches
void worker_fixed(queue *q, stats *s, int duration, int eb, int db) {
  double start = omp_get_wtime();
  value_t v;
  while (omp_get_wtime() - start < duration) {
    for (int i = 0; i < eb; i++) {
      if (enq_stats((value_t)i, q, s) == QUEUE_OK) {
        s->enq_succ++;
      } else {
        s->enq_fail++;
      }
    }
    for (int i = 0; i < db; i++) {
      if (deq_stats(&v, q, s) == QUEUE_OK) {
        s->deq_succ++;
      } else {
        s->deq_fail++;
      }
    }
  }
  s->duration = omp_get_wtime() - start;
}

// threaded worker with random number of enqueue and dequeue batches
void worker_rand(queue *q, stats *s, int duration, int eb_min, int eb_max, int db_min, int db_max) {
  double start = omp_get_wtime();
  unsigned int seed = (unsigned int)(omp_get_thread_num() * 100000);
  value_t v;
  while (omp_get_wtime() - start < duration) {
    int eb = eb_min + rand_r(&seed) % (eb_max - eb_min + 1);
    for (int i = 0; i < eb; i++) {
      if (enq_stats((value_t)i, q, s) == QUEUE_OK) {
        s->enq_succ++;
      } else {
        s->enq_fail++;
      }
    }
    int db = db_min + rand_r(&seed) % (db_max - db_min + 1);
    for (int i = 0; i < db; i++) {
      if (deq_stats(&v, q, s) == QUEUE_OK) {
        s->deq_succ++;
      } else {
        s->deq_fail++;
      }
    }
  }
  s->duration = omp_get_wtime() - start;
}

// run one (equal) experiment
int experiment_equal(int threads, int duration, int eb_min, int eb_max, int db_min, int db_max) {
  queue *q = create();
  init(q);

  stats *ss = (stats*)calloc(threads, sizeof(stats));
  if (ss == NULL) {
    printf("ERROR: Unable to allocate shit\n");
    destroy(q);
    return 1;
  }

  if (eb_min == eb_max && db_min == db_max) {
    #pragma omp parallel num_threads(threads)
    worker_fixed(q, &ss[omp_get_thread_num()], duration, eb_min, db_min);
  } else {
    #pragma omp parallel num_threads(threads)
    worker_rand(q, &ss[omp_get_thread_num()], duration, eb_min, eb_max, db_min, db_max);
  }

  for (int i = 0; i < threads; i++) {
    printf("Thread: %d ", i);
    print_stats(&ss[i]);
  }

  stats s = comb_stats(ss, threads);
  printf("\n");
  printf("Summary ");
  print_stats(&s);

  free(ss);
  destroy(q);

  return 0;
}

// run one (unequal) experiment
int experiment_unequal(int threads, int duration, int *Ebs, int *Dbs) {
  queue *q = create();
  init(q);

  stats *ss = (stats*)calloc(threads, sizeof(stats));
  if (ss == NULL) {
    printf("ERROR: Unable to allocate shit\n");
    destroy(q);
    return 1;
  }

  #pragma omp parallel num_threads(threads)
  {
    int id = omp_get_thread_num();
    worker_fixed(q, &ss[id], duration, Ebs[id], Dbs[id]);
  }

  for (int i = 0; i < threads; i++) {
    printf("Thread: %d ", i);
    print_stats(&ss[i]);
  }

  stats s = comb_stats(ss, threads);
  printf("\n");
  printf("Summary ");
  print_stats(&s);

  free(ss);
  destroy(q);

  return 0;
}

// run correctneess check
int check_correctness(int threads, int duration) {
  queue *q = create();
  init(q);

  int *enques = (int*)calloc(threads, sizeof(int));
  int *deques = (int*)calloc(threads, sizeof(int));

  #pragma omp parallel num_threads(threads)
  {
    int id = omp_get_thread_num();
    double start = omp_get_wtime();
    int i = 0;
    while (omp_get_wtime() - start < (duration * 0.5)) {
      enq((value_t)(i * threads + id), q);
      i++;
    }
    enques[id] = i;
  }

  #pragma omp parallel num_threads(threads)
  {
    int *deques_local = (int*)calloc(threads, sizeof(int));
    value_t v;
    while (deq(&v, q) != QUEUE_EMPTY) {
      deques_local[v % threads]++;
    }

    #pragma omp critical
    {
      for (int i = 0; i < threads; i++) {
        deques[i] += deques_local[i];
      }
    }

    free(deques_local);
  }

  int correct = 0;
  for (int i = 0; i < threads; i++) {
    if (enques[i] != deques[i]) {
      correct = 1;
    }
  }

  if (correct == 0) {
    printf("Correctness check passed\n");
  } else {
    printf("Correctness check not passed\n");
  }
  printf("\n");
  printf("Detailed output:\n");
  printf("thread: enqued ?= dequed\n");
  for (int i = 0; i < threads; i++) {
    if (enques[i] == deques[i]) {
      printf("%d: %d == %d\n", i, enques[i], deques[i]);
    } else {
      printf("%d: %d != %d  x\n", i, enques[i], deques[i]);
    }
  }

  free(enques);
  free(deques);
  destroy(q);

  return correct;
}

// start the experiment with user provided parameters
int main(int argc, char** argv) {
  // decrease malloc arena count; otherwise nebula cannot do shit for > 50 threads
  mallopt(M_ARENA_MAX, 1);

  int threads = omp_get_max_threads();
  int duration = 1;
  int repetition = 1;
  int correctness = 0;
  int help = 0;
  int eb_flag = 0;
  int eb_min = 10;
  int eb_max = 10;
  int db_flag = 0;
  int db_min = 10;
  int db_max = 10;
  char *Eb = NULL;
  char *Db = NULL;

  int opt;
  while((opt = getopt(argc, argv, "n:t:r:ce:d:E:D:h")) != -1) {
    switch(opt) {
      case 'n': threads = atoi(optarg); break;
      case 't': duration = atoi(optarg); break;
      case 'r': repetition = atoi(optarg); break;
      case 'c': correctness = 1; break;
      case 'h': help = 1; break;
      case 'e': {
        eb_flag = 1;
        char *comma = strchr(optarg, ',');
        if (comma) {
          sscanf(optarg, "%d,%d", &eb_min, &eb_max);
        } else {
          eb_min = eb_max = atoi(optarg);
        }
        break;
      }
      case 'd': {
        db_flag = 1;
        char *comma = strchr(optarg, ',');
        if (comma) {
          sscanf(optarg, "%d,%d", &db_min, &db_max);
        } else {
          db_min = db_max = atoi(optarg);
        }
        break;
      }
      case 'E': Eb = strdup(optarg); break;
      case 'D': Db = strdup(optarg); break;
      default: help = 1;
    }
  }

  if ((Eb != NULL && Db == NULL) || (Eb == NULL && Db != NULL)) {
    printf("ERROR: -E and -D flag must both be set or not set.\n");
    help = 1;
  } else if (Eb != NULL && (eb_flag != 0 || db_flag != 0)) {
    printf("ERROR: if -E or -D flag is set, -e or -d flag can not be set.\n");
    help = 1;
  }
  if (Eb == NULL) {
    if (eb_min > eb_max) {
      printf("ERROR: eb_min (%d) > eb_max(%d)\n", eb_min, eb_max);
      help = 1;
    }
    if (db_min > db_max) {
      printf("ERROR: db_min (%d) > db_max(%d)\n", db_min, db_max);
      help = 1;
    }
  }

  if (help == 1) {
    printf("Usage: \n");
    printf("%s:\n", argv[0]);
    
    return 0;
  }

  printf("INFO: Threads:     %d\n", threads);
  printf("INFO: Duration:    %d\n", duration);

  if (correctness == 1) {
    printf("INFO: Checking for correctness. Ignoring all flags except -n and -t\n");
    printf("\n");
    return check_correctness(threads, duration);
  }

  printf("INFO: Repetitions: %d\n", repetition);

  int *Ebs = NULL;
  int *Dbs = NULL;
  if (Eb != NULL) {
    Ebs = (int*)malloc(threads * sizeof(int));
    Dbs = (int*)malloc(threads * sizeof(int));
    if (Ebs == NULL || Dbs == NULL) {
      printf("ERROR: Unable to allocate shit\n");
      free(Ebs);
      free(Dbs);
      return 1;
    }

    int count = 0;
    char *token = strtok(Eb, ",");
    while (token && count < threads) {
      Ebs[count++] = atoi(token);
      token = strtok(NULL, ",");
    }
    if (count != threads || token != NULL) {
      printf("ERROR: expected exactly %d (= -n) values for -E\n", threads);
      free(Ebs);
      free(Dbs);
      return 1;
    }

    count = 0;
    token = strtok(Db, ",");
    while (token && count < threads) {
      Dbs[count++] = atoi(token);
      token = strtok(NULL, ",");
    }
    if (count != threads || token != NULL) {
      printf("ERROR: expected exactly %d (= -n) values for -D\n", threads);
      free(Ebs);
      free(Dbs);
      return 1;
    }

    printf("INFO: Enque batches: [");
    for (int i = 0; i < threads; i++) {
      printf("%d ", Ebs[i]);
    }
    printf("]\n");

    printf("INFO: Deque batches: [");
    for (int i = 0; i < threads; i++) {
      printf("%d ", Dbs[i]);
    }
    printf("]\n");

  } else {
    if (eb_min != eb_max) {
      printf("INFO: Enque batch: (%d, %d)\n", eb_min, eb_max);
    } else {
      printf("INFO: Enque batch: %d\n", eb_min);
    }
    if (db_min != db_max) {
      printf("INFO: Deque batch: (%d, %d)\n", db_min, db_max);
    } else {
      printf("INFO: Deque batch: %d\n", db_min);
    }
  }

  int ret_code = 0;
  printf("\n");
  for (int r = 0; r < repetition; r++) {
    if (Eb != NULL) {
      ret_code = experiment_unequal(threads, duration, Ebs, Dbs);
    } else {
      ret_code = experiment_equal(threads, duration, eb_min, eb_max, db_min, db_max);
    }
    if (ret_code != 0) { break; }
    printf("\n\n");
  }

  free(Ebs);
  free(Dbs);
  return ret_code;
}


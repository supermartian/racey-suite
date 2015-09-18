//
// A really simple test program.
// Printfs every so often.
//

#include <assert.h>
#include <memory.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "dmp.h"

static int ids[32];

void* run(void* tid)
{
  int k,i;
  int threadId = *(int *) tid;
  printf("%d: START\n", threadId);

  for (k = 0; k < 500; ++k)
    for (i = 0; i < 500000; ++i)
      if (i && k % 100 == 0 && i % 100000 == 0)
        printf("%d: %d\n", threadId, i);

  printf("%d: DONE\n", threadId);
  return NULL;
}

int main(int argc, char* argv[])
{
  int i,n,q;

  for (i = 0; i < 32; ++i)
    ids[i] = i;

  printf("INIT\n");
  n = (argc >= 2) ? atoi(argv[1]) : 2;
  q = (argc >= 3) ? atoi(argv[2]) : 500000;

  dmp_make_deterministic("SERIAL", 0);
  printf("RUN: qs=%d n=%d\n", q, n);

  for (i = 1; i < n; ++i) {
    pthread_t th;
    pthread_create(&th, NULL, run, &ids[i]);
  }
  run(&ids[0]);

  // Has no effect, but should write a printk.
  dmp_make_deterministic("SERIAL", 0);

  return 0;
}

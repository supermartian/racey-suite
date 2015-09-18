/*
 * RACEY: a program print a result which is very sensitive to the
 * ordering between processors (races).
 *
 * It is important to "align" the short parallel executions in the
 * simulated environment. First, a simple barrier is used to make sure
 * thread on different processors are starting at roughly the same time.
 * Second, each thread is bound to a physical cpu. Third, before the main
 * loop starts, each thread use a tight loop to gain the long time slice
 * from the OS scheduler.
 *
 * Author: Min Xu <mxu@cae.wisc.edu>
 * Main idea: Due to Mark Hill
 * Created: 09/20/02
 *
 * Compile (on Solaris for Simics) :
 *   cc -mt -o racey racey.c magic.o
 * (on linux with gcc)
 *   gcc -m32 -lpthread -o racey racey.c
 *
 * DMP CHANGES:
 * - PHASE_MARKER is removed
 * - ProcessorIds is removed
 * - MaxLoop is an optional command line parameter
 * - Can spawn 32 threads (previous max was 15)
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>

int MaxLoop = 50000;
#define MAX_ELEM 64
#define PAGE_SIZE (1 << 10)

#define PRIME1   103072243
#define PRIME2   103995407

int               NumProcs;
volatile int      startCounter;

/* shared variables */
unsigned sig[33] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                     16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
                     30, 31, 32 };
union {
  /* 64 bytes cache line */
  char b[64];
  int value;
} m[MAX_ELEM];


/* the mix function */
unsigned mix(unsigned i, unsigned j) {
  return (i + j * PRIME2) % PRIME1;
}

/* The function which is called once the thread is created */
void* ThreadBody(void* tid)
{
  int threadId = *(int *) tid;
  int i;

  /* seize the cpu, roughly 0.5-1 second on ironsides */
  for(i=0; i<0x07ffffff; i++) {};

  /*
   * main loop:
   *
   * Repeatedly using function "mix" to obtain two array indices, read two 
   * array elements, mix and store into the 2nd
   *
   * If mix() is good, any race (except read-read, which can tell by software)
   * should change the final value of mix
   */
  for(i = 0 ; i < MaxLoop; i++) {
    unsigned num = sig[threadId];
    unsigned index1 = num%MAX_ELEM;
    unsigned index2;
    num = mix(num, m[index1].value);
    index2 = num%MAX_ELEM;
    num = mix(num, m[index2].value);
    m[index2].value = num;
    sig[threadId] = num;
  }
  return NULL;
}

int
main(int argc, char* argv[])
{
  pthread_t*     threads;
  int*           tids;
  pthread_attr_t attr;
  int            ret;
  int            mix_sig, i;

  /* Parse arguments */
  if(argc < 2) {
    fprintf(stderr, "%s <numProcesors> <maxLoop>\n", argv[0]);
    exit(1);
  }
  NumProcs = atoi(argv[1]);
  assert(NumProcs > 0 && NumProcs <= 32);
  if (argc >= 3) {
    MaxLoop = atoi(argv[2]);
    assert(MaxLoop > 0);
  }

  /* Initialize the mix array */
  for(i = 0; i < MAX_ELEM; i++) {
    m[i].value = mix(i,i);
  }

  /* Initialize barrier counter */
  startCounter = NumProcs;

  /* Initialize array of thread structures */
  threads = (pthread_t *) malloc(sizeof(pthread_t) * NumProcs);
  assert(threads != NULL);
  tids = (int *) malloc(sizeof (int) * NumProcs);
  assert(tids != NULL);

  /* Initialize thread attribute */
  pthread_attr_init(&attr);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

  for(i=0; i < NumProcs; i++) {
    /* ************************************************************
     * pthread_create takes 4 parameters
     *  p1: threads(output)
     *  p2: thread attribute
     *  p3: start routine, where new thread begins
     *  p4: arguments to the thread
     * ************************************************************ */
    tids[i] = i+1;
    ret = pthread_create(&threads[i], &attr, ThreadBody, &tids[i]);
    assert(ret == 0);
  }

  /* Wait for each of the threads to terminate */
  for(i=0; i < NumProcs; i++) {
    ret = pthread_join(threads[i], NULL);
    assert(ret == 0);
  }

  /* compute the result */
  mix_sig = sig[0];
  for(i = 1; i < NumProcs ; i++) {
    mix_sig = mix(sig[i], mix_sig);
  }

  /* end of parallel phase */

  /* ************************************************************
   * print results
   *  1. mix_sig  : deterministic race?
   *  2. &mix_sig : deterministic stack layout?
   *  3. malloc   : deterministic heap layout?
   * ************************************************************ */
  printf("\n\nShort signature: %08x @ %p @ %p\n\n\n",
         mix_sig, &mix_sig, (void*)malloc(PAGE_SIZE/5));
  fflush(stdout);
  usleep(5);

  pthread_attr_destroy(&attr);

  return 0;
}


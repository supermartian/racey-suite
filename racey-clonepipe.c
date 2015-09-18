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
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

int MaxLoop = 50000;
#define MAX_ELEM 64
#define PAGE_SIZE (1 << 10)

#define PRIME1   103072243
#define PRIME2   103995407

#define RD 0
#define WR 1

int  NumProcs;
int  inputs[33][2];
int  output[33][2];

/* shared variables */
unsigned sig[33] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                     16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
                     30, 31, 32 };


/* the mix function */
unsigned mix(unsigned i, unsigned j) {
  return (i + j * PRIME2) % PRIME1;
}

void* ReaderThread(void* arg)
{
  const int threadId = *(int*)arg;
  char buffer[128];
  int* numbers = (int*)buffer;
  int num = sig[threadId];
  int i, k, r;

  printf("Reader %d start\n", threadId);
  /* seize the cpu, roughly 0.5-1 second on ironsides */
  //for (i=0; i<0x07ffffff; i++) {};
  printf("Reader %d go\n", threadId);

  /*
   * main loop:
   *
   * If mix() is good, any race (except read-read, which can tell by software)
   * should change the final value of mix
   */
  for (r = 1; r > 0; ) {
    r = read(inputs[threadId][RD], buffer, sizeof buffer);
    num = mix(num, r);
    if (r > 0) {
      for (k = 0; k < r / sizeof(*numbers); k++)
        num = mix(num, numbers[k]);
    }
  }

  /* return */
  write(output[threadId][WR], &num, sizeof(num));
  return NULL;
}

void* WriterThread(void* arg)
{
  const int threadId = *(int*)arg;
  int buffer[16];
  int num = sig[threadId];
  int i, k, r;

  printf("Writer %d start\n", threadId);
  /* seize the cpu, roughly 0.5-1 second on ironsides */
  //for (i=0; i<0x07ffffff; i++) {};
  printf("Writer %d go\n", threadId);

  /*
   * main loop:
   *
   * If mix() is good, any race (except read-read, which can tell by software)
   * should change the final value of mix
   */
  for (i = 0; i < MaxLoop; ++i) {
    for (k = 0; k < sizeof(buffer)/sizeof(*buffer); ++k) {
      num = mix(num, k * PRIME1);
      num = mix(num, PRIME2 / (k+1));
      buffer[k] = num;
    }
    const int target = (num % NumProcs) + 1;
    r = write(inputs[target][WR], buffer, sizeof buffer);
    num = mix(num, r);
  }

  return NULL;
}

int
main(int argc, char* argv[])
{
  int  mix_sig, i, r;
  int* tids;
  pthread_t* threads;

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

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

  tids = calloc(sizeof(int), NumProcs*2 + 1);
  threads = calloc(sizeof(threads), NumProcs*2 + 1);

  /* Open pipes */
  for(i=1; i <= NumProcs; i++) {
    r = pipe(inputs[i]);
    if (r < 0) {
      perror("pipe");
      return 1;
    }
    r = pipe(output[i]);
    if (r < 0) {
      perror("pipe");
      return 1;
    }
  }

  /* Spawn threads */
  printf("Spawn threads!\n");
  fflush(stdout);
  for(i=1; i <= NumProcs*2; i++) {
    tids[i] = (i+1)/2;
    printf("Spawning thread %d %d\n", i, tids[i]);
    fflush(stdout);
    if (i%2 == 1)
      r = pthread_create(&threads[i], &attr, ReaderThread, &tids[i]);
    else
      r = pthread_create(&threads[i], &attr, WriterThread, &tids[i]);
    printf("Spawned thread %d\n", i);
    fflush(stdout);
    assert(r == 0);
  }

  /* Wait for WriterThreads to terminate */
  for(i=1; i <= NumProcs*2; i++) {
    if (i%2 == 0) {
      printf("Waiting for join!\n");
      r = pthread_join(threads[i], NULL);
      assert(r == 0);
      printf("Joined thread %d\n", i);
    }
  }

  /* Wait for ReaderThreads to terminate */
  for (i=1; i <= NumProcs; ++i) {
    close(inputs[i][WR]);
  }

  for(i=1; i <= NumProcs*2; i++) {
    if (i%2 == 1) {
      r = pthread_join(threads[i], NULL);
      assert(r == 0);
    }
  }

  /* Compute the result */
  mix_sig = sig[0];
  for(i = 1; i < NumProcs ; i++) {
    int num = 0;
    r = read(output[i][RD], &num, sizeof(num));
    if (r < 0) {
      perror("read");
      return 1;
    }
    if (r == 0) {
      fprintf(stderr, "no output from thread %d\n", i);
      return 1;
    }
    mix_sig = mix(num, mix_sig);
    mix_sig = mix(r, mix_sig);
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

  return 0;
}


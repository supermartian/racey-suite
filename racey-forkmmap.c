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
#include <string.h>
#include <memory.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

int MaxLoop = 50000;
#define MAX_ELEM 64
#define PAGE_SIZE (1 << 10)

#define PRIME1   103072243
#define PRIME2   103995407

/* shared variables */
struct Shared {
  unsigned waiting;
  unsigned sig[33];

  union {
    /* 64 bytes cache line */
    char b[64];
    int value;
  } m[MAX_ELEM];
};

int               NumProcs;
struct Shared*    SHARED;

/* shared initialization */
const unsigned sig_init[33] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                                16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
                                30, 31, 32 };


/* the mix function */
unsigned mix(unsigned i, unsigned j) {
  return (i + j * PRIME2) % PRIME1;
}

/* The function which is called once the process is created */
void ChildProcess(int threadId)
{
  int i;

//  printf("CHILD SPAWN: %d\n", threadId);

  /* seize the cpu, roughly 0.5-1 second on ironsides */
  for(i=0; i<0x07ffffff; i++) {};

//  printf("CHILD ARRIVE: %d\n", threadId);

  /* simple barrier, pass only once */
//  if (__sync_sub_and_fetch(&SHARED->waiting, 1) > 0) {
//    while (SHARED->waiting > 0) {
//      sched_yield();
//    }
//  }

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
    unsigned num = SHARED->sig[threadId];
    unsigned index1 = num%MAX_ELEM;
    unsigned index2;
    num = mix(num, SHARED->m[index1].value);
    index2 = num%MAX_ELEM;
    num = mix(num, SHARED->m[index2].value);
    SHARED->m[index2].value = num;
    SHARED->sig[threadId] = num;
  }

//  printf("CHILD EXIT: %d\n", threadId);
}

int
main(int argc, char* argv[])
{
  int* pids;
  int  ret;
  int  mix_sig, i, k;

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

  pids = calloc(sizeof(int), NumProcs*2);

  /* Allocate the shared page */
  SHARED = mmap(NULL, 8*4096, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
  if (!SHARED || SHARED == MAP_FAILED) {
    perror("mmap");
    return 1;
  }
printf("SHARED (a): %p\n", SHARED);
printf("SHARED (z): %p\n", ((char*)SHARED) + 8*4096);

  SHARED->waiting = NumProcs;
  memcpy(SHARED->sig, sig_init, sizeof(SHARED->sig));
  for(i = 0; i < MAX_ELEM; i++) {
    SHARED->m[i].value = mix(i,i);
  }

  /* Spawn processes */
  for(i=1; i <= NumProcs; i++) {
    ret = fork();
    if (ret < 0) {
      perror("fork");
      return 1;
    }
    if (ret == 0) {
      ChildProcess(i);
      return 0;
    }
    pids[i-1] = ret;
  }

  /* compute the result */
  mix_sig = SHARED->sig[0];

  for(i=1; i <= NumProcs; i++) {
    ret = wait(NULL);
    for (k=0; k <= NumProcs; k++) {
      if (pids[k] == ret) {
//        printf("R: %d\n", k);
//        mix_sig = mix(k, mix_sig);
        break;
      }
    }
  }

  for(i = 1; i < NumProcs ; i++) {
    mix_sig = mix(SHARED->sig[i], mix_sig);
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


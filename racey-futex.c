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

#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
static inline int futex(volatile int* uaddr, int op, int val) {
  return syscall(SYS_futex, uaddr, op, val, NULL /* no timeout */, NULL, 0);
}

int MaxLoop = 50000;
#define MAX_ELEM 64
#define PAGE_SIZE (1 << 10)

#define PRIME1   103072243
#define PRIME2   103995407

int               NumProcs;
pthread_barrier_t barrier;
__thread int      threadId;

/* private variables (sig[i] private to thread i) */
unsigned sig[33] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                     16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
                     30, 31, 32 };

/* futex groups */
#define NFUTEX 4
struct FutexGroup {
  volatile int owner;        // threadId, or -1 for "all"
  volatile int round;        // scheduling round
  volatile int threads[33];  // set to 1 if the thread is present
  int begin, end;            // range of possibly live indexes in threads[]
} groups[NFUTEX];

/* shared variables */
union {
  char b[64];   /* 64 bytes cache line */
  int value;
} m[MAX_ELEM];

/* the mix function */
unsigned mix(unsigned i, unsigned j) {
  return (i + j * PRIME2) % PRIME1;
}

static int groupSize(struct FutexGroup* g) {
  int i, n = 0;
  for (i = 0; i < 33; ++i)
    if (g->threads[i])
      n++;
  return n;
}

static int groupLeader(struct FutexGroup* g) {
  int i;
  for (i = 0; i < 33; ++i)
    if (g->threads[i])
      return i;
  return 0;
}

static void groupRemoveMe(struct FutexGroup* g) {
  g->threads[threadId] = 0;
}

static int groupPickNext(struct FutexGroup* g) {
  int n;
  for (n = 0; n < 33; ++n) {
    if (g->threads[n]) {
      /* group is non-empty, so we can schedule */
      n = sig[threadId] % (g->end - g->begin + 1) + g->begin;
      while (n != g->end && g->threads[n] == 0)
        n = (n+1) % 33;
      /* n == end means schedule the whole group */
      return (n == g->end) ? -groupSize(g) : n;
    }
  }
  return 0;
}

/* The function which is called once the thread is created */
void* ThreadBody(void* tid)
{
  struct FutexGroup* g = NULL;
  const int npersig =
    MaxLoop >= 1000 ? MaxLoop / 1000 :
    MaxLoop >= 100  ? MaxLoop / 100  : 1;
  int i, k;

  /* seize the cpu, roughly 0.5-1 second on ironsides */
  for(i=0; i<0x07ffffff; i++) {};

  threadId = *(int*) tid;
  for(i=0; i<NFUTEX; ++i) {
    g = groups + i;
    if (g->threads[threadId])
      break;
  }
  assert(g);
  assert(g->threads[threadId]);

  /* simple barrier */
  pthread_barrier_wait(&barrier);

  /*
   * main loop:
   *
   * Repeatedly using function "mix" to obtain two array indices, read two 
   * array elements, mix and store into the 2nd
   *
   * If mix() is good, any race (except read-read, which can tell by software)
   * should change the final value of mix
   */
  for(i = 0; i < MaxLoop; ) {
    const int owner = g->owner;
    /*************************************************
     * My turn?
     */
    if (owner == threadId || owner < 0) {
      const int isLeader = (owner == threadId) || (groupLeader(g) == threadId);
      const int oldRound = g->round;
      /* EXECUTE */
      for (k = 0; k < npersig && i < MaxLoop; ++k, ++i) {
        unsigned num = sig[threadId];
        unsigned index1 = num%MAX_ELEM;
        unsigned index2;
        num = mix(num, m[index1].value);
        index2 = num%MAX_ELEM;
        num = mix(num, m[index2].value);
        m[index2].value = num;
        sig[threadId] = num;
      }
      if (i == MaxLoop) {
        groupRemoveMe(g);
      }
      /* SCHEDULE NEXT */
      if (owner < 0) {
        // execution barrier
        __sync_fetch_and_add(&g->owner, 1);
        while (g->owner != 0 && g->round == oldRound)
          ;
        // leader gets to schedule
        if (isLeader) {
          g->owner = groupPickNext(g);
          g->round++;
          futex(&g->round, FUTEX_WAKE, 33);
        } else {
          while (futex(&g->round, FUTEX_WAIT, oldRound) == EINTR)
            ;
        }
      } else {
        // always wake everyone
        g->owner = groupPickNext(g);
        g->round++;
        futex(&g->owner, FUTEX_WAKE, 33);
      }
    }
    /*************************************************
     * Wait for my turn!
     */
    else {
      futex(&g->owner, FUTEX_WAIT, owner);
      sig[threadId]++;  // muck with this each time we wake
    }
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
  int            mix_sig, i, k;

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

  /* Initialize array of thread structures */
  threads = (pthread_t *) malloc(sizeof(pthread_t) * NumProcs);
  assert(threads != NULL);
  tids = (int *) malloc(sizeof (int) * NumProcs);
  assert(tids != NULL);

  /* Initialize thread attribute */
  pthread_attr_init(&attr);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

  ret = pthread_barrier_init(&barrier, NULL, NumProcs);
  assert(ret == 0);

  /* Initialize futex groups */
  k = 1;
  for(i=0; i < NFUTEX; i++) {
    int end = (NumProcs < NFUTEX) ? k + NumProcs : k + NumProcs/NFUTEX;
    int size = 0;
    groups[i].begin = k;
    for(; k < end && k <= NumProcs; ++k) {
      ++size;
      groups[i].threads[k] = 1;
    }
    groups[i].end = k;
    groups[i].owner = -size;
    groups[i].round = 0;
  }

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
  pthread_barrier_destroy(&barrier);

  return 0;
}


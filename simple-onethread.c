//
// A really simple test program.
// No printfs and no syscalls after becoming
// deterministic, until exit().
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

static int var;

#define INCR "incl %0\n\t"

#define INCR_5   INCR    INCR    INCR    INCR    INCR
#define INCR_25  INCR_5  INCR_5  INCR_5  INCR_5  INCR_5
#define INCR_100 INCR_25 INCR_25 INCR_25 INCR_25

#define INCR_1000 INCR_100 INCR_100 INCR_100 INCR_100 INCR_100 \
                  INCR_100 INCR_100 INCR_100 INCR_100 INCR_100

#define INCR_10000 INCR_1000 INCR_1000 INCR_1000 INCR_1000 INCR_1000 \
                   INCR_1000 INCR_1000 INCR_1000 INCR_1000 INCR_1000

int main(int argc, char* argv[])
{
  if(argc < 3) {
    fprintf(stderr, "%s <quantumSize> <maxLoop>\n", argv[0]);
    exit(1);
  }

  const int Quantum = atoi(argv[1]);
  const int MaxLoop = atoi(argv[2]);
  printf("Running with Quantum=%d (ignored), MaxLoop=%d\n", Quantum, MaxLoop);

  // Deterministic section.
  dmp_make_deterministic("SERIAL", 0);

  asm volatile ("1:\t"
                INCR_10000
                "decl %1\n\t"
                "jne 1b\n\t"
                : /* no outputs */
                : "m" (var), "r" (MaxLoop)
                : "%rcx", "memory");

  // Has no effect, but should write a printk.
  dmp_make_deterministic("SERIAL", 0);
  return 0;
}

# racey-suite

## Introduction

This repository contains **racey** benchmark and its variants for different situation.

[Racey](http://pages.cs.wisc.edu/~markhill/racey.html) is originally developed by Mark D. Hill and Min Xu.

Other variants of racey are developed along with [Deterministic MultiProcessing (DMP)](http://sampa.cs.washington.edu/research/dmp.html) by [Sampa](http://sampa.cs.washington.edu/) group of University of Washington.

## Benchmark Description

### racey-basic.c

Other unit tests are based on this program, from:
http://pages.cs.wisc.edu/~markhill/racey.html

### racey-freqsyscall.c

Makes frequent system calls (to sys_getuid).

### racey-nobarrier.c

Doesn't use a barrier to sync the start of all threads.

### racey-guarded.c

Uses locks to guard the shared data in m[]; each lock
guards a range of m[].  Technically there are no data
races, but there is a race on the order in which locks
are acquired.

### racey-futex.c

Threads are divided into scheduling groups.  Each round,
each group decides which threads will be scheduled to run
in the next round (we select one thread or all threads).
The scheduler uses futexes to put threads to sleep when
it's not their turn and wake them when it might be.

### racey-signal.c

Each thread periodically sends SIGUSR1 to another thread,
chosen pseudo-randomly.  The signal handler mixes m[],
like the main code in ThreadBody, but uses a different
salt value when mixing.

### racey-readfile.c

Each thread reads small chunks from a large file (should
be about 1MB) and computes a local hash.  Output is a hash
of the local hashes.

### racey-forkpipe.c

Spawns processes with fork(), instead of threads.  Processes
communicate over pipes, where msgs are computed semi-randomly
using mix().  Output is sent back to the main process using
a pipe.

### test.pl

Run many unit tests.  See ./test.pl --help for usage.

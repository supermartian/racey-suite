#!/bin/bash

if [ "$*" == "" ]; then
  echo "Usage: $0 <bench1> <bench2> ..."
  exit 1
fi
if [ ! -x "./test.pl" ]; then
  echo "Run from the test/ directory"
  exit 1
fi

for p in "2" "8" "16"; do
  ./test.pl "$@" -n 100 -p $p -q   1003 --loops    10000 || exit 1
  ./test.pl "$@" -n 100 -p $p -q  10003 --loops    50000 || exit 1
  ./test.pl "$@" -n 100 -p $p -q  10003 --loops   100000 || exit 1
  ./test.pl "$@" -n 100 -p $p -q  50003 --loops    50000 || exit 1
  ./test.pl "$@" -n 100 -p $p -q  50003 --loops   100000 || exit 1
  ./test.pl "$@" -n 100 -p $p -q  50003 --loops   500000 || exit 1
  ./test.pl "$@" -n 100 -p $p -q  50003 --loops  1000000 || exit 1
  ./test.pl "$@" -n 100 -p $p -q  50003 --loops  5000000 || exit 1
  ./test.pl "$@" -n 100 -p $p -q 100003 --loops   500000 || exit 1
  ./test.pl "$@" -n 100 -p $p -q 100003 --loops  1000000 || exit 1
  ./test.pl "$@" -n 100 -p $p -q 100003 --loops  5000000 || exit 1
  ./test.pl "$@" -n 100 -p $p -q 100003 --loops 10000000 || exit 1
done

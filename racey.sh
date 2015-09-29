#! /bin/sh
#
# racey.sh
# Copyright (C) 2015 wen <wen@Saturn>
#
# Distributed under terms of the MIT license.
#


for i in `seq 1 2000`;
do
	/home/wen/Projects/racey-suite/racey-forkpipe 32 50 | grep signature >> ~/racey_out
	echo "rep $i"
done

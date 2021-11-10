#!/usr/bin/bash

FILE1=$(mktemp --tmpdir file1.XXXXXX)
FILE2=$(mktemp --tmpdir file2.XXXXXX)

cleanup()
{
	rm -f $FILE1 $FILE2
}

trap cleanup EXIT

echo 'Generating test files ...'
dd if=/dev/urandom of=$FILE1 bs=8M count=128

bsz=$((1*1024))
fsz=$((1024*1024*1024))
id1=$(./fgen);

set -x
# write
sleep 0.5; ./c0cp $id1 $FILE1 $bsz -b -pf -c1 $1
# write (async)
sleep 0.5; ./c0cp $id1 $FILE1 $bsz -b -pf -c1 -a8 $1
# read
sleep 0.5; ./c0cat $id1 $FILE2 $bsz $fsz -b -p -c1 $1
# delete
sleep 0.5; ./c0rm $id1 -y $1
set +x

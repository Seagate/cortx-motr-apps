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
id2=$(./fgen);
id3=$(./fgen);

set -x
# write
sleep 0.5; ./c0cp $id1 $FILE1 $bsz -b -px1 -c1
sleep 0.5; ./c0cp $id2 $FILE1 $bsz -b -px2 -c1
sleep 0.5; ./c0cp $id3 $FILE1 $bsz -b -px3 -c1
# write (async)
sleep 0.5; ./c0cp $id1 $FILE1 $bsz -b -pfx1 -c1 -a8
sleep 0.5; ./c0cp $id2 $FILE1 $bsz -b -pfx2 -c1 -a8
sleep 0.5; ./c0cp $id3 $FILE1 $bsz -b -pfx3 -c1 -a8
# read
sleep 0.5; ./c0cat $id1 $FILE2 $bsz $fsz -b -p -c1
sleep 0.5; ./c0cat $id2 $FILE2 $bsz $fsz -b -p -c1
sleep 0.5; ./c0cat $id3 $FILE2 $bsz $fsz -b -p -c1
# delete
sleep 0.5; ./c0rm $id1 -y 
sleep 0.5; ./c0rm $id2 -y 
sleep 0.5; ./c0rm $id3 -y 
set +x

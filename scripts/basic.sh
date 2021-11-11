#!/usr/bin/bash

FILE1=$(mktemp --tmpdir file1.XXXXXX)
FILE2=$(mktemp --tmpdir file2.XXXXXX)

cleanup()
{
	rm -f $FILE1 $FILE2
	( set -x; sleep 0.5; ./c0rm $id1 -y $1 )	
}

trap cleanup EXIT

echo 'Generating test files ...'
dd if=/dev/urandom of=$FILE1 bs=8M count=4
ls -lh $FILE1

bsz=$((1*64))
fsz=$(wc -c < "$FILE1")
id1=$(./fgen);

set -x
# write
sleep 0.5; ./c0cp $id1 $FILE1 $bsz -b -pf -mc1 $1
# write (async)
sleep 0.5; ./c0cp $id1 $FILE1 $bsz -b -pf -mc1 -a8 $1
# read
sleep 0.5; ./c0cat $id1 $FILE2 $bsz $fsz -b -p -mc1 $1
# compare
diff $FILE1 $FILE2
set +x

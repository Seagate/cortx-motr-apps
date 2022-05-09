#!/usr/bin/bash

FILE1=$(mktemp --tmpdir file1.XXXXXX)
FILE2=$(mktemp --tmpdir file2.XXXXXX)

cleanup()
{
	rm -f "$FILE1" "$FILE2"
}

trap cleanup EXIT

echo 'Generating test files ...'
dd if=/dev/urandom of="$FILE1" bs=8M count=128

bsz=$((1*1024))
fsz=$((1024*1024*1024))
read -r id1h id1l <<< $(./fgen);
read -r id2h id2l <<< $(./fgen);
read -r id3h id3l <<< $(./fgen);

set -x
# write
sleep 0.5; ./c0cp "$id1h" "$id1l" "$FILE1" "$bsz" -b -px1 -c1
sleep 0.5; ./c0cp "$id2h" "$id2l" "$FILE1" "$bsz" -b -px2 -c1
sleep 0.5; ./c0cp "$id3h" "$id3l" "$FILE1" "$bsz" -b -px3 -c1
# write (async)
sleep 0.5; ./c0cp "$id1h" "$id1l" "$FILE1" "$bsz" -b -pfx1 -c1 -a8
sleep 0.5; ./c0cp "$id2h" "$id2l" "$FILE1" "$bsz" -b -pfx2 -c1 -a8
sleep 0.5; ./c0cp "$id3h" "$id3l" "$FILE1" "$bsz" -b -pfx3 -c1 -a8
# read
sleep 0.5; ./c0cat "$id1h" "$id1l" "$FILE2" "$bsz" "$fsz" -b -p -c1
sleep 0.5; ./c0cat "$id2h" "$id2l" "$FILE2" "$bsz" "$fsz" -b -p -c1
sleep 0.5; ./c0cat "$id3h" "$id3l" "$FILE2" "$bsz" "$fsz" -b -p -c1
# delete
sleep 0.5; ./c0rm "$id1h" "$id1l" -y 
sleep 0.5; ./c0rm "$id2h" "$id2l" -y 
sleep 0.5; ./c0rm "$id3h" "$id3l" -y 
set +x

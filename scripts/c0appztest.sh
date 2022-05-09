#!/usr/bin/bash

TEST_FILE_256MB=$(mktemp --tmpdir 256MB.XXXXXX)
TEST_FILE_1GB=$(mktemp --tmpdir 1GB.XXXXXX)
TEST_FILE_OUT=$(mktemp --tmpdir out.XXXXXX)

cleanup()
{
	rm -f "$TEST_FILE_256MB" "$TEST_FILE_1GB" "$TEST_FILE_OUT"
}

trap cleanup EXIT

echo 'Generating test files ...'
dd if=/dev/urandom of="$TEST_FILE_256MB" bs=1M count=256
dd if=/dev/urandom of="$TEST_FILE_1GB" bs=8M count=128

echo "Tiers!"
set -x
./c0rm 21 10 -y
./c0cp 21 10 "$TEST_FILE_256MB" 1024 -x 1
./c0rm 21 20 -y
./c0cp 21 20 "$TEST_FILE_256MB" 1024 -x 2
./c0rm 21 30 -y
./c0cp 21 30 "$TEST_FILE_256MB" 1024 -x 3
set +x

echo "Contiguous Mode!"
set -x
./c0rm 21 21 -y
./c0cp 21 21 "$TEST_FILE_256MB" 1024 -fc 2
./c0cat 21 21 "$TEST_FILE_OUT" 1024 $((256*1024*1024)) -c 2
./c0cp 21 21 "$TEST_FILE_1GB" 1024 -fc 3
./c0cat 21 21 "$TEST_FILE_OUT" 1024 $((1024*1024*1024)) -c 3
set +x

echo "Performance Mode!"
set -x
#default tier
./c0cp 21 21 "$TEST_FILE_256MB" 1024 -pfc 5
./c0cat 21 21 "$TEST_FILE_OUT" 1024 $((256*1024*1024)) -pc 5
./c0cp 21 21 "$TEST_FILE_1GB" 1024 -pfc 3
./c0cat 21 21 "$TEST_FILE_OUT" 1024 $((1024*1024*1024)) -pc 3
#tiers 123
./c0rm 21 221 -y
./c0rm 21 222 -y
./c0rm 21 223 -y
./c0cp 21 221 "$TEST_FILE_1GB" 1024 -x 1 -p
./c0cp 21 222 "$TEST_FILE_1GB" 1024 -x 2 -p
./c0cp 21 223 "$TEST_FILE_1GB" 1024 -x 3 -p
./c0cp 21 221 "$TEST_FILE_1GB" 1024 -x 1 -pfc 3
./c0cp 21 222 "$TEST_FILE_1GB" 1024 -x 2 -pfc 3
./c0cp 21 223 "$TEST_FILE_1GB" 1024 -x 3 -pfc 3
./c0cat 21 221 "$TEST_FILE_OUT" 1024 $((1024*1024*1024)) -p
./c0cat 21 222 "$TEST_FILE_OUT" 1024 $((1024*1024*1024)) -p
./c0cat 21 223 "$TEST_FILE_OUT" 1024 $((1024*1024*1024)) -p
./c0cat 21 221 "$TEST_FILE_OUT" 1024 $((1024*1024*1024)) -pc 3
./c0cat 21 222 "$TEST_FILE_OUT" 1024 $((1024*1024*1024)) -pc 3
./c0cat 21 223 "$TEST_FILE_OUT" 1024 $((1024*1024*1024)) -pc 3
set +x

echo "Asynchronous Mode!"
set -x
#default tier
./c0cp 21 21 "$TEST_FILE_1GB" 1024 -a 8 -pf
./c0cp 21 21 "$TEST_FILE_1GB" 1024 -a 8 -pfc 3
./c0cp 21 21 "$TEST_FILE_256MB" 1024 -a 8 -pfc 5
#tiers 123 
./c0cp 21 221 "$TEST_FILE_1GB" 1024 -a 8 -x 1 -p
./c0cp 21 222 "$TEST_FILE_1GB" 1024 -a 8 -x 2 -p
./c0cp 21 223 "$TEST_FILE_1GB" 1024 -a 8 -x 3 -p
./c0cp 21 221 "$TEST_FILE_1GB" 1024 -a 8 -x 1 -pfc 3
./c0cp 21 222 "$TEST_FILE_1GB" 1024 -a 8 -x 2 -pfc 3
./c0cp 21 223 "$TEST_FILE_1GB" 1024 -a 8 -x 3 -pfc 3
set +x

echo "DONE!"

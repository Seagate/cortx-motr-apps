#!/usr/bin/bash

TEST_FILE_256MB=$(mktemp --tmpdir 256MB.XXXXXX)
TEST_FILE_1GB=$(mktemp --tmpdir 1GB.XXXXXX)
TEST_FILE_OUT=$(mktemp --tmpdir out.XXXXXX)

cleanup()
{
	rm -f $TEST_FILE_256MB $TEST_FILE_1GB $TEST_FILE_OUT
}

trap cleanup EXIT

echo 'Generating test files ...'
dd if=/dev/urandom of=$TEST_FILE_256MB bs=1M count=256
dd if=/dev/urandom of=$TEST_FILE_1GB bs=8M count=128

echo "Tiers!"
set -x
./c0rm 21 22 -y
./c0cp 21 22 $TEST_FILE_256MB 1024 -x 1
sleep 5
./c0rm 21 23 -y
./c0cp 21 23 $TEST_FILE_256MB 1024 -x 2
sleep 5
./c0rm 21 24 -y
./c0cp 21 24 $TEST_FILE_256MB 1024 -x 3
set +x

echo "Contiguous Mode!"
set -x
./c0cp 21 21 $TEST_FILE_256MB 1024 -fc 3
sleep 5
./c0ct 21 21 $TEST_FILE_OUT 1024 $((256*1024*1024)) -c 3
set +x

echo "Performance Mode!"
set -x
./c0cp 21 21 $TEST_FILE_256MB 1024 -pfc 3
sleep 5
./c0ct 21 21 $TEST_FILE_OUT 1024 $((256*1024*1024)) -pc 3
sleep 5
./c0cp 21 21 $TEST_FILE_1GB 1024 -pfc 3
sleep 5
./c0ct 21 21 $TEST_FILE_OUT 1024 $((256*1024*1024)) -pc 3
set +x

echo "DONE!"


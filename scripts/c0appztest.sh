#!/usr/bin/bash

echo "Tiers!"
set -x
./c0rm 21 22 -y
./c0cp 21 22 ~/tmp/256MB 1024 -x 1 
sleep 5
./c0rm 21 23 -y
./c0cp 21 23 ~/tmp/256MB 1024 -x 2 
sleep 5
./c0rm 21 24 -y
./c0cp 21 24 ~/tmp/256MB 1024 -x 3
set +x

echo "Contiguous Mode!"
set -x
./c0cp 21 21 ~/tmp/256MB 1024 -fc 3
sleep 5
./c0ct 21 21 ~/tmp/out 1024 $((256*1024*1024)) -c 3
set +x

echo "Performance Mode!"
set -x
./c0cp 21 21 ~/tmp/256MB 1024 -pfc 3
sleep 5
./c0ct 21 21 ~/tmp/out 1024 $((256*1024*1024)) -pc 3
sleep 5
./c0cp 21 21 ~/tmp/1GB 1024 -pfc 3
sleep 5
./c0ct 21 21 ~/tmp/out 1024 $((256*1024*1024)) -pc 3
set +x

echo "DONE!"


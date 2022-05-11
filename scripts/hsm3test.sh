#!/usr/bin/bash
source /dev/stdin <<<"$(./scripts/motraddr.sh --exp)"

if [ $(./scripts/motraddr.sh | grep TIER | wc -l) -ne 3 ]; then
	echo "ERROR!"	
	echo "Cannot run HSM test!!"
	echo "Please run a cluster with at least 3 data pools!!!"       
	exit
fi

set -x
m0hsm create 888:888 1
m0hsm show 888:888
m0hsm write 888:888 0 $((8*4096)) 11
m0hsm write 888:888 $((8*4096)) $((8*4096)) 22
m0hsm write 888:888 $((16*4096)) $((8*4096)) 33
m0hsm write 888:888 $((24*4096)) $((8*4096)) 44
m0hsm show 888:888
m0hsm dump 888:888
m0hsm move 888:888 0 $((32*4096)) 1 3
m0hsm show 888:888
m0hsm write 888:888 0 $((32*4096)) 55
m0hsm move 888:888 0 $((32*4096)) 1 2
m0hsm show 888:888
m0hsm write 888:888 0 $((32*4096)) 66
m0hsm archive 888:888 0 $((32*4096)) 3
m0hsm show 888:888
m0hsm write 888:888 0 $((32*4096)) 66
m0hsm stage 888:888 0 $((32*4096)) 2
m0hsm show 888:888
m0hsm set_write_tier 888:888 2
m0hsm show 888:888
m0hsm write 888:888 0 $((32*4096)) 77
m0hsm show 888:888
set +x

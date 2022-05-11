#!/usr/bin/bash
source /dev/stdin <<<"$(./scripts/motraddr.sh --exp)"
set -x
m0hsm create 555:555 1
m0hsm show 555:555
m0hsm write 555:555 0 4096 44
m0hsm show 555:555
m0hsm write 555:555 0 $((8*4096)) 44
m0hsm show 555:555
m0hsm write 555:555 $((2*4096)) $((8*4096)) 44
m0hsm show 555:555
m0hsm dump 555:555
set +x

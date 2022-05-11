#!/usr/bin/bash
source /dev/stdin <<<"$(./scripts/motraddr.sh --exp)"
#source <(./scripts/motraddr.sh --exp)
m0composite $CLIENT_LADDR $CLIENT_HA_ADDR $CLIENT_PROFILE $CLIENT_PROC_FID
mkdir -p $HOME/.hsm
./scripts/motraddr.sh | grep TIER > $HOME/.hsm/config
cat $HOME/.hsm/config

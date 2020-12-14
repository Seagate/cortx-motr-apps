#!/usr/bin/env bash
#
# This script clean install given Mero packages.
# It should work on Red Hat and Debian based systems.
#
# Ganesan Umanesan <ganesan.umanesan@seagate.com>
# 14/12/2020

# prints out a set of motr parameters for a 
# single connection.

hastatus=$(mktemp --tmpdir hastatus.XXXXXX)

cleanup()
{
        rm -f $hastatus
}

trap cleanup EXIT

#c="client-22"
#ssh client-21 hctl status > $hastatus
c=$HOSTNAME
hctl status > $hastatus

r=$((3 + $RANDOM % 16))
p=()

# HA_ENDPOINT_ADDR
p[0]=$(grep -A1 client-21 $hastatus | tail -n1 | awk '{print $4}')
# PROFILE_FID
p[1]=$(grep -A2 Profile $hastatus | tail -n1 | awk '{print $1}')

# Data pools
p[2]=$(grep -A4 'Data pools' $hastatus | grep tier1 | awk '{print $1}')
p[3]=$(grep -A4 'Data pools' $hastatus | grep tier2 | awk '{print $1}')
p[4]=$(grep -A4 'Data pools' $hastatus | grep tier3 | awk '{print $1}')

# LOCAL_ENDPOINT_ADDR0
p[5]=$(grep -A$r $c $hastatus | tail -n1 | awk '{print $4}')
# LOCAL_PROC_FID0
p[6]=$(grep -A$r $c $hastatus | tail -n1 | awk '{print $3}')


echo "#"
echo "# USER: $USER"
echo "# Application: All"
echo "#"

echo
echo "HA_ENDPOINT_ADDR = ${p[0]}"
echo "PROFILE_FID = ${p[1]}"

echo
echo "M0_POOL_TIER1 = ${p[2]}"
echo "M0_POOL_TIER2 = ${p[3]}"
echo "M0_POOL_TIER3 = ${p[4]}"

echo
echo "LOCAL_ENDPOINT_ADDR0 = ${p[5]}"
echo "LOCAL_PROC_FID0 = ${p[6]}"

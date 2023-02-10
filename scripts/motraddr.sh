#!/usr/bin/env bash
#
# prints out a set of motr parameters for a 
# single connection.
#
# Ganesan Umanesan <ganesan.umanesan@seagate.com>
# 14/12/2020 - initial script
# 28/12/2020 - MIO yaml format added
# 05/01/2021 - Bash export format added 
# 20/02/2021 - hastatus.yaml in /etc/motr
# 26/09/2022 - ceph.config file
#

hastatusetc='/etc/motr/hastatus.yaml'
hastatustmp=$(mktemp --tmpdir hastatus.XXXXXX)

cleanup()
{
        rm -f "$hastatustmp"
}

trap cleanup EXIT

c=$(hostname -s)
[[ -f "$hastatusetc" ]] && hastatus="$hastatusetc"
[[ ! -f "$hastatusetc" ]] && hastatus="$hastatustmp"

# exclude cache, read afresh 
for a in "$@" ; do
     [[ "$a" = "-x" ]] && hastatus="$hastatustmp"
     [[ "$a" = "--exclude-cache" ]] && hastatus="$hastatustmp"
done

# use /tmp/hastatus.yaml
if [ "$hastatus" == "$hastatustmp" ]; then
	hctl status > "$hastatustmp"  
	echo "sage-tier9-9" >> "$hastatustmp"	#add a dummy node pattern at the end 
fi

pools=()
descp=()
pools=($(sed -n "/Data/,/Profile/p" "$hastatus" | sed -n "/0x.*/p" | awk '{print $1}'))
descp=($(sed -n "/Data/,/Profile/p" "$hastatus" | sed -n "/0x.*/p" | awk '{print $2}'))
#printf '%s\n' "${pools[@]}"
#printf '%s\n' "${descp[@]}"
if [ ${#pools[@]} -ne ${#descp[@]} ]; then
        echo "pool/description mismatch"
        exit
fi
#for (( idx=0; idx<${#pools[@]}; idx++ ))
#do
#	echo "${pools[$idx]} ${descp[$idx]}"
#done

profs=()
profs=($(sed -n "/Profile/,/Services/p" "$hastatus" | sed -n "/0x.*/p" | awk '{print $1}'))
#printf '%s\n' "${profs[@]}"
#for (( idx=0; idx<${#profs[@]}; idx++ ))
#do
#	echo "${profs[$idx]}"
#done

#regex for sage nodes
spat0+="client-2[1-8]|datawarp-0[1-4]|visnode-0[1-4]|sage-tier[0-9]-[0-9]"
spat0+="|client-tx2-[0-9]|sage-tier[0-9]-[0-9]a"
#regex for ssc nodes
spat0+="|ssc-eurd-[0-9][0-9][0-9][0-9]"

laddr=()
lproc=()

#echo $c
#sed -nE "/Services/,/$spat0/p" $hastatus | sed '1d;$d' | sed 's/[:@]/ /g' | awk '/hax|m0_client/ {print $0}'
#sed -nE "/Services/,/$spat0/p" $hastatus | sed '1d;$d'
#sed -nE "/Services/,/$spat0/p" $hastatus
#exit

#single-node cluster
if grep -q "localhost" "$hastatus"; then
	laddr=($(sed -nE "/Services/,/$spat0/p" "$hastatus" | sed '1d;$d' | awk '/hax|m0_client/ {print $4}'))
	lproc=($(sed -nE "/Services/,/$spat0/p" "$hastatus" | sed '1d;$d' | awk '/hax|m0_client/ {print $3}'))
#multi-node sage cluster
else
	laddr=($(sed -nE "/$c/,/$spat0/p" "$hastatus" | sed '1d;$d' | awk '/hax|m0_client/ {print $4}'))
	lproc=($(sed -nE "/$c/,/$spat0/p" "$hastatus" | sed '1d;$d' | awk '/hax|m0_client/ {print $3}'))
fi
#printf '%s\n' "${laddr[@]}"
#printf '%s\n' "${lproc[@]}"
if [ ${#laddr[@]} -ne ${#lproc[@]} ]; then
        echo "addr/port mismatch"
        exit
fi
#for (( idx=0; idx<${#laddr[@]}; idx++ ))
#do
#	echo "${laddr[$idx]} ${lproc[$idx]}"
#done
#exit

s=$((${#laddr[@]}-1))	# max local addresses
r=$((0 + RANDOM % s))	# random local address

usage()
{
    cat <<USAGE_END
Usage: $(basename "$0") [-h|--help] [options]
	
    -m|--mio	print mio configuration parameters

    -e|--exp	print in shell export format

    -h|--help	Print this help screen.
USAGE_END

	exit 1
}

exp()
{
	read -r -d '' BASH <<EOF
# $USER $HOSTNAME
# Bash shell export format
export CLIENT_LADDR="${laddr[1+$r]}"
export CLIENT_HA_ADDR="${laddr[0]}"
export CLIENT_PROFILE="${profs[0]}"
export CLIENT_PROC_FID="${lproc[1+$r]}"
EOF
	echo "$BASH"
}

mio()
{
	read -r -d '' YAML <<EOF
# $USER $HOSTNAME
# MIO configuration Yaml file. 
# MIO_Config_Sections: [MIO_CONFIG, MOTR_CONFIG]
MIO_CONFIG:
  MIO_LOG_DIR:
  MIO_LOG_LEVEL: MIO_DEBUG 
  MIO_DRIVER: MOTR
  MIO_TELEMETRY_STORE: ADDB
  
MOTR_CONFIG:
  MOTR_USER_GROUP: motr 
  MOTR_INST_ADDR: ${laddr[1+$r]}
  MOTR_HA_ADDR: ${laddr[0]}
  MOTR_PROFILE: <${profs[0]}>
  MOTR_PROCESS_FID: <${lproc[1+$r]}>
  MOTR_DEFAULT_UNIT_SIZE: 1048576
  MOTR_IS_OOSTORE: 1
  MOTR_IS_READ_VERIFY: 0
  MOTR_TM_RECV_QUEUE_MIN_LEN: 64
  MOTR_MAX_RPC_MSG_SIZE: 131072
  MOTR_POOLS:
     # Set SAGE cluster pools, ranking from high performance to low. 
     # The pool configuration parameters can be queried using hare.
     # MOTR_POOL_TYPE currently Only supports HDD, SSD or NVM.
     - MOTR_POOL_NAME:	Pool1  
       MOTR_POOL_ID:  	${pools[0]}
       MOTR_POOL_TYPE: 	NVM
     - MOTR_POOL_NAME: 	Pool2
       MOTR_POOL_ID:  	${pools[1]}
       MOTR_POOL_TYPE:	SSD
     - MOTR_POOL_NAME: 	Pool3
       MOTR_POOL_ID:  	${pools[2]}
       MOTR_POOL_TYPE: 	HDD
EOF

	echo "$YAML"
}

miof()
{
	r="$1"
	((r--))
	mio > "$2"
}

rgw()
{
	read -r -d '' CEPH <<EOF
[global]
debug rgw = 20
 
[client]
rgw backend store = motr
motr profile fid = ${profs[0]}
motr ha endpoint = ${laddr[0]}
motr my endpoint = ${laddr[1]}
motr my fid = ${lproc[1]}
motr admin endpoint = ${laddr[1]}
motr admin fid = ${lproc[1]}

[client.rgw]
rgw frontends = beast port=8000
log file = /var/log/ceph/ceph-client.rgw.log
EOF

	echo "$CEPH"
}

#
# MAIN
#

# options
TEMP=$( getopt -o cdxmeh --long create-cache,delete-cache,exclude-cache,mio,exp,help,ceph-config -n "$PROG_NAME" -- "$@" )
[[ $? != 0 ]] && usage
eval set -- "$TEMP"

while true ; 
	do
    case "$1" in
     	-x|--exclude-cache)
     	shift
		;;
     	-d|--delete-cache)
     	set -x
     	rm -rf "$hastatusetc"
     	rm -rf /tmp/hastatus.*
     	set +x
     	shift
     	exit
		;;
     	-c|--create-cache)
     	set -x
     	hctl status > "$hastatusetc"  
		echo "dummy-node9-9" >> "$hastatusetc"
		tail "$hastatusetc"
     	set +x
     	shift
     	exit
		;;
     	-m|--mio)
     	# print to stdout 
     	if [ "$#" -eq 2 ]; then
    		mio
    		exit 0
		fi
     	# print to files
    	shift; 	shift; z="$1"; shift
    	[[ "$z" -ge 1 ]] || { echo "Invalid number!, must be > 0!!"; exit 1; }
    	[[ "$#" -eq "$z" ]] || { echo "Illeg number of arguments!"; exit 1; }
     	for (( i=1; i<="$z"; i++ ))
		do
   			miof "$i" "$1"; shift
		done		
		exit 0
    	shift
    	;;
     	-e|--exp)
		exp
		exit 0
    	shift
    	;;
     	--ceph-config)
		rgw
		exit 0
    	shift
    	;;
     	-h|--help)
    	usage
    	shift
    	;;  	
		--)     
       	shift
     	break 
     	;;
    	*)
      	echo 'getopt: internal error...'
      	exit 1 
      	;;
   	esac
    done

echo "#"
echo "# USER: $USER"
echo "# Application: All"
echo "#"

echo
echo "HA_ENDPOINT_ADDR = ${laddr[0]}"
echo "PROFILE_FID = ${profs[0]}"

echo
for (( idx=0; idx<${#pools[@]}; idx++ ))
do
	echo "M0_POOL_TIER$((idx+1)) = ${pools[$((idx))]}"
done

echo
for (( idx=0; idx<${#laddr[@]}-1; idx++ ))
do
	echo "LOCAL_ENDPOINT_ADDR$((idx)) = ${laddr[1+(($idx+$r)%$s)]}"
	echo "LOCAL_PROC_FID$((idx)) = ${lproc[1+(($idx+$r)%$s)]}"
done

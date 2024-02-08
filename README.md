[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://github.com/Seagate/cortx-motr-apps/blob/main/LICENSE) [![Slack](https://img.shields.io/badge/chat-on%20Slack-blue")](https://join.slack.com/t/cortxcommunity/shared_invite/zt-femhm3zm-yiCs5V9NBxh89a_709FFXQ?) [![YouTube](https://img.shields.io/badge/Video-YouTube-red)](https://cortx.link/videos)

# Disclaimer: This project is not maintained anymore
# Motr client sample apps

There are three sample applications.
A running instance of a Motr cluster such as a singlenode Motr cluster is a prerequisite for
compiling, building and running these applications. 

1.	c0cp
2.	c0cat
3.	c0rm

## Installation
Download, build and test/run using the following commands:

```sh
git clone https://github.com/Seagate/cortx-motr-apps
cd cortx-motr-apps
./scripts/install-build-deps.sh
./autogen.sh
./configure
make
make test
make clean
make distclean
```

Generate rc files on VM or Sage cluster

```sh
make vmrcf
make sagercf
```

### Quick Start on Sage platform

```sh
git clone https://github.com/Seagate/cortx-motr-apps
cd cortx-motr-apps
./autogen.sh
./configure
make 
make sagercf
make test
```

### Quick Start on VM

```sh
git clone https://github.com/Seagate/cortx-motr-apps
cd cortx-motr-apps
./autogen.sh
./configure
make 
make vmrcf
make test
```

### How to obtain connection parameters on a VM or Sage cluster

```sh
cd cortx-motr-apps
./scripts/motraddr.sh 
#
# USER: seagate
# Application: All
#

HA_ENDPOINT_ADDR = inet:tcp:10.0.2.15@22001
PROFILE_FID = 0x7000000000000001:0x5b

M0_POOL_TIER1 = 0x6f00000000000001:0x31
M0_POOL_TIER2 = 0x6f00000000000001:0x3b
M0_POOL_TIER3 = 0x6f00000000000001:0x45

LOCAL_ENDPOINT_ADDR0 = inet:tcp:10.0.2.15@21502
LOCAL_PROC_FID0 = 0x7200000000000001:0x2c
LOCAL_ENDPOINT_ADDR1 = inet:tcp:10.0.2.15@21501
LOCAL_PROC_FID1 = 0x7200000000000001:0x29
```

### HSM 

```sh
make hsminit
make hsm1test
make hsm3test
```

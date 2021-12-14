[![Codacy Badge](https://app.codacy.com/project/badge/Grade/7d9b003bbaeb449dac098b2bf72197fa)](https://www.codacy.com/gh/Seagate/m0client-sample-apps/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=Seagate/m0client-sample-apps&amp;utm_campaign=Badge_Grade)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://github.com/Seagate/cortx-motr-apps/blob/main/LICENSE) [![Slack](https://img.shields.io/badge/chat-on%20Slack-blue")](https://join.slack.com/t/cortxcommunity/shared_invite/zt-femhm3zm-yiCs5V9NBxh89a_709FFXQ?) [![YouTube](https://img.shields.io/badge/Video-YouTube-red)](https://cortx.link/videos)


# Motr client sample apps

There are three sample applications.
A running instance of Motr such as singlenode Motr is a prerequisite for
compiling, building and running these applications. 

1. c0cp
2. c0cat
3. c0rm

### Installation
Build dependencies using following command
```sh
$ ./scripts/install-build-deps
```

Compile, build and run applications
```sh
$ autogen.sh
$ configure
$ make
$ make test
```

Generate rc files on VM
```sh
$ make vmrcf
```

### Clean 

```sh
make clean
```

### Clean distribution
```sh
make distclean
```

### Clean install motr

The clean install motr script stops currently running m0singlenode,
uninstall motr and motr-devel packages, removes all configuration and
storage disks. It then installs the new version pointed by directory path,
configures it as a m0singlenode and starts services.

```sh
$ ./scripts/clean_install_motr <rpms directory path>
$ # (example) ./scripts/clean_install_motr ./rpms/jenkins-OSAINT_motr-1400-29-g7a51cbd/
```

### Apps resource file <.[app]rc>
Each application requires a resource file residing in the same directory
where the application is residing. This resource file contains all
Motr-client related resource parameters for running the application
on a particular client node where the application is executed.
This file can be generated using the cappsrcgen utility for dev VMs.
The name of the file should be .[application name]rc. Replace the
application name with your application's basename. A c0cp example
is shown below:

```sh
$ ./scripts/c0appzrcgen > .c0cprc  # Generates configuration
```

Containts of `.c0cprc` is as follows
```
#local address
10.0.2.15@tcp:12345:44:101

#ha address
10.0.2.15@tcp:12345:45:1

#profile id
<0x7000000000000001:0>

#process id
<0x7200000000000000:0>

#tmp
/tmp/
```

### How to generate resource files on the Sage cluster?

Pre-registration with Sage userID and application names is required.
Once registered the sage-user-application-assignment script can be used
on the CMU node to generate this information. This script takes Username,
app name and the client IP as input and outputs the required resource
information. See example below:

```
[sage0004@sage-cmu]$ sage-user-application-assignment ganesan c0del 172.18.1.21

#
# USER: ganesan
# Application: c0rm
#

#local address
172.18.1.21@tcp:12345:41:304

#ha address
172.18.1.21@tcp:12345:34:101

#profile id
<0x7000000000000001:0x1>

#process id
<0x7200000000000001:0x3b>

#tmp
/tmp/
```

### Quick Start on Sage platform

```
>> make clean
>> git pull
>> autogen.sh
>> configure
>> make 
>> make sagercf
>> make test
```


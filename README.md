- clovis-sample-apps

There are three sample applications. A running instance of Mero such as singlenode Mero is a prerequisite for compiling, building and running these Clovis applications. 
1. c0cp
2. c0cat
3. c0rm

- install build dependencies
>> sudo ./install-build-deps

- compile, build and run applications
>> make
>> make test

- generate rc files on VM
>> make rcfile

- clean
>> make clean

- Clean install mero
The clean install mero script stops currently running m0singlenode, uninstall mero and mero-devel packages, removes all configuration and storage disks. It then installs the new version pointed by directory path, configuraes it as a m0singlenode and starts services.
>> ./clean_install_mero
illegal number of parameters
clean_install_mero <rpms directory path>
>> ./clean_install_mero ./rpms/jenkins-OSAINT_mero-1400-29-g7a51cbd/

- clovis apps resource file <.[app]rc>
Each Clovis application requires a clovis resource file residing in the same directory where the application is residing. This resource file contains all clovis related resource parameters for running the clovis application on a particular client machine where the application is executed. This file can be generated using the cappsrcgen utility for dev VMs. The name of the file should be .[application name]rc. Replace the application name with your application's basename. A c0cp example is shown below:
>>./c0appzrcgen > .c0cprc
[sudo] password for seagate: 
>>cat .c0cprc
10.0.2.15@tcp:12345:44:101
10.0.2.15@tcp:12345:45:1
<0x7000000000000001:0>
<0x7200000000000000:0>
/tmp/

- How to generate resource files on the Sage cluster?
Pre-registration with Sage userID and application names is required. Once registered the sage-user-application-assignment script can be used on the CMU to generate this information.This script takes Username, app name and the client IP as input and outputs the required resource information. See example below:

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


#!/usr/bin/bash

echo "WARNING!"
echo "This will install lustre and eos!!"
echo "Press Ctl-C to exit, anykey to continue!!"
read -rn1 _ #unused or throw away variable _

set -x
sudo m0singlenode stop
sudo lctl list_nids
sudo systemctl stop lnet
sudo cp /etc/lnet.conf /etc/lnet.conf.bak
sudo cp /etc/modprobe.d/lnet.conf /etc/modprobe.d/lnet.conf.bak

sudo yum remove lustre-client lustre-client-devel kmod-lustre-client -y
sudo rm -rf /usr/src/lustre-client-*
sudo rm -rf /usr/libexec/motr/
sudo rm -rf /etc/motr/
sudo rm -rf /var/motr/
sudo yum install ./lustre-client*.rpm lustre-client-devel*.rpm kmod-lustre-client*.rpm -y
sudo yum install ./eos-core-*.rpm ./eos-core-devel-*.rpm -y

sudo cp /etc/modprobe.d/lnet.conf.bak /etc/modprobe.d/lnet.conf
sudo cp /etc/lnet.conf.bak /etc/lnet.conf
sudo systemctl start lnet
sudo lctl list_nids

set +x
echo "DONE!"


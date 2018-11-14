#/*
# * COPYRIGHT 2014 SEAGATE LLC
# *
# * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
# * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE LLC,
# * ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
# * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
# * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
# * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
# * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
# *
# * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
# * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
# * http://www.xyratex.com/contact
# *
# * Original author:  Ganesan Umanesan <ganesan.umanesan@seagate.com>
# * Original creation date: 10-Jan-2017
# *
# * Subsequent Modification: Abhishek Saha <abhishek.saha@seagate.com>
# * Modification Date: 02-Nov-2018
#*/


#sudo
SUDO = sudo
#or not
undefine SUDO

#files
FILE1 = './file1'
FILE2 = './file2'
FILE3 = './file3'

#executables
EXE1 = c0cp
EXE2 = c0ct
EXE3 = c0rm
EXE4 = fgen
EXE5 = c0cp_async

#archieve/node names 
TARF = m0trace_$(shell date +%Y%m%d-%H%M%S).tar.bz2
TARN = $(shell ls -la m0trace.* &> /dev/null | wc -l)
NODE = $(shell eval uname -n)

#c0cp parameters
#valid block sizes are: 4KB ~ 32MB
#4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288,
#1048576, 2097152, 4194304, 8388608, 16777216, 33554432
BSZ = 4096
CNT = 1024

#compiler/linker options
LFLAGS += -lm -lpthread -lrt -lgf_complete -lyaml -luuid -lmero
CFLAGS += -D_REENTRANT -D_GNU_SOURCE -DM0_INTERNAL='' -DM0_EXTERN=extern
CFLAGS += -fno-common -Wall -Werror -Wno-attributes -fno-strict-aliasing 
CFLAGS += -fno-omit-frame-pointer -g -O2 -Wno-unused-but-set-variable 
CFLAGS += -rdynamic 
ifneq ($(M0_SRC_DIR),)
LFLAGS += -L$(M0_SRC_DIR)/mero/.libs -Wl,-rpath,$(M0_SRC_DIR)/mero/.libs
LFLAGS += -L$(M0_SRC_DIR)/extra-libs/gf-complete/src/.libs -Wl,-rpath,$(M0_SRC_DIR)/extra-libs/gf-complete/src/.libs
CFLAGS += -I$(M0_SRC_DIR)
endif

all: $(EXE1) $(EXE2) $(EXE3) $(EXE5)
.PHONY: all

$(EXE1):
	gcc c0appz.c c0cp.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -o $(EXE1)

$(EXE2):
	gcc c0appz.c c0ct.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -o $(EXE2)

$(EXE3):
	gcc c0appz.c c0rm.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -o $(EXE3)

$(EXE5):
	gcc c0appz.c c0cp_async.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -o $(EXE5)

test: $(EXE1) $(EXE2) $(EXE3) $(EXE5)
	$(SUDO) dd if=/dev/urandom of=$(FILE1) bs=$(BSZ) count=$(CNT)
	@echo "#####"
	@ls -lh $(FILE1)
	$(SUDO) ./$(EXE1) 0 1048577 $(FILE1) $(BSZ) $(CNT)
	@echo "#####"
	$(SUDO) ./$(EXE2) 0 1048577 $(BSZ) $(CNT) > $(FILE2)
	@ls -lh $(FILE2)
	@echo "#####"
	@ls -lh $(FILE1)
	$(SUDO) ./$(EXE5) 0 1048599 $(FILE1) $(BSZ) $(CNT)
	@echo "#####"
	$(SUDO) ./$(EXE2) 0 1048599 $(BSZ) $(CNT) > $(FILE3)
	@ls -ls $(FILE3)
	@echo "#####"
	cmp $(FILE1) $(FILE2) || echo "ERROR: Test Failed !!"
	@echo "#####"
	cmp $(FILE1) $(FILE3) || echo "ERROR: Async Test Failed !!"
	@echo "#####"
	$(SUDO) ./$(EXE3) 0 1048577
	$(SUDO) ./$(EXE3) 0 1048599

#yaml
#bundle trace files for shipment
yaml:
	@ls m0trace.* &> /dev/null || (echo "No traces!" && exit 1)
	@for file in m0trace.*; do					\
		echo $$file;							\
	ls -lh $$file;							\
		m0trace -Y -i $$file -o $$file.yml;	\
		ls -lh $$file.yml;						\
	done
	tar -jcvf $(TARF) m0trace.*.yml
	tar -jtvf $(TARF)
	rm -f m0trace.*.yml
	ls -lh $(TARF)

fgen:
	gcc -Wall -lssl -lcrypto fgen.c -o $(EXE4)

vmrcf:
	mkdir -p .$(EXE1)rc
	mkdir -p .$(EXE2)rc
	mkdir -p .$(EXE3)rc
	mkdir -p .$(EXE5)rc
	./scripts/c0appzrcgen > ./.$(EXE1)rc/$(NODE)
	./scripts/c0appzrcgen > ./.$(EXE2)rc/$(NODE)
	./scripts/c0appzrcgen > ./.$(EXE3)rc/$(NODE)
	./scripts/c0appzrcgen > ./.$(EXE5)rc/$(NODE)

sagercf:
	mkdir -p .${EXE1}rc
	mkdir -p .${EXE2}rc
	mkdir -p .${EXE3}rc
	mkdir -p .${EXE5}rc
	sage-user-application-assignment ganesan $(EXE1) 172.18.1.${c} > .$(EXE1)rc/client-${c}
	sage-user-application-assignment ganesan $(EXE2) 172.18.1.${c} > .$(EXE2)rc/client-${c}
	sage-user-application-assignment ganesan $(EXE3) 172.18.1.${c} > .$(EXE3)rc/client-${c}
	sage-user-application-assignment ganesan $(EXE5) 172.18.1.${c} > .$(EXE5)rc/client-${c}

clean:
	rm -f $(EXE1) $(EXE2) $(EXE3) $(EXE5) m0trace.*
	rm -f $(FILE1) $(FILE2) $(FILE3)
	rm -f $(EXE4)

m0t1fs:
	touch /mnt/m0t1fs/0:3000
	setfattr -n lid -v 8 /mnt/m0t1fs/0:3000
	dd if=/dev/zero of=/mnt/m0t1fs/0:3000 bs=4M count=10
	ls -lh /mnt/m0t1fs/0:3000
	rm -rf /mnt/m0t1fs/0:3000

#
#MPI Appz
#

EXE6 = mpix

mpix:
	mpicc c0appz.c mpiapp.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -o $(EXE6)

mpi-clean:
	rm -f m0trace.*
	rm -f $(EXE6)

mpi-sagercf:
	mkdir -p .${EXE6}rc
	sage-user-application-assignment ganesan $(EXE6) 172.18.1.${c} > .$(EXE6)rc/client-${c}


#
#ECMWF Appz
#

ecmwf:
	gcc c0appz.c c0fgen.c ecmwf.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -lssl -lcrypto -o ecmwfx

ecmwf-clean:
	rm -f m0trace.*
	rm -f ecmwfx

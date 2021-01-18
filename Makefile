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
# *
# * Modification: Ganesan Umanesan <ganesan.umanesan@seagate.com>
# * Modification Date: 07-May-2020
# *
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
C0CP = c0cp
C0CT = c0cat
C0RM = c0rm
FGEN = fgen

RCDIR = $(HOME)/.c0appz
APPASSIGN = sage-user-application-assignment

#isc executables and library
LIBISC = libdemo.so
ISC_REG = c0isc_reg
ISC_INVK = c0isc_demo

#archive/node names
TARF = m0trace_$(shell date +%Y%m%d-%H%M%S).tar.bz2
TARN = $(shell ls -la m0trace.* &> /dev/null | wc -l)
NODE = $(shell eval uname -n)

#dd block size, count and filesize
#dd can have any block size and a random
#number of blocks, making the file size
#random
DDZ := 1032
CNT := $(shell expr 100 + $$RANDOM % 1000)
#CNT := $(shell expr 1024 \* 16)
#CNT := 1024
FSZ := $(shell expr $(DDZ) \* $(CNT) )
FID1 := $(shell eval $(FGEN))
FID2 := $(shell eval $(FGEN))

#c0cp block size
#valid block sizes are: 4KB ~ 32MB
#4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288,
#1048576, 2097152, 4194304, 8388608, 16777216, 33554432
BSZ := 64

#compiler/linker options
LFLAGS += -lm -lpthread -lrt -lgalois -lyaml -luuid -lmotr
CFLAGS += -I/usr/include/motr
CFLAGS += -D_REENTRANT -D_GNU_SOURCE -DM0_INTERNAL='' -DM0_EXTERN=extern
CFLAGS += -fno-common -Wall -Werror -Wno-attributes -fno-strict-aliasing
CFLAGS += -fno-omit-frame-pointer -g -O2 -Wno-unused-but-set-variable
CFLAGS += -rdynamic
ifneq ($(M0_SRC_DIR),)
LFLAGS += -L$(M0_SRC_DIR)/motr/.libs -Wl,-rpath,$(M0_SRC_DIR)/motr/.libs
LFLAGS += -L$(M0_SRC_DIR)/extra-libs/galois/src/.libs -Wl,-rpath,$(M0_SRC_DIR)/extra-libs/gf-complete/src/.libs
CFLAGS += -I$(M0_SRC_DIR)
endif

SRC = perf.o buffer.o qos.o c0appz.o
SRC_ALL = $(SRC) c0cp.o c0cat.o c0rm.o fgen.o \
		c0isc_register.o c0isc_demo.o isc_libdemo.o

all: $(C0CP) $(C0CT) $(C0RM) $(FGEN) isc-all
.PHONY: all

# Generate automatic dependencies,
# see https://www.gnu.org/software/make/manual/html_node/Automatic-Prerequisites.html
%.d: %.c
	@set -e; rm -f $@; \
		$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$
-include $(SRC_ALL:.o=.d)

$(C0CP): $(SRC) c0cp.c
	gcc $(SRC) c0cp.c -I/usr/include/motr $(CFLAGS) $(LFLAGS) -o $(C0CP)

$(C0CT): $(SRC) c0cat.c
	gcc $(SRC) c0cat.c -I/usr/include/motr $(CFLAGS) $(LFLAGS) -o $(C0CT)

$(C0RM): $(SRC) c0rm.c
	gcc $(SRC) c0rm.c -I/usr/include/motr $(CFLAGS) $(LFLAGS) -o $(C0RM)

$(FGEN):
	gcc -Wall -lssl -lcrypto fgen.c -o $(FGEN)

test: $(C0CP) $(C0CT) $(C0RM) $(FGEN)
	$(SUDO) dd if=/dev/urandom of=$(FILE1) bs=$(DDZ) count=$(CNT)
	@echo "#####"
	@ls -lh $(FILE1)
	$(SUDO) ./$(C0CP) $(FID1) $(FILE1) $(BSZ) -p
	@echo "#####"
	@ls -la $(FILE1)
	$(SUDO) ./$(C0CT) $(FID1) $(FILE2) $(BSZ) $(FSZ) -p
	@ls -la $(FILE2)
	@echo "#####"
	@ls -la $(FILE1)
	$(SUDO) ./$(C0CP) $(FID2) $(FILE1) $(BSZ) -a 8 -p
	@echo "#####"
	$(SUDO) ./$(C0CT) $(FID2) $(FILE3) $(BSZ) $(FSZ) -p
	@ls -ls $(FILE3)
	@echo "#####"
	cmp $(FILE1) $(FILE2) || echo "ERROR: Test Failed !!"
	@echo "#####"
	cmp $(FILE1) $(FILE3) || echo "ERROR: Async Test Failed !!"
	@echo "#####"
	$(SUDO) ./$(C0RM) $(FID1) -y
	$(SUDO) ./$(C0RM) $(FID2) -y

#yaml
#bundle trace files for shipment
yaml:
	@ls m0trace.* &> /dev/null || (echo "No traces!" && exit 1)
	@for file in m0trace.*; do					\
		echo $$file;							\
	ls -lh $$file;								\
		m0trace -Y -i $$file -o $$file.yml;		\
		ls -lh $$file.yml;						\
	done
	tar -jcvf $(TARF) m0trace.*.yml
	tar -jtvf $(TARF)
	rm -f m0trace.*.yml
	ls -lh $(TARF)

vmrcf:
	mkdir -p $(RCDIR)/${C0CP}rc
	mkdir -p $(RCDIR)/${C0CT}rc
	mkdir -p $(RCDIR)/${C0RM}rc
	mkdir -p $(RCDIR)/$(ISC_REG)rc
	mkdir -p $(RCDIR)/$(ISC_INVK)rc
	./scripts/c0appzrcgen > $(RCDIR)/$(C0CP)rc/$(NODE)
	./scripts/c0appzrcgen > $(RCDIR)/$(C0CT)rc/$(NODE)
	./scripts/c0appzrcgen > $(RCDIR)/$(C0RM)rc/$(NODE)
	./scripts/c0appzrcgen > $(RCDIR)/$(ISC_REG)rc/$(NODE)
	./scripts/c0appzrcgen > $(RCDIR)/$(ISC_INVK)rc/$(NODE)

sagercf:
	mkdir -p $(RCDIR)/${C0CP}rc
	mkdir -p $(RCDIR)/${C0CT}rc
	mkdir -p $(RCDIR)/${C0RM}rc
	mkdir -p $(RCDIR)/$(ISC_REG)rc
	mkdir -p $(RCDIR)/$(ISC_INVK)rc
#	$(APPASSIGN) ganesan $(C0CP) 172.18.1.${c} > $(RCDIR)/$(C0CP)rc/client-${c}
#	$(APPASSIGN) ganesan $(C0CT) 172.18.1.${c} > $(RCDIR)/$(C0CT)rc/client-${c}
#	$(APPASSIGN) ganesan $(C0RM) 172.18.1.${c} > $(RCDIR)/$(C0RM)rc/client-${c}
#	$(APPASSIGN) ganesan $(C0CP) 172.18.1.${c} > $(RCDIR)/$(ISC_REG)rc/client-${c}
#	$(APPASSIGN) ganesan $(C0CT) 172.18.1.${c} > $(RCDIR)/$(ISC_INVK)rc/client-${c}
	./scripts/motraddr.sh > out-$(HOSTNAME).txt
	@echo "#####"
	cat ./out-$(HOSTNAME).txt
	@echo "#####"
	cp out-$(HOSTNAME).txt $(RCDIR)/$(C0CP)rc/$(NODE)
	cp out-$(HOSTNAME).txt $(RCDIR)/$(C0CT)rc/$(NODE)
	cp out-$(HOSTNAME).txt $(RCDIR)/$(C0RM)rc/$(NODE)
	cp out-$(HOSTNAME).txt $(RCDIR)/$(ISC_REG)rc/$(NODE)
	cp out-$(HOSTNAME).txt $(RCDIR)/$(ISC_INVK)rc/$(NODE)
	rm -rf out-$(HOSTNAME).txt
	
clean: isc-clean
	rm -f $(C0CP) $(C0CT) $(C0RM) m0trace.*
	rm -f $(FILE1) $(FILE2) $(FILE3)
	rm -f $(FGEN)
	rm -f sfilet-* snodet-* fidout-*
	rm -f upFile dwFile
	rm -f *.o *.d *.d.* help.h

m0t1fs:
	touch /mnt/m0t1fs/0:3000
	setfattr -n lid -v 8 /mnt/m0t1fs/0:3000
	dd if=/dev/zero of=/mnt/m0t1fs/0:3000 bs=4M count=10
	ls -lh /mnt/m0t1fs/0:3000
	rm -rf /mnt/m0t1fs/0:3000

bigtest:
	make
	make fgen
	touch ./.fgenrc
	./scripts/single_node_test 4096 1024*1   1
	./scripts/single_node_test 4096 1024*2   1
	./scripts/single_node_test 4096 1024*4   1
	./scripts/single_node_test 4096 1024*8   1
	./scripts/single_node_test 4096 1024*16  1
	./scripts/single_node_test 4096 1024*32  1
	./scripts/single_node_test 4096 1024*64  1
	./scripts/single_node_test 4096 1024*128 1
	./scripts/single_node_test 4096 1024*256 1
	./scripts/single_node_test 4096 1024*512 1

#
#MPI Appz
#

EXE6 = mpix

mpix:
	mpicc c0appz.c mpiapp.c -I/usr/include/motr $(CFLAGS) $(LFLAGS) -o $(EXE6)

mpi-clean:
	rm -f m0trace.*
	rm -f $(EXE6)

mpi-sagercf:
	mkdir -p .${EXE6}rc
	$(APPASSIGN) ganesan $(EXE6) 172.18.1.${c} > .$(EXE6)rc/client-${c}

#
#ISC Demo
#Sage Function Shipping
#

$(ISC_REG): $(SRC) c0isc_register.c
	gcc $(SRC) c0isc_register.c -I/usr/include/motr -g $(CFLAGS) $(LFLAGS) -o $(ISC_REG)
$(LIBISC): isc_libdemo.c
	gcc isc_libdemo.c -I/usr/include/motr $(CFLAGS) -fpic -shared -o $(LIBISC)
$(ISC_INVK): $(SRC) c0isc_demo.c
	gcc $(SRC) c0isc_demo.c -I/usr/include/motr -g $(CFLAGS) $(LFLAGS) -o $(ISC_INVK)
isc-all: $(ISC_REG) $(ISC_INVK) $(LIBISC)
isc-clean:
	rm -f $(ISC_REG) $(ISC_INVK) $(LIBISC)

#
#ECMWF Appz
#

ecmwf:
	gcc c0appz.c c0fgen.c ecmwf.c -I/usr/include/motr $(CFLAGS) $(LFLAGS) -lssl -lcrypto -o ecmwfx

ecmwf-clean:
	rm -f m0trace.*
	rm -f ecmwfx

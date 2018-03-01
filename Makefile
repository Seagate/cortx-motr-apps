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
#*/


#sudo
SUDO = sudo
#or not
undefine SUDO

#files
FILE1 = './file1'
FILE2 = './file2'

#executables
EXE1 = c0cp
EXE2 = c0cat
EXE3 = c0rm

#c0cp parameters
#valid block sizes are: 4KB ~ 32MB
#4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 
#1048576, 2097152, 4194304, 8388608, 16777216, 33554432    
BSZ = 4096
CNT = 64

#compilar/linker options
LFLAGS += -lm -lpthread -lrt -lgf_complete -lyaml -luuid -lmero
CFLAGS += -D_REENTRANT -D_GNU_SOURCE -DM0_INTERNAL='' -DM0_EXTERN=extern
CFLAGS += -fno-common -Wall -Werror -Wno-attributes -fno-strict-aliasing 
CFLAGS += -fno-omit-frame-pointer -g -O2 -Wno-unused-but-set-variable 
CFLAGS += -rdynamic 

all: $(EXE1) $(EXE2) $(EXE3)
.PHONY: all

$(EXE1):
	gcc c0appz.c c0cp.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -o $(EXE1)

$(EXE2):
	gcc c0appz.c c0cat.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -o $(EXE2)

$(EXE3):
	gcc c0appz.c c0rm.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -o $(EXE3)


test: $(EXE1) $(EXE2) $(EXE3)
	$(SUDO) dd if=/dev/urandom of=$(FILE1) bs=$(BSZ) count=$(CNT)
	@echo "#####"
	@ls -lh $(FILE1)	
	$(SUDO) ./$(EXE1) 0 1048577 $(FILE1) $(BSZ) $(CNT)
	@echo "#####"
	$(SUDO) ./$(EXE2) 0 1048577 $(BSZ) $(CNT) > $(FILE2)
	@ls -lh $(FILE2)
	@echo "#####"
	cmp $(FILE1) $(FILE2) || echo "ERROR: Test Failed !!"
	@echo "#####"
	$(SUDO) ./$(EXE3) 0 1048577

c0fidgen:
	gcc -Wall -lssl -lcrypto c0fidgen.c -o c0fidgen

rcfile:
	./scripts/c0appzrcgen > ./.$(EXE1)rc
	./scripts/c0appzrcgen > ./.$(EXE2)rc
	./scripts/c0appzrcgen > ./.$(EXE3)rc

clean:
	rm -f $(EXE1) $(EXE2) $(EXE3) m0trace.*
	rm -f a.out c0fidgen
	rm -f $(FILE1) $(FILE2)
	
m0t1fs:
	touch /mnt/m0t1fs/0:3000
	setfattr -n lid -v 8 /mnt/m0t1fs/0:3000
	dd if=/dev/zero of=/mnt/m0t1fs/0:3000 bs=4M count=10
	ls -lh /mnt/m0t1fs/0:3000
	rm -rf /mnt/m0t1fs/0:3000


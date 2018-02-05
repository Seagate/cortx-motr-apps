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

#executables
EXE1 = c0cp
EXE2 = c0cat
EXE3 = c0rm

#c0cp parameters
BSZ = 4096
CNT = 24

#compilar/linker options
LFLAGS += -lm -lpthread -lrt -lgf_complete -lyaml -luuid -lmero
CFLAGS += -D_REENTRANT -D_GNU_SOURCE -DM0_INTERNAL='' -DM0_EXTERN=extern
CFLAGS += -fno-common -Wall -Werror -Wno-attributes -fno-strict-aliasing 
CFLAGS += -fno-omit-frame-pointer -g -O2 -Wno-unused-but-set-variable 
CFLAGS += -rdynamic 

all: $(EXE1) $(EXE2) $(EXE3)
.PHONY: all

$(EXE1):
	gcc capps.c c0cp.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -o $(EXE1)

$(EXE2):
	gcc capps.c c0cat.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -o $(EXE2)

$(EXE3):
	gcc capps.c c0rm.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -o $(EXE3)


test: $(EXE1) $(EXE2) $(EXE3)
	@echo $(IP)
	sudo dd if=/dev/urandom of=/tmp/8kFile bs=$(BSZ) count=$(CNT)
	@echo "#####"
	@ls -la /tmp/8kFile	
	sudo ./$(EXE1) 0 1048577 /tmp/8kFile $(BSZ) $(CNT)
	@echo "#####"
	sudo ./$(EXE2) 0 1048577 $(BSZ) $(CNT) > /tmp/8kFile_downloaded
	@ls -la /tmp/8kFile_downloaded
	@echo "#####"
	cmp /tmp/8kFile /tmp/8kFile_downloaded || echo "ERROR: Test Failed !!"
	@echo "#####"
	sudo ./$(EXE3) 0 1048577

clean:
	rm -f $(EXE1) $(EXE2) $(EXE3) m0trace.*


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


BSZ = 4096
CNT = 24
IP = $(shell echo $$(sudo lctl list_nids))

LFLAGS += -lm -lpthread -lrt -lgf_complete -lyaml -luuid -lmero
CFLAGS += -D_REENTRANT -D_GNU_SOURCE -DM0_INTERNAL='' -DM0_EXTERN=extern
CFLAGS += -fno-common -Wall -Werror -Wno-attributes -fno-strict-aliasing 
CFLAGS += -fno-omit-frame-pointer -g -O2 -Wno-unused-but-set-variable 
CFLAGS += -rdynamic 

all: c0cp c0cat c0del
.PHONY: all

c0cp:
	gcc capps.c c0cp.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -o c0cp

c0cat:
	gcc capps.c c0cat.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -o c0cat

c0del:
	gcc capps.c c0del.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -o c0del


test: c0cp c0cat c0del
	@echo $(IP)
	sudo dd if=/dev/urandom of=/tmp/8kFile bs=$(BSZ) count=$(CNT)
	@echo "#####"
	@ls -la /tmp/8kFile	
	sudo ./c0cp 0 1048577 /tmp/8kFile $(BSZ) $(CNT)
	@echo "#####"
	sudo ./c0cat 0 1048577 $(BSZ) $(CNT) > /tmp/8kFile_downloaded
	@ls -la /tmp/8kFile_downloaded
	@echo "#####"
	cmp /tmp/8kFile /tmp/8kFile_downloaded || echo "ERROR: Test Failed !!"
	@echo "#####"
	sudo ./c0del 0 1048577

clean:
	rm -f c0cp c0cat c0del m0trace.*


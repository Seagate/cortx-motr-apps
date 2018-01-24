
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


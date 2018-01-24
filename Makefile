
IP = $(shell echo $$(sudo lctl list_nids))

LFLAGS += -lm -lpthread -lrt -lgf_complete -lyaml -luuid -lmero
CFLAGS += -D_REENTRANT -D_GNU_SOURCE -DM0_INTERNAL='' -DM0_EXTERN=extern
CFLAGS += -fno-common -Wall -Werror -Wno-attributes -fno-strict-aliasing 
CFLAGS += -fno-omit-frame-pointer -g -O2 -Wno-unused-but-set-variable 
CFLAGS += -rdynamic 

all: c0cp c0cat c0del
.PHONY: all

c0cp:
	gcc c0cp.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -o c0cp

c0cat:
	gcc c2.c c0cat.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -o c0cat

c0del:
	gcc c2.c c0del.c -I/usr/include/mero $(CFLAGS) $(LFLAGS) -o c0del


test: c0cp c0cat
	@echo $(IP)
	sudo dd if=/dev/urandom of=/tmp/8kFile bs=4096 count=2
	@echo "---"
	@ls -la /tmp/8kFile	
	sudo ./c0cp $(IP):12345:44:101 $(IP):12345:45:1 '<0x7000000000000001:0>' '<0x7200000000000000:0>' /tmp/ 1048577 /tmp/8kFile 4096 2
	@echo "---"
#	sudo ./c0cat $(IP):12345:44:101 $(IP):12345:45:1 '<0x7000000000000001:0>' '<0x7200000000000000:0>' /tmp/ 1048577 4096 2 > /tmp/8kFile_downloaded
	sudo ./c0cat 0 1048577 4096 2 > /tmp/8kFile_downloaded
	@ls -la /tmp/8kFile_downloaded
	@echo "---"
	cmp /tmp/8kFile /tmp/8kFile_downloaded || echo "ERROR: Test Failed !!"
	@echo "---"
#	sudo ./c0del $(IP):12345:44:101 $(IP):12345:45:1 '<0x7000000000000001:0>' '<0x7200000000000000:0>' /tmp/ 1048577
	sudo ./c0del 0 1048577

clean:
	rm -f c0cp c0cat c0del m0trace.*



all: c0cp c0cat
.PHONY: all

c0cp:
	  gcc c0cp.c  -I/usr/include/mero -D_REENTRANT -D_GNU_SOURCE -DM0_INTERNAL='' -DM0_EXTERN=extern -fno-common -Wall -Werror -Wno-attributes -fno-strict-aliasing -fno-omit-frame-pointer -g -O2 -DM0_INTERNAL='' -DM0_EXTERN=extern -Wall -Werror -Wno-attributes -fno-omit-frame-pointer -g -O2 -Wno-unused-but-set-variable -rdynamic -lm -lpthread -lrt -lgf_complete -lyaml -luuid -lmero -o c0cp

c0cat:
	  gcc c0cat.c  -I/usr/include/mero -D_REENTRANT -D_GNU_SOURCE -DM0_INTERNAL='' -DM0_EXTERN=extern -fno-common -Wall -Werror -Wno-attributes -fno-strict-aliasing -fno-omit-frame-pointer -g -O2 -DM0_INTERNAL='' -DM0_EXTERN=extern -Wall -Werror -Wno-attributes -fno-omit-frame-pointer -g -O2 -Wno-unused-but-set-variable -rdynamic -lm -lpthread -lrt -lgf_complete -lyaml -luuid -lmero -o c0cat

test: c0cp c0cat
	    sudo dd if=/dev/urandom of=/tmp/8kFile bs=4096 count=2
	    sudo ./c0cp $(IP)@tcp:12345:44:101 $(IP)@tcp:12345:45:1 $(IP)@tcp:12345:44:101 '<0x7000000000000001:0>' '<0x7200000000000000:0>' /tmp/ 1048577 /tmp/8kFile 4096 2
	    sudo ./c0cat $(IP)@tcp:12345:44:101 $(IP)@tcp:12345:45:1 $(IP)@tcp:12345:44:101 '<0x7000000000000001:0>' '<0x7200000000000000:0>' /tmp/ 1048577 4096 2 > /tmp/8kFile_downloaded
	        @cmp /tmp/8kFile /tmp/8kFile_downloaded || echo "ERROR: Test Failed !!"

clean:
	  rm -f c0cp c0cat m0trace.*

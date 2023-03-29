/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 *
 * Original author:  Ganesan Umanesan <ganesan.umanesan@seagate.com>
 * Original creation date: 24-Jan-2017
 */

#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include "c0appz.h"
#include "c0appz_internal.h"
#include "dir.h"


/*
 ******************************************************************************
 * EXTERN VARIABLES
 ******************************************************************************
 */
extern int perf;	/* performance 		*/
int force=0; 		/* overwrite  		*/
extern uint64_t qos_whgt_served;
extern uint64_t qos_whgt_remain;
extern uint64_t qos_laps_served;
extern uint64_t qos_laps_remain;
extern pthread_mutex_t qos_lock;	/* lock  qos_total_weight */

char *prog;

const char *help_c0cp_txt = "\
Usage:\n\
  c0cp [-fptv] [-x id] [-u sz] [-b [sz]] [-c n] [-a n] idh idl file bsz\n\
  c0cp 1234 56789 file 64\n\
\n\
Copy file to the object store.\n\
\n\
idh - object id high number\n\
idl - object id low  number\n\
bsz - block size (in KiBs)\n\
\n\
  -a n    async mode: do n object store operations in a batch\n\
  -b [sz] block size for the object store i/o (m0bs);\n\
            if sz is not specified, automatically figure out the optimal\n\
            value based on the object size and the pool width (does not\n\
            work for the composite objects atm); (by default, use bsz)\n\
  -c n    write n contiguous copies of the file into the object\n\
  -m      use multiple threads\n\
  -f      force: overwrite an existing object\n\
  -p      show performance stats\n\
  -u sz   unit size (in KiBs), must be power of 2, >= 4 and <= 4096;\n\
            by default, determined automatically for the new objects\n\
            based on the m0bs and parity configuration of the pool\n\
  -x id   id of the tier (pool) to create the object in (1,2,3,..)\n\
  -t      create m0trace.pid file\n\
  -v      be more verbose\n\
  -i | n socket index, [0-3] currently\n\
  -h      print this help\n\
\n\
Note: in order to get the maximum performance, m0bs should be multiple\n\
of the data size in the parity group, i.e. multiple of (unit_size * n),\n\
where n is 2 in 2+1 parity group configuration, 8 in 8+2 and so on.\n";

int help()
{
	fprintf(stderr, "%s\n%s\n", help_c0cp_txt, c0appz_help_txt);
	exit(1);
}

int main(int argc, char **argv)
{
	uint64_t	idh;	/* object id high    		*/
	uint64_t 	idl;    /* object is low     		*/
	uint64_t 	bsz;   	/* block size        		*/
	uint64_t 	m0bs=0;	/* m0 block size     		*/
	uint64_t 	cnt;   	/* count             		*/
	uint64_t 	pos=0;	/* starting position 		*/
	char    	*fbuf;	/* file buffer       		*/
	int      	rc;		/* return code       		*/
	uint64_t	j;
	uint64_t	k;

	prog = basename(strdup(argv[0]));

	idh = 11;
	idl = 11;
	//bsz = atoi(argv[1]);
	//cnt = atoi(argv[2]);
	bsz = 64;
	cnt = 4*1024 + 256;
	m0bs = 1;

	bsz *= 1024;	/* bsz in KB 		*/
	m0bs = 1;		/* m0bs in bsz 		*/
	perf = 1;		/* show bandwidth	*/
	force= 1;		/* overwrite object	*/

	c0appz_setrc(prog);
	c0appz_putrc();

	/* initialise */
	rc = c0appz_init(0);
	if (rc != 0) {
		fprintf(stderr,"%s(): error: c0appz_init() failed: %d\n",
			__func__, rc);
		return 222;
	}

	/* create object */
	rc = c0appz_cr(idh, idl, 0, m0bs*bsz);
	if (rc < 0 || (rc != 0 && !force)) {
		if (rc < 0)
			fprintf(stderr,"%s(): object create failed: rc=%d\n",
				__func__, rc);
		rc = 333;
		goto end;
	}

	printf("bsz=%" PRIu64 " cnt=%" PRIu64 " m0bs=%" PRIu64 "\n" ,bsz,cnt,m0bs);

	/* QOS start */
	qos_whgt_served = 0;
	qos_whgt_remain = bsz * cnt;
	qos_laps_served = 0;
	qos_laps_remain = 0;
	qos_pthread_start();
	c0appz_timein();

	k = cnt%m0bs;
	printf("%lu\n",k);

	if(k) {
		printf("OK\n");
		/* write */
		fbuf = malloc(k*bsz);
		rc = c0appz_mw(fbuf, idh, idl, pos, bsz, k, k*bsz);
		if (rc != 0) {
			ERR("copying failed at pos %lu: %d\n", pos, rc);
		}
		pos += k*bsz;
		free(fbuf);
	}

	/* write */
	fbuf = malloc(m0bs*bsz);
	for(j=0; j<(cnt-k)/m0bs; j++) {
		rc = c0appz_mw(fbuf, idh, idl, pos, bsz, m0bs, m0bs*bsz);
		if (rc != 0) {
			ERR("copying failed at pos %lu: %d\n", pos, rc);
		}
		pos += m0bs*bsz;
	}
	free(fbuf);

	/* QOS stop */
	ppf("%8s","write");
	c0appz_timeout(pos);
	qos_pthread_wait();
	c0appz_dump_perf();
	c0appz_clear_perf();
	sleep(2);



end:
	/* free */
	c0appz_free();

	/* failure */
	if (rc != 0) {
		fprintf(stderr,"[%d] %s failed!\n", rc, prog);
		return rc;
	}

	/* success */
	c0appz_dump_perf();
	printf("Total %" PRIu64 " bytes\n",bsz*cnt);
	fprintf(stderr,"%s success\n", prog);
	return 0;
}

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */

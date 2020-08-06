/* -*- C -*- */
/*
 * COPYRIGHT 2014 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE LLC,
 * ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.xyratex.com/contact
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
#include "c0appz.h"
#include "c0appz_internal.h"

/*
 ******************************************************************************
 * EXTERN VARIABLES
 ******************************************************************************
 */
extern int perf; 	/* performance 		*/
int force=0; 		/* overwrite  		*/
extern uint64_t qos_whgt_served;
extern uint64_t qos_whgt_remain;
extern uint64_t qos_laps_served;
extern uint64_t qos_laps_remain;
extern pthread_mutex_t qos_lock;       /* lock  qos_total_weight */
extern struct m0_fid *pool_fid;

char *prog;

const char *help_c0cp_txt = "\
Usage:\n\
\n\
c0cp [options] idh idl filename bsz\n\
c0cp 1234 56789 256KiB-file 64\n\
\n\
idh - Mero fid high\n\
idl - Mero fid low\n\
bsz - Clovis block size (in KiBs)\n\
\n\
options are:\n\
	-f | force\n\
	-p | performance\n\
	-c | contiguous mode\n\
	   | -c <n> write <n> contiguous copies of the file. \n\
	-x | pool ID. \n\
	   | -x <n> creates the object in pool with ID <n>. \n\
	-u | unit size (in KiBs), must be power of 2, >= 4 and <= 4096.\n\
           | By default, determined automatically based on the bsz and\n\
           | parity configuration of the pool.\n\
        -v | be more verbose\n\
\n\
The -f option forces rewriting on an object if that object already exists. \n\
It creates a new object if the object does not exist.\n\
c0cp -f 1234 56789 256MiB-file 8192 -u 1024\n\
\n\
The -p option enables performance monitoring. It collects performance stats\n\
(such as bandwidth) and displays them at the end of the execution. It also\n\
outputs realtime storage bandwith consumption.\n\
c0cp -p 1234 56789 256MiB-file 8192 -u 1024\n\
\n\
The -c <n> option takes <n> a positive number as an argument and creates an \n\
object with <n> contiguous copies of the same file.\n\
c0cp -c 10 1234 56789 256MiB-file 8192 -u 1024\n\
\n\
The -x option takes <n> a positive number as an argument and selects the pool\n\
with ID <n> for creating the object. Without this option objects are created\n\
in the default pool.\n\
c0cp -x 3 1234 56789 256MiB-file 8192 -u 1024\n\
\n\
Note: in order to get the maximum performance, block_size should be multiple\n\
of data units in the parity group, i.e. multiple of unit_size * N, where\n\
N is the number of data units in the parity group configuration of the pool.\n\
For example, with 8+2 parity group configuration (8 data + 2 parity units\n\
in the group) and 1MiB unit size - the block_size should be multiple of 8MiB.";

int help()
{
	fprintf(stderr, "%s\n", help_c0cp_txt);
	exit(1);
}

int main(int argc, char **argv)
{
	uint64_t idh;        /* object id high    */
	uint64_t idl;        /* object is low     */
	uint64_t bsz;        /* block size        */
	uint64_t m0bs;       /* m0 block size     */
	uint64_t cnt;        /* count             */
	uint64_t pos;        /* starting position */
	char    *fname;      /* input filename    */
	struct stat64 fs;    /* file statistics   */
	int      opt;        /* options           */
	int      rc;         /* return code       */
	char    *fbuf;       /* file buffer       */
	int      cont=0;     /* continuous mode   */
	int      pool=0;     /* default pool ID   */

	prog = basename(strdup(argv[0]));

	/* getopt */
	while((opt = getopt(argc, argv, ":pfc:x:u:v"))!=-1){
		switch(opt){
		case 'p':
			perf = 1;
			break;
		case 'f':
			force = 1;
			break;
		case 'c':
			cont = atoi(optarg);
			if(cont<0) cont=0;
			if(!cont) help();
			break;
		case 'u':
			if (sscanf(optarg, "%i", &unit_size) != 1) {
				fprintf(stderr, "invalid unit size\n");
				help();
			}
			break;
		case 'x':
			pool = atoi(optarg);
			if(!isdigit(optarg[0])) pool = -1;
			if((pool<0)||(pool>3)) help();
			break;
		case 'v':
			trace_level++;
			break;
		case ':':
			fprintf(stderr,"option %c needs a value\n",optopt);
			help();
			break;
		case '?':
			fprintf(stderr,"unknown option: %c\n", optopt);
			help();
			break;
		default:
			fprintf(stderr,"unknown option: %c\n", optopt);
			help();
			break;
		}
	}

	/* check input */
	if(argc-optind!=4){
		help();
	}

	/* c0rcfile
	 * overwrite .cappzrc to a .[app]rc file.
	 */
	char str[256];
	sprintf(str,"%s/.%src", dirname(argv[0]), prog);
	c0appz_setrc(str);
	c0appz_putrc();

	/* set input */
	idh = atoll(argv[optind+0]);
	idl = atoll(argv[optind+1]);
	fname = argv[optind+2];
	if (sscanf(argv[optind+3], "%li", &bsz) != 1) {
		ERR("invalid block size - %s", argv[optind+3]);
		help();
	}
	bsz *= 1024;

	if (bsz == 0) {
		fprintf(stderr,"%s: bsz must be > 0\n", prog);
		help();
	}

	rc = stat64(fname, &fs);
	if (rc != 0) {
		ERRS("%s", fname);
		exit(1);
	}
	cnt = (fs.st_size + bsz - 1) / bsz;

	/* init */
	c0appz_timein();
	if (c0appz_init(0) != 0) {
		fprintf(stderr,"%s(): error: clovis initialisation failed.\n",
			__func__);
		return 222;
	}
	ppf("%8s","init");
	c0appz_timeout(0);

	/* pool */
	if (pool != 0) {
		c0appz_pool_ini();
		c0appz_pool_set(pool-1);
	}

	m0bs = c0appz_m0bs(bsz * cnt, pool_fid);
	if (!m0bs) {
		fprintf(stderr,"%s(): error: c0appz_m0bs() failed.\n",
			__func__);
		rc = 222;
		goto end;
	}

	/* create object */
	c0appz_timein();
	rc = c0appz_cr(idh, idl, pool_fid, m0bs);
	if (rc < 0 || (rc != 0 && !force)) {
		if (rc < 0)
			fprintf(stderr,"%s(): object create failed: rc=%d\n",
				__func__, rc);
		rc = 333;
		goto end;
	}
	ppf("%8s","create");
	c0appz_timeout(0);

	/* continuous write */
	if (cont) {
		fbuf = malloc(fs.st_size + bsz - 1);
		if (!fbuf) {
			fprintf(stderr,"%s(): error: not enough memory.\n",
				__func__);
			rc = 111;
			goto end;
		}
		rc = c0appz_fr(fbuf, fname, bsz, cnt);
		if (rc != 0) {
			fprintf(stderr,"%s(): c0appz_fr failed: rc=%d\n",
				__func__, rc);
			rc = 555;
			goto end;
		}

		qos_whgt_served = 0;
		qos_whgt_remain = bsz * cnt * cont;
		qos_laps_served = 0;
		qos_laps_remain = cont;
		qos_pthread_start();
		c0appz_timein();

		for (pos = 0; cont > 0; cont--, pos += cnt * bsz) {
			rc = c0appz_mw(fbuf, idh, idl, pos, bsz, cnt, m0bs);
			if (rc != 0) {
				fprintf(stderr, "%s: c0appz_mw() failed"
					" at pos %lu: %d\n", prog, pos, rc);
				break;
			}
			pthread_mutex_lock(&qos_lock);
			qos_laps_served++;
			qos_laps_remain--;
			pthread_mutex_unlock(&qos_lock);
		}

		ppf("%8s","write");
		c0appz_timeout(pos);
		qos_pthread_wait();
		printf("%" PRIu64 " x %" PRIu64 " = %" PRIu64 "\n", cnt, bsz,
		       cnt * bsz);
		free(fbuf);
		goto end;
	}

	qos_whgt_served = 0;
	qos_whgt_remain = bsz * cnt;
	qos_laps_served = 0;
	qos_laps_remain = 1;
	qos_pthread_start();
	c0appz_timein();

	/* copy */
	rc = c0appz_cp(idh, idl, fname, bsz, cnt, m0bs);
	if (rc != 0) {
		fprintf(stderr,"%s(): object copying failed: rc=%d\n",
			__func__, rc);
		rc = 222;
		goto end;
	};
	pthread_mutex_lock(&qos_lock);
	qos_laps_served++;
	qos_laps_remain--;
	pthread_mutex_unlock(&qos_lock);
	ppf("%8s","copy");
	c0appz_timeout(bsz*cnt);
	qos_pthread_wait();

end:
	/* free */
	c0appz_timein();
	c0appz_free();
	ppf("%8s","free");
	c0appz_timeout(0);

	/* failure */
	if (rc != 0) {
		printf("%s %" PRIu64 "\n",fname,fs.st_size);
		fprintf(stderr,"%s failed!\n", prog);
		return rc;
	}

	/* success */
	c0appz_dump_perf();
	printf("%s %" PRIu64 "\n",fname,fs.st_size);
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

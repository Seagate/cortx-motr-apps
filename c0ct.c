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
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include <assert.h>
#include "c0appz.h"
#include "c0appz_internal.h"

/*
 ******************************************************************************
 * EXTERN VARIABLES
 ******************************************************************************
 */
extern int perf; /* performance */
extern uint64_t qos_whgt_served;
extern uint64_t qos_whgt_remain;
extern uint64_t qos_laps_served;
extern uint64_t qos_laps_remain;
extern pthread_mutex_t qos_lock;	/* lock  qos_total_weight */
extern struct m0_fid *pool_fid;

char *prog;

const char *help_c0ct_txt = "\
Usage:\n\
  c0ct [-ptv] [-b [sz]] [-c n] idh idl filename bsz fsz\n\
  c0ct 1234 56789 file 1024 268435456\n\
\n\
Copy object from the object store into the file (at filename).\n\
\n\
idh - object id high number\n\
idl - object id low  number\n\
bsz - block size (in KiBs)\n\
fsz - file size (in bytes)\n\
\n\
  -b [sz] block size for the object store i/o (m0bs);\n\
            if sz is not specified, automatically figure out the optimal\n\
            value based on the object size and the pool width (does not\n\
            work for the composite objects atm); (by default, use bsz)\n\
  -c n    read n contiguous copies of the file from the object\n\
  -p      show performance stats\n\
  -t      create m0trace.pid file\n\
  -v      be more verbose\n\
\n\
Note: in order to get the maximum performance, m0bs should be multiple\n\
of the data size in the parity group, i.e. multiple of (unit_size * n),\n\
where n is 2 in 2+1 parity group configuration, 8 in 8+2 and so on.\n";

int help()
{
	fprintf(stderr,"%s\n", help_c0ct_txt);
	exit(1);
}

int main(int argc, char **argv)
{
	uint64_t idh;	/* object id high		*/
	uint64_t idl;   /* object id low  		*/
	uint64_t bsz;   /* block size     		*/
	uint64_t m0bs=0; /* m0 block size     		*/
	uint64_t cnt;   /* count          		*/
	uint64_t pos;	/* starting position	*/
	uint64_t fsz;   /* file size			*/
	char *fname;	/* output filename 		*/
	char *fbuf=NULL;/* file buffer			*/
	int opt=0;		/* options				*/
	int cont=0; 	/* continuous mode 		*/
	int laps=0;		/* number of reads		*/
	int rc=0;		/* return code			*/

	prog = basename(strdup(argv[0]));

	/* getopt */
	while((opt = getopt(argc, argv, ":b:pc:tv"))!=-1){
		switch(opt){
		case 'b':
			if (sscanf(optarg, "%li", &m0bs) != 1) {
				/* optarg might contain some other options */
				optind--; /* rewind */
				m0bs = 1; /* get automatically */
			} else
				m0bs *= 1024;
			break;
		case 'p':
			perf = 1;
			break;
		case 'c':
			cont = 1;
			cont = atoi(optarg);
			if(cont<0) cont=0;
			if(!cont) help();
			break;
		case 't':
			m0trace_on = true;
			break;
		case 'v':
			trace_level++;
			break;
		case ':':
			switch (optopt) {
			case 'b':
				m0bs = 1; /* get automatically */
				break;
			default:
				ERR("option %c needs a value\n", optopt);
				help();
			}
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
	if(argc-optind!=5){
		help();
	}

	/* time in */
	c0appz_timein();

	/* c0rcfile
	 * overwrite .cappzrc to a .[app]rc file.
	 */
	char str[256];
	sprintf(str,"%s/.%src", dirname(argv[0]), prog);
	c0appz_setrc(str);
	c0appz_putrc();

	/* set input */
	if (sscanf(argv[optind+0], "%li", &idh) != 1) {
		ERR("invalid idhi - %s", argv[optind+0]);
		help();
	}
	if (sscanf(argv[optind+1], "%li", &idl) != 1) {
		ERR("invalid idlo - %s", argv[optind+1]);
		help();
	}
	fname = argv[optind+2];
	if (sscanf(argv[optind+3], "%li", &bsz) != 1) {
		ERR("invalid block size value: %s\n", argv[optind+3]);
		help();
	}
	bsz *= 1024;
	if (sscanf(argv[optind+4], "%li", &fsz) != 1) {
		ERR("invalid file size value: %s\n", argv[optind+4]);
		help();
	}
	DBG("bsz=%ld fsz=%ld\n", bsz, fsz);
	cnt = (fsz+bsz-1)/bsz;
	assert(bsz>0);
	assert(!(fsz>cnt*bsz));

	/* init */
	c0appz_timein();
	if (c0appz_init(0) != 0) {
		fprintf(stderr,"error! clovis initialization failed.\n");
		return 222;
	}
	ppf("%8s","init");
	c0appz_timeout(0);

	/* check */
	c0appz_timein();
	if (!c0appz_ex(idh, idl, NULL)) {
		fprintf(stderr,"%s(): error!\n",__FUNCTION__);
		fprintf(stderr,"%s(): object NOT found!!\n",__FUNCTION__);
		rc = 777;
		goto end;
	}
	ppf("%8s","check");
	c0appz_timeout(0);

	if (m0bs == 1) {
		m0bs = c0appz_m0bs(idh, idl, bsz * cnt, pool_fid);
		if (!m0bs)
			LOG("WARNING: failed to figure out the optimal m0bs,"
			    " will use bsz for m0bs\n");
	}
	if (!m0bs)
		m0bs = bsz;
	DBG("m0bs=%lu\n", m0bs);

	/* continuous read */
	if(cont){
		fbuf = malloc(cnt*bsz);
		if(!fbuf){
			fprintf(stderr,"%s(): error!\n",__FUNCTION__);
			fprintf(stderr,"%s(): not enough memory!!\n",__FUNCTION__);
			rc = 333;
			goto end;
		}
		laps=cont;
		qos_whgt_served=0;
		qos_whgt_remain=bsz*cnt*laps;
		qos_laps_served=0;
		qos_laps_remain=laps;
		qos_pthread_start();
		c0appz_timein();
		while(cont>0){
			pos = (laps-cont)*cnt*bsz;
			c0appz_mr(fbuf, idh, idl, pos, bsz, cnt, m0bs);
			cont--;
			pthread_mutex_lock(&qos_lock);
			qos_laps_served++;
			qos_laps_remain--;
			pthread_mutex_unlock(&qos_lock);
		}
		ppf("%8s","read");
		c0appz_timeout(bsz*cnt*laps);
		qos_pthread_wait();
		fprintf(stderr,"writing to file...\n");
		c0appz_timein();
		if(c0appz_fw(fbuf,fname,bsz,cnt)!=0){
			fprintf(stderr,"%s(): c0appz_fw failed!!\n",__FUNCTION__);
			rc = 444;
			goto end;
		}
		ppf("%8s","fwrite");
		c0appz_timeout(bsz*cnt);
		printf("%" PRIu64 " x %" PRIu64 " = %" PRIu64 "\n",cnt,bsz,cnt*bsz);
		free(fbuf);
		goto end;
	}

	/* cat */
	qos_whgt_served=0;
	qos_whgt_remain=bsz*cnt;
	qos_laps_served=0;
	qos_laps_remain=1;
	qos_pthread_start();
	c0appz_timein();
	if (c0appz_ct(idh, idl, fname, bsz, cnt, m0bs) != 0) {
		fprintf(stderr,"%s(): error!\n",__FUNCTION__);
		fprintf(stderr,"%s(): cat object failed!!\n",__FUNCTION__);
		rc = 555;
		goto end;

	};
	pthread_mutex_lock(&qos_lock);
	qos_laps_served++;
	qos_laps_remain--;
	pthread_mutex_unlock(&qos_lock);
	ppf("%8s","cat");
	c0appz_timeout(bsz*cnt);
	qos_pthread_wait();

end:

//	qos_pthread_stop(rc);

	/* resize */
	truncate64(fname,fsz);

	/* free */
	c0appz_timein();
	c0appz_free();
	ppf("%8s","free");
	c0appz_timeout(0);

	/* failure */
	if(rc){
		printf("%s %" PRIu64 "\n",fname,fsz);
		fprintf(stderr,"%s failed!\n", prog);
		return rc;
	}

	/* success */
	c0appz_dump_perf();
	printf("%s %" PRIu64 "\n",fname,fsz);
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

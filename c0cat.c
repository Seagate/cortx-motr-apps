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
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include <assert.h>
#include "c0appz.h"
#include "c0appz_internal.h"


struct Block {
	uint64_t idh;	/* object id high    	*/
	uint64_t idl;   /* object is low     	*/
	uint64_t bsz;   /* block size        	*/
	uint64_t cnt;  	/* count             	*/
	uint64_t m0bs;	/* m0 block size     	*/
	uint64_t pos;   /* starting position 	*/
	char    *fbuf;  /* file buffer     		*/
};

/* threads */
pthread_t wthrd[512];			/* max 512 threads */
struct Block wthrd_blk[512];	/* max 512 threads */

static void *tfunc_c0appz_mr(void *block)
{
	struct Block *b = (struct Block *)block;
	/*
	printf("pos = %" PRIu64 "\n", b->pos);
	printf("idh = %" PRIu64 "\n", b->idh);
	printf("idl = %" PRIu64 "\n", b->idl);
	*/
	c0appz_mr(b->fbuf, b->idh, b->idl, b->pos, b->bsz, b->cnt, b->m0bs);
    return NULL;
}

void pack(int idx, char *fbuf, uint64_t idh, uint64_t idl, uint64_t pos,
		uint64_t bsz, uint64_t cnt, uint64_t m0bs)
{
	wthrd_blk[idx].idh = idh;
	wthrd_blk[idx].idl = idl;
	wthrd_blk[idx].pos = pos;
	wthrd_blk[idx].bsz = bsz;
	wthrd_blk[idx].cnt = cnt;
	wthrd_blk[idx].m0bs = m0bs;
	wthrd_blk[idx].fbuf = (char *)fbuf;
	/*
	printf("\n#####\n");
	printf("idh = %" PRIu64 "\n", wthrd_blk[idx].idh);
	printf("idl = %" PRIu64 "\n", wthrd_blk[idx].idl);
	printf("pos = %" PRIu64 "\n", wthrd_blk[idx].pos);
	printf("bsz = %" PRIu64 "\n", wthrd_blk[idx].bsz);
	printf("cnt = %" PRIu64 "\n", wthrd_blk[idx].cnt);
	*/
	return;
}


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

char *prog;

const char *help_c0cat_txt = "\
Usage:\n\
  c0cat [-ptv] [-b [sz]] [-c n] idh idl file bsz fsz\n\
  c0cat 1234 56789 file 1024 268435456\n\
\n\
Copy object from the object store into file.\n\
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
  -i | n socket index, [0-3] currently\n\
  -h      print this help\n\
\n\
Note: in order to get the maximum performance, m0bs should be multiple\n\
of the data size in the parity group, i.e. multiple of (unit_size * n),\n\
where n is 2 in 2+1 parity group configuration, 8 in 8+2 and so on.\n";

int help()
{
	fprintf(stderr,"%s\n%s\n", help_c0cat_txt, c0appz_help_txt);
	exit(1);
}

int main(int argc, char **argv)
{
	uint64_t idh;		/* object id high		*/
	uint64_t idl;   	/* object id low  		*/
	uint64_t bsz;   	/* block size     		*/
	uint64_t cnt;   	/* count          		*/
	uint64_t pos;		/* starting position	*/
	uint64_t fsz;   	/* file size			*/
	char *fname;		/* output filename 		*/
	char *fbuf=NULL;	/* file buffer			*/
	int opt=0;			/* options				*/
	int cont=0; 		/* continuous mode 		*/
	int laps=0;			/* number of reads		*/
	int rc=0;			/* return code			*/
	uint64_t m0bs=0;	/* m0 block size    	*/
	int	mthrd=0;	 	/* multi-threaded 	  	*/
	int idx=0;			/* socket index			*/

	int i;

	prog = basename(strdup(argv[0]));

	/* getopt */
	while ((opt = getopt(argc, argv, ":i:b:pmc:tvh")) != -1) {
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
		case 'm':
			mthrd = 1;
			break;
		case 't':
			m0trace_on = true;
			break;
		case 'v':
			trace_level++;
			break;
		case 'i':
			idx = atoi(optarg);
			if (idx < 0 || idx > 3) {
				ERR("invalid socket index: %s (allowed \n"
				    "values are 0, 1, 2, or 3 atm)\n", optarg);
				help();
			}
			break;
		case 'h':
			help();
			break;
		case ':':
			switch (optopt) {
			case 'b':
				m0bs = 1; /* get automatically */
				break;
			default:
				ERR("option -%c needs a value\n", optopt);
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
	if (argc-optind != 5)
		help();

	/* time in */
	c0appz_timein();

	c0appz_setrc(prog);
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
	rc = c0appz_init(idx);
	if (rc != 0) {
		fprintf(stderr,"error! c0appz_init() failed: %d\n", rc);
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
		m0bs = c0appz_m0bs(idh, idl, bsz * cnt, 0);
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
		if(!mthrd) {
			while(cont>0){
				pos = (laps-cont)*cnt*bsz;
				c0appz_mr(fbuf, idh, idl, pos, bsz, cnt, m0bs);
				cont--;
				pthread_mutex_lock(&qos_lock);
				qos_laps_served++;
				qos_laps_remain--;
				pthread_mutex_unlock(&qos_lock);
			}
		}
		else {
			pos = 0;
			for(i=0; i<cont; i++) {
				pack(i,fbuf, idh, idl, pos, bsz, cnt,m0bs);
				pthread_create(&(wthrd[i]),NULL,&tfunc_c0appz_mr,(void *)(wthrd_blk+i));
				pos += cnt * bsz;
			}
		}

		for(i=0; i<cont; i++) {
			pthread_join(wthrd[i],NULL);
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
	if (c0appz_cat(idh, idl, fname, bsz, cnt, m0bs) != 0) {
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

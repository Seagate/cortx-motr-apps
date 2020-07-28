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
#include "help.h"

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
extern pthread_mutex_t qos_lock;	/* lock  qos_total_weight 		*/
extern struct m0_fid *pool_fid;

/*
 * help()
 */
int help()
{
	fprintf(stderr,"%s",(const char*)help_c0ct_txt);
	fprintf(stderr,"\n");
	exit(1);
}

/* main */
int main(int argc, char **argv)
{
	uint64_t idh;	/* object id high		*/
	uint64_t idl;   /* object id low  		*/
	uint64_t bsz;   /* block size     		*/
	uint64_t m0bs;  /* m0 block size     		*/
	uint64_t cnt;   /* count          		*/
	uint64_t pos;	/* starting position	*/
	uint64_t fsz;   /* file size			*/
	char *fname;	/* output filename 		*/
	char *fbuf=NULL;/* file buffer			*/
	int opt=0;		/* options				*/
	int cont=0; 	/* continuous mode 		*/
	int laps=0;		/* number of reads		*/
	int rc=0;		/* return code			*/

	/* getopt */
	while((opt = getopt(argc, argv, ":pc:"))!=-1){
		switch(opt){
			case 'p':
				perf = 1;
				break;
			case 'c':
				cont = 1;
				cont = atoi(optarg);
				if(cont<0) cont=0;
				if(!cont) help();
				break;
			case ':':
				fprintf(stderr,"option needs a value\n");
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
	if(argc-optind!=5){
		help();
	}

	/* time in */
	c0appz_timein();

	/* c0rcfile
	 * overwrite .cappzrc to a .[app]rc file.
	 */
	char str[256];
	sprintf(str,".%src",basename(argv[0]));
	c0appz_setrc(str);
	c0appz_putrc();

	/* set input */
	idh	= atoll(argv[optind+0]);
	idl = atoll(argv[optind+1]);
	fname = argv[optind+2];
	bsz = atoll(argv[optind+3]) * 1024;
	fsz = atoll(argv[optind+4]);
	cnt = (fsz+bsz-1)/bsz;
	assert(bsz>0);
	assert(!(fsz>cnt*bsz));

	/* init */
	c0appz_timein();
	if(c0appz_init(0)!=0){
		fprintf(stderr,"error! clovis initialization failed.\n");
		return 222;
	}
	ppf("%8s","init");
	c0appz_timeout(0);

	/* check */
	c0appz_timein();
	if(!(c0appz_ex(idh,idl))){
		fprintf(stderr,"%s(): error!\n",__FUNCTION__);
		fprintf(stderr,"%s(): object NOT found!!\n",__FUNCTION__);
		rc = 777;
		goto end;
	}
	ppf("%8s","check");
	c0appz_timeout(0);

	m0bs = c0appz_m0bs(bsz * cnt, pool_fid);
	if (!m0bs) {
		fprintf(stderr,"%s(): error: c0appz_m0bs() failed.\n",
			__func__);
		exit(1);
	}

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
		fprintf(stderr,"%s failed!\n",basename(argv[0]));
		return rc;
	}

	/* success */
	c0appz_dump_perf();
	printf("%s %" PRIu64 "\n",fname,fsz);
	fprintf(stderr,"%s success\n",basename(argv[0]));
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

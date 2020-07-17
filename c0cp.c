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
#include <ctype.h>
#include "c0appz.h"
#include "help.h"

/*
 ******************************************************************************
 * EXTERN VARIABLES
 ******************************************************************************
 */
extern int perf; 	/* performance 		*/
int force=0; 		/* overwrite  		*/
int cont=0; 		/* continuous mode 	*/
extern uint64_t qos_whgt_served;
extern uint64_t qos_whgt_remain;
extern uint64_t qos_laps_served;
extern uint64_t qos_laps_remain;
extern pthread_mutex_t qos_lock;	/* lock  qos_total_weight 		*/

/*
 * help()
 */
int help()
{
	fprintf(stderr,"%s",(const char*)help_c0cp_txt);
	fprintf(stderr,"\n");
	exit(1);
}

/* main */
int main(int argc, char **argv)
{
	uint64_t idh;        /* object id high    */
	uint64_t idl;        /* object is low     */
	uint64_t bsz;        /* block size        */
	uint64_t cnt;        /* count             */
	uint64_t pos;        /* starting position */
	uint64_t fsz;        /* initial file size */
	char *fname;         /* input filename    */
	struct stat64 fs;    /* file statistics   */
	int opt;             /* options           */
	int rc=0;            /* return code       */
	char *fbuf=NULL;     /* file buffer       */
	int laps=0;          /* number of writes  */
	int pool=0;          /* default pool ID   */

	/* getopt */
	while((opt = getopt(argc, argv, ":pfc:x:u:"))!=-1){
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
	sprintf(str,".%src",basename(argv[0]));
	c0appz_setrc(str);
	c0appz_putrc();

	/* set input */
	idh = atoll(argv[optind+0]);
	idl = atoll(argv[optind+1]);
	fname = argv[optind+2];
	bsz = atoll(argv[optind+3]) * 1024;
	assert(bsz>0);

	/* extend */
	stat64(fname, &fs);
	fsz = fs.st_size;
	cnt = (fs.st_size + bsz - 1)/bsz;
	truncate64(fname,fs.st_size + bsz - 1);
	assert(!(fsz>cnt*bsz));

	/* init */
	c0appz_timein();
	if (c0appz_init(0) != 0) {
		fprintf(stderr,"%s(): error: clovis initialisation failed.\n",
			__func__);
		truncate64(fname,fs.st_size);
		stat64(fname,&fs);
		assert(fsz==fs.st_size);
		return 222;
	}
	ppf("%8s","init");
	c0appz_timeout(0);

	/* pool */
	if (pool != 0) {
		c0appz_pool_ini();
		c0appz_pool_set(pool-1);
	}

	/* create object */
	c0appz_timein();
	rc = c0appz_cr(idh, idl);
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

		truncate64(fname,fs.st_size);
		stat64(fname,&fs);
		assert(fsz==fs.st_size);
		laps=cont;
		qos_whgt_served=0;
		qos_whgt_remain=bsz*cnt*laps;
		qos_laps_served=0;
		qos_laps_remain=laps;
		qos_pthread_start();
		c0appz_timein();
		while (cont > 0) {
			pos = (laps-cont)*cnt*bsz;
			c0appz_mw(fbuf,idh,idl,pos,bsz,cnt);
			cont--;
			pthread_mutex_lock(&qos_lock);
			qos_laps_served++;
			qos_laps_remain--;
			pthread_mutex_unlock(&qos_lock);
		}
		ppf("%8s","write");
		c0appz_timeout(bsz*cnt*laps);
		qos_pthread_wait();
		printf("%" PRIu64 " x %" PRIu64 " = %" PRIu64 "\n",cnt,bsz,cnt*bsz);
		free(fbuf);
		goto end;
	}

	qos_whgt_served=0;
	qos_whgt_remain=bsz*cnt;
	qos_laps_served=0;
	qos_laps_remain=1;
	qos_pthread_start();
	c0appz_timein();
	/* copy */
	rc = c0appz_cp(idh, idl, fname, bsz, cnt);
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

	/* resize */
	truncate64(fname,fs.st_size);
	stat64(fname,&fs);
	assert(fsz==fs.st_size);

	/* free */
	c0appz_timein();
	c0appz_free();
	ppf("%8s","free");
	c0appz_timeout(0);

	/* failure */
	if(rc){
		printf("%s %" PRIu64 "\n",fname,fs.st_size);
		fprintf(stderr,"%s failed!\n",basename(argv[0]));
		return rc;
	}

	/* success */
	c0appz_dump_perf();
	printf("%s %" PRIu64 "\n",fname,fs.st_size);
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

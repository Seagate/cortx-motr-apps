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

/*
 ******************************************************************************
 * EXTERN VARIABLES
 ******************************************************************************
 */
extern int perf; 	/* performance 		*/
int force=0; 		/* overwrite  		*/
int cont=0; 		/* continuous mode 	*/
extern uint64_t qos_served;
extern uint64_t qos_remain;
extern uint64_t qos_laps_served;
extern uint64_t qos_laps_remain;

/* main */
int main(int argc, char **argv)
{
	uint64_t idh;		/* object id high 		*/
	uint64_t idl;		/* object is low		*/
	uint64_t bsz;		/* block size 			*/
	uint64_t cnt;		/* count				*/
	uint64_t pos;		/* starting position	*/
	uint64_t fsz;		/* initial file size	*/
	char *fname;		/* input filename 		*/
	struct stat64 fs;	/* file statistics		*/
	int opt;			/* options				*/
	int rc=0;			/* return code			*/
	char *fbuf=NULL;	/* file buffer			*/
	int laps=0;			/* number of writes		*/
	int pool=-111;		/* pool id - default 	*/

	/* getopt */
	while((opt = getopt(argc, argv, ":pfc:x:"))!=-1){
		switch(opt){
			case 'p':
				perf = 1;
				break;
			case 'f':
				force = 1;
				break;
			case 'c':
				cont = 1;
				cont = atoi(optarg);
				if(cont<0) cont=0;
				break;
			case 'x':
				pool = 1;
				pool = atoi(optarg);
				if(pool<0) pool=0;
				break;
			case ':':
				fprintf(stderr,"option needs a value\n");
				break;
			case '?':
				fprintf(stderr,"unknown option: %c\n", optopt);
				break;
			default:
				fprintf(stderr,"unknown default option: %c\n", optopt);
				break;
		}
	}

	/* check input */
	if(argc-optind!=4){
		fprintf(stderr,"Usage:\n");
		fprintf(stderr,"%s [options] idh idl filename bsz\n", basename(argv[0]));
		return 111;
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
	bsz = atoll(argv[optind+3]);
	assert(bsz>0);
	assert(!(bsz%1024));

	/* extend */
	stat64(fname, &fs);
	fsz = fs.st_size;
	cnt = (fs.st_size + bsz - 1)/bsz;
	truncate64(fname,fs.st_size + bsz - 1);
	assert(!(fsz>cnt*bsz));

	/* init */
	c0appz_timein();
	if(c0appz_init(0)!=0){
		fprintf(stderr,"%s(): error!\n",__FUNCTION__);
		fprintf(stderr,"%s(): clovis initialisation failed.\n",__FUNCTION__);
		truncate64(fname,fs.st_size);
		stat64(fname,&fs);
		assert(fsz==fs.st_size);
		return 222;
	}
	ppf("%8s","init");
	c0appz_timeout(0);

	/* pool */
	if(pool>=0){
		c0appz_pool_ini();
		c0appz_pool_set(pool);
	}

	/* create object */
	c0appz_timein();
	if((!force)&&(c0appz_cr(idh,idl)!=0)){
		fprintf(stderr,"%s(): error!\n",__FUNCTION__);
		fprintf(stderr,"%s(): create object failed!!\n",__FUNCTION__);
		rc = 333;
		goto end;
	}
	if((force)&&(c0appz_cr(idh,idl)<0)){
		fprintf(stderr,"%s(): error!\n",__FUNCTION__);
		fprintf(stderr,"%s(): object NOT found!!\n",__FUNCTION__);
		rc = 333;
		goto end;
	}
	ppf("%8s","create");
	c0appz_timeout(0);

	/* continuous write */
	if(cont){
		fbuf = malloc(fs.st_size + bsz - 1);
		if(!fbuf){
			fprintf(stderr,"%s(): error!\n",__FUNCTION__);
			fprintf(stderr,"%s(): not enough memory!!\n",__FUNCTION__);
			rc = 111;
			goto end;
		}
		if(c0appz_fr(fbuf,fname,bsz,cnt)!=0){
			fprintf(stderr,"%s(): c0appz_fr failed!!\n",__FUNCTION__);
			rc = 555;
			goto end;
		}

		truncate64(fname,fs.st_size);
		stat64(fname,&fs);
		assert(fsz==fs.st_size);
		laps=cont;
		qos_served=0;
		qos_remain=bsz*cnt*laps;
		qos_laps_served=0;
		qos_laps_remain=laps;
		qos_pthread_start();
		c0appz_timein();
		while(cont>0){
			pos = (laps-cont)*cnt*bsz;
			c0appz_mw(fbuf,idh,idl,pos,bsz,cnt);
			cont--;
		}
		ppf("%8s","write");
		c0appz_timeout((uint64_t)bsz * (uint64_t)cnt * (uint64_t)laps);
		qos_pthread_wait();
		printf("%" PRIu64 " x %" PRIu64 " = %" PRIu64 "\n",cnt,bsz,cnt*bsz);
		free(fbuf);
		goto end;
	}

	qos_served=0;
	qos_remain=bsz*cnt;
	qos_laps_served=0;
	qos_laps_remain=1;
	qos_pthread_start();
	c0appz_timein();
	/* copy */
	if (c0appz_cp(idh,idl,fname,bsz,cnt) != 0) {
		fprintf(stderr,"%s(): error!\n",__FUNCTION__);
		fprintf(stderr,"%s(): copy object failed!!\n",__FUNCTION__);
		rc = 222;
		goto end;
	};
	ppf("%8s","copy");
	c0appz_timeout((uint64_t)bsz * (uint64_t)cnt);
	qos_pthread_wait();

end:

//	qos_pthread_stop(rc);
//	qos_pthread_wait();


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

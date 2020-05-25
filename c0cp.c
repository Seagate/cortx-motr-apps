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
	uint64_t laps=0;	/* number of reads		*/

	/* getopt */
	while((opt = getopt(argc, argv, ":pfc:"))!=-1){
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

	/* initialise resources */
	if(c0appz_init(0)!=0){
		fprintf(stderr,"%s(): error!\n",__FUNCTION__);
		fprintf(stderr,"%s(): clovis initialisation failed.\n",__FUNCTION__);
		truncate64(fname,fs.st_size);
		stat64(fname,&fs);
		assert(fsz==fs.st_size);
		return 222;
	}

	/* time out/in */
	if(perf){
		fprintf(stderr,"%4s","init");
		c0appz_timeout(0);
		c0appz_timein();
	}

	/* create object */
	if((c0appz_cr(idh,idl)!=0)&&(!force)){
		fprintf(stderr,"%s(): error!\n",__FUNCTION__);
		fprintf(stderr,"%s(): create object failed!!\n",__FUNCTION__);
		rc = 333;
		goto end;
	}

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
		qos_pthread_start();
		while(cont>0){
			printf("[%d/%d]:\n",(int)laps-cont+1,(int)laps);
			pos = (laps-cont)*cnt*bsz;
			c0appz_mw(fbuf,idh,idl,pos,bsz,cnt);
			cont--;
		}

		printf("%" PRIu64 " x %" PRIu64 " = %" PRIu64 "\n",cnt,bsz,cnt*bsz);
		free(fbuf);
		goto end;
	}

	laps=1;
	qos_pthread_start();
	/* copy */
	if (c0appz_cp(idh,idl,fname,bsz,cnt) != 0) {
		fprintf(stderr,"%s(): error!\n",__FUNCTION__);
		fprintf(stderr,"%s(): copy object failed!!\n",__FUNCTION__);
		rc = 222;
		goto end;
	};

end:

	qos_pthread_stop(rc);

	/* resize */
	truncate64(fname,fs.st_size);
	stat64(fname,&fs);
	assert(fsz==fs.st_size);

	/* time out/in */
	if((perf)&&(!rc)){
		fprintf(stderr,"%4s","i/o");
		c0appz_timeout((uint64_t)bsz * (uint64_t)cnt * (uint64_t)laps);
		c0appz_timein();
	}

	/* free resources*/
	c0appz_free();

	/* time out */
	if((perf)&&(!rc)){
		fprintf(stderr,"%4s","free");
		c0appz_timeout(0);
	}

	/* failure */
	if(rc){
		printf("%s %" PRIu64 "\n",fname,fs.st_size);
		fprintf(stderr,"%s failed!\n",basename(argv[0]));
		return rc;
	}

	/* success */
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

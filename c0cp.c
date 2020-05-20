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
extern int perf; 	/* performance 	*/
int force=0; 		/* overwrite  	*/


/* main */
int main(int argc, char **argv)
{
	uint64_t idh;		/* object id high 		*/
	uint64_t idl;		/* object is low		*/
	uint64_t bsz;		/* block size 			*/
	uint64_t cnt;		/* count				*/
	uint64_t fsz;		/* initial file size	*/
	char *fname;		/* input filename 		*/
	struct stat64 fs;	/* file statistics		*/
	int opt;			/* options				*/
	pthread_t tid;		/* real-time bw thread	*/

	/* getopt */
	while((opt = getopt(argc, argv, ":pf"))!=-1){
		switch(opt){
			case 'p':
				perf = 1;
				break;
			case 'f':
				force = 1;
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
		return -11;
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
		fprintf(stderr,"error! clovis initialisation failed.\n");
		truncate64(fname,fs.st_size);
		stat64(fname,&fs);
		assert(fsz==fs.st_size);
		return -22;
	}

	/* time out/in */
	if(perf){
		fprintf(stderr,"%4s","init");
		c0appz_timeout(0);
		c0appz_timein();
	}

	/* create object */
	if((c0appz_cr(idh,idl)!=0)&&(!force)){
		fprintf(stderr,"error! create object failed.\n");
		truncate64(fname,fs.st_size);
		stat64(fname,&fs);
		assert(fsz==fs.st_size);
		c0appz_free();
		return -33;
	}

	if(perf){
		pthread_create(&tid,NULL,&disp_realtime_bw,NULL);
	}

	/* copy */
	if (c0appz_cp(idh,idl,fname,bsz,cnt) != 0) {
		fprintf(stderr,"error! copy object failed.\n");
		truncate64(fname,fs.st_size);
		stat64(fname,&fs);
		assert(fsz==fs.st_size);
		c0appz_free();
		return -44;
	};

	if(perf){
		pthread_cancel(tid);
	}

	/* resize */
	truncate64(fname,fs.st_size);
	stat64(fname,&fs);
	assert(fsz==fs.st_size);

	/* time out/in */
	if(perf){
		fprintf(stderr,"%4s","i/o");
		c0appz_timeout((uint64_t)bsz * (uint64_t)cnt);
		c0appz_timein();
	}

	/* free resources*/
	c0appz_free();

	/* time out */
	if(perf){
		fprintf(stderr,"%4s","free");
		c0appz_timeout(0);
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

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
 * Original creation date: 10-Jan-2017
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include "c0appz.h"

/*
 ******************************************************************************
 * EXTERN VARIABLES
 ******************************************************************************
 */
extern int perf; /* performance */

/* main */
int main(int argc, char **argv)
{
	uint64_t idh;	/* object id high 	*/
	uint64_t idl;	/* object id low 	*/
	int opt=0;		/* options			*/

	/* getopt */
	while((opt = getopt(argc, argv, ":p"))!=-1){
		switch(opt){
			case 'p':
				perf = 1;
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
	if(argc-optind!=2){
		fprintf(stderr,"Usage:\n");
		fprintf(stderr,"%s [options] idh idl\n", basename(argv[0]));
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

	/* initialize resources */
	if(c0appz_init(0) != 0){
		fprintf(stderr,"error! clovis initialization failed.\n");
		return 222;
	}

	/* time out/in */
	if(perf){
		ppf("%4s","init");
		c0appz_timeout(0);
		c0appz_timein();
	}

	/* delete */
	if(c0appz_rm(idh,idl) != 0){
		fprintf(stderr,"error! delete object failed.\n");
		c0appz_free();
		return 333;
	};

	/* time out/in */
	if(perf){
		ppf("%4s","i/o");
		c0appz_timeout(0);
		c0appz_timein();
	}

	/* free resources*/
	c0appz_free();

	/* time out */
	if(perf){
		ppf("%4s","free");
		c0appz_timeout(0);
	}

	/* success */
	c0appz_dump_perf();
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

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
#include "help.h"

/*
 ******************************************************************************
 * EXTERN VARIABLES
 ******************************************************************************
 */
extern int perf; /* performance */

/*
 * help()
 */
int help()
{
	fprintf(stderr,"%s",(const char*)help_c0rm_txt);
	fprintf(stderr,"\n");
	exit(1);
}

/* main */
int main(int argc, char **argv)
{
	uint64_t idh;	/* object id high 	*/
	uint64_t idl;	/* object id low 	*/
	int opt=0;		/* options			*/
	int rc=0;
	int yes=0;

	/* getopt */
	while((opt = getopt(argc, argv, ":py"))!=-1){
		switch(opt){
			case 'p':
				perf = 1;
				break;
			case 'y':
				yes = 1;
				break;
			case ':':
				fprintf(stderr,"option needs a value\n");
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
	if(argc-optind!=2){
		help();
	}

	/* c0rcfile
	 * overwrite .cappzrc to a .[app]rc file.
	 */
	char str[256];
	sprintf(str,"%s/.%src", dirname(argv[0]), basename(argv[0]));
	c0appz_setrc(str);
	c0appz_putrc();

	/* set input */
	idh = atoll(argv[optind+0]);
	idl = atoll(argv[optind+1]);

	/* init */
	c0appz_timein();
	if(c0appz_init(0) != 0){
		fprintf(stderr,"error! clovis initialization failed.\n");
		return 222;
	}
	ppf("init");
	c0appz_timeout(0);

	/* check */
	c0appz_timein();
	if(!(c0appz_ex(idh,idl))){
		fprintf(stderr,"error!\n");
		fprintf(stderr,"object NOT found!!\n");
		rc = 1;
	}
	ppf("chck");
	c0appz_timeout(0);

	if(rc == 1) goto end;

	if(!yes){
		int c=0;
		printf("delete object(y/n):");
		c = getchar();
		if(c!='y'){
			fprintf(stderr,"object NOT deleted.\n");
			goto end;
		}
	}

	/* delete */
	c0appz_timein();
	if(c0appz_rm(idh,idl) != 0){
		fprintf(stderr,"error! delete object failed.\n");
		rc = 333;
		goto end;
	};
	ppf("%4s","rm");
	c0appz_timeout(0);
	fprintf(stderr,"success! object deleted!!\n");

end:

	/* c0appz_free */
	c0appz_timein();
	c0appz_free();
	ppf("free");
	c0appz_timeout(0);

	/* failure */
	if(rc>1){
		fprintf(stderr,"%s failed!\n",basename(argv[0]));
		return rc;
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

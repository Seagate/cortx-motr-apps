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
#include "c0appz.h"

/* main */
int main(int argc, char **argv)
{
	int64_t idh;	/* object id high 	*/
	int64_t idl;	/* object is low	*/
	int bsz;		/* block size 		*/
	int cnt;		/* count			*/
	char *fname;	/* input filename 	*/
	struct stat fs;	/* file statistics	*/

	/* check input */
	if (argc != 5) {
		fprintf(stderr,"Usage:\n");
		fprintf(stderr,"%s idh idl filename bsz\n", basename(argv[0]));
		return -1;
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
	idh = atoll(argv[1]);
	idl = atoll(argv[2]);
	fname = argv[3];
	bsz = atoi(argv[4]);
//	cnt 	= atoi(argv[5]);

	/* extend */
	stat(fname, &fs);
	cnt = (fs.st_size + bsz - 1)/bsz;
	truncate(fname,fs.st_size + bsz - 1);

	/* initialize resources */
	if (c0appz_init(0) != 0) {
		fprintf(stderr,"error! clovis initialization failed.\n");
		return -2;
	}

	/* time out/in */
	fprintf(stderr,"%4s","init");
	c0appz_timeout(0);
	c0appz_timein();

	/* copy */
	if (c0appz_cp(idh,idl,fname,bsz,cnt) != 0) {
		fprintf(stderr,"error! copy object failed.\n");
		c0appz_free();
		return -3;
	};

	/* resize */
	truncate(fname,fs.st_size);
	printf("%s %d\n",fname, (int)fs.st_size);

	/* time out/in */
	fprintf(stderr,"%4s","i/o");
	c0appz_timeout((uint64_t)bsz * (uint64_t)cnt);
	c0appz_timein();

	/* free resources*/
	c0appz_free();

	/* time out */
	fprintf(stderr,"%4s","free");
	c0appz_timeout(0);

	/* success */
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

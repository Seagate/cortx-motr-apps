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

#include "capps.h"

/* main */
int main(int argc, char **argv)
{
	int64_t idh;	/* object id high 	*/
	int64_t idl;	/* object is low	*/
	int bsz;		/* block size 		*/
	int cnt;		/* count			*/
	char *fname;	/* input filename 	*/

	/* check input */
	if (argc != 6) {
		fprintf(stderr,"Usage:\n");
		fprintf(stderr,"c0cat idh idl filename bsz cnt\n");
		return -1;
	}

	/* c0rcfile
	 * overwrite .cappsrc to a .[app]rc file.
	 */
	char str[256];
	sprintf(str,".%src",basename(argv[0]));
	c0apps_setrc(str);
	c0apps_putrc();

	/* set input */
	idh = atoll(argv[1]);
	idl = atoll(argv[2]);
	bsz = atoi(argv[4]);
	cnt = atoi(argv[5]);
	fname = argv[3];

	/* initialize resources */
	if (c0apps_init() != 0) {
		fprintf(stderr,"error! clovis initialization failed.\n");
		return -2;
	}

	/* copy */
	if (c0apps_cp(idh,idl,fname,bsz,cnt) != 0) {
		fprintf(stderr,"error! copy object failed.\n");
		c0apps_free();
		return -3;
	};

	/* free resources*/
	c0apps_free();

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

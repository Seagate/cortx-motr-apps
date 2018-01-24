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

#include "c2.h"

/* main */
int main(int argc, char **argv)
{
	int idh;	/* object id high 	*/
	int idl;	/* object is low	*/
	int bsz;	/* block size 		*/
	int cnt;	/* count			*/

	/* check input */
	if (argc != 5) {
		printf("Usage:\n");
		printf("c0cat idh idl bsz cnt\n");
		return -1;
	}

	/* set input */
	idh = atoi(argv[1]);
	idl = atoi(argv[2]);
	bsz = atoi(argv[3]);
	cnt = atoi(argv[4]);

	/* initialize resources */
	if (c2init() != 0) {
		printf("error! clovis initialization failed.\n");
		return -2;
	}

	/* cat */
	if (objcat(idh,idl,bsz,cnt) != 0) {
		printf("error! cat object failed.\n");
		c2free();
		return -3;
	};

	/* free resources*/
	c2free();

	/* success */
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

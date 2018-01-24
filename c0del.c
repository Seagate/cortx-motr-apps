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

#include "capps.h"


int main(int argc, char **argv)
{
	int idh;
	int idl;

	/* check input */
	if (argc != 3) {
		fprintf(stderr,"Usage:\n");
		fprintf(stderr,"c0del idh idl\n");
		return -1;
	}

	/* set input */
	idh = atoi(argv[1]);
	idl = atoi(argv[2]);

	/* initialize resources */
	if (c0init() != 0) {
		fprintf(stderr,"error! clovis initialization failed.\n");
		return -2;
	}

	/* delete */
	if (objdel(idh,idl) != 0) {
		fprintf(stderr,"error! delete object failed.\n");
		c0free();
		return -3;
	};

	/* free resources*/
	c0free();

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

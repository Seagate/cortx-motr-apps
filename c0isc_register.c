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
#include "c0appz.h"
#include "c0appz_isc.h"
#include "fid/fid.h"
#include "lib/buf.h"
#include "lib/misc.h"
#include "iscservice/isc.h"
#include "rpc/link.h"

char *prog;

/* main */
int main(int argc, char **argv)
{
	int rc = 0;

	prog = basename(strdup(argv[0]));

	/* check input */
	if (argc != 2) {
		fprintf(stderr,"Usage:\n");
		fprintf(stderr,"%s libpath\n", prog);
		return -1;
	}

	/* time in */
	c0appz_timein();

	c0appz_setrc(prog);
	c0appz_putrc();

	/* initialize resources */
	rc = c0appz_init(0);
	if (rc != 0) {
		fprintf(stderr,"error! c0appz_init() failed: %d\n", rc);
		return -2;
	}
	rc = c0appz_isc_api_register(argv[1]);
	if ( rc != 0)
		fprintf(stderr, "error! loading of library from %s failed. \n",
			argv[1]);
	/* free resources*/
	c0appz_free();

	/* time out */
	c0appz_timeout(0);

	/* success */
	if (rc == 0)
		fprintf(stderr,"%s success\n", prog);
	else
		fprintf(stderr,"%s fail\n", prog);
	return rc;
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

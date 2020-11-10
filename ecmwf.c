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
 *
 * build/run:
 * 	- make ecmwf
 * 	- make ecmwf-clean
 * 	- ./scripts/c0appzrcgen > ./.cappzrc
 * 	- ./ecmwfx
 */

#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <inttypes.h>
#include "c0appz.h"

/* main */
int main(int argc, char **argv)
{
	int i;
	int64_t  idh;	/* object id high 	*/
	int64_t  idl;	/* object id low	*/

	printf("ECMWF test\n");

	/* fgen test */
	for(i=0; i<10; i++) {
		if(!c0appz_generate_id(&idh,&idl)) {
			printf("%" PRId64 " " "%" PRId64, idh,idl);
			printf("\n");
		}
		else {
			fprintf(stderr, "error!\n");
			fprintf(stderr, "failed to generate ids\n");
		}
	}

	/*
	 * Motr client tests
	 */

	/* initialize resources */
	rc = c0appz_init(0);
	if (rc != 0) {
		fprintf(stderr,"error! c0appz_init() failed: %d\n", rc);
		return -2;
	}

	/*
	 ** TODO:
	 ** do something here.
	 */

	/* free resources*/
	c0appz_free();

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

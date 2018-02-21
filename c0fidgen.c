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
#include <inttypes.h>
#include <time.h>
#include <openssl/md5.h>

#ifndef DEBUG
#define DEBUG 0
#endif

/*
 * compile:
 * gcc -Wall -lssl -lcrypto c0fidgen.c
 */

/* main */
int main(int argc, char **argv)
{
	int64_t  idh;	/* object id high 	*/
	int64_t  idl;	/* object is low	*/

    MD5_CTX c;
    char buf[512];
    unsigned char chksum[16];

	idh = 0;
	idl = 0;

	/* init */
    MD5_Init(&c);

    /* utc */
    memset(buf, 0x00, 512);
    sprintf(buf, "%d", (int)time(NULL));
	#if DEBUG
    fprintf(stderr, "%s\n", buf);
    fprintf(stderr, "sz = %d\n",(int)strlen(buf));
	#endif
    MD5_Update(&c, buf, strlen(buf));

    /* srandom */
	srandom(0);
    memset(buf, 0x00, 512);
    sprintf(buf, "%d", (int)random());
	#if DEBUG
    fprintf(stderr, "%s\n", buf);
    fprintf(stderr, "sz = %d\n",(int)strlen(buf));
	#endif
    MD5_Update(&c, buf, strlen(buf));

    /*
     * TO DO
     * add more salt here.
     */

	/* final */
    MD5_Final(chksum, &c);

	#if DEBUG
    int n;
    fprintf(stderr, "md5: ");
    for(n=0; n<MD5_DIGEST_LENGTH; n++)
    	fprintf(stderr, "%02x", chksum[n]);
    fprintf(stderr, "\n");
	#endif

	#if DEBUG
	fprintf(stderr, "[%" PRId64 ", " "%" PRId64 "]", idh, idl);
	fprintf(stderr, "\n");
	#endif

	memmove(&idh, &chksum[0], sizeof(int64_t));
	memmove(&idl, &chksum[8], sizeof(int64_t));
	printf("%" PRId64 " " "%" PRId64, idh, idl);
	printf("\n");

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

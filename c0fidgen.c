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

/*
 * compile:
 * gcc -Wall -lssl -lcrypto c0fidgen.c
 */

/* main */
int main(int argc, char **argv)
{
	int64_t  idh;	/* object id high 	*/
	int64_t  idl;	/* object is low	*/
	time_t   utc;	/* utc time			*/
	long int rnd;	/* random number	*/

    int n;
    MD5_CTX c;
    char buf[512];
//    ssize_t bytes;
    unsigned char chksum[16];


	idh = 0x1122334455667788;
	idl = 0xaabbccddeeff0011;

	srandom(0);
	rnd = random();
	utc = time(NULL);

	idh += utc;
	idl += utc;
	idh += rnd;
	idl += rnd;

    MD5_Init(&c);
    MD5_Update(&c, buf, 10);
    MD5_Update(&c, buf, 20);
    MD5_Final(chksum, &c);

    for(n=0; n<MD5_DIGEST_LENGTH; n++)
    	printf("%02x", chksum[n]);
    printf("\n");


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

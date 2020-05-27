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
 * Original creation date: 26-May-2020
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <sys/time.h>
#include <stdarg.h>

#define PBUFSZ 1024

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */

/*
 ******************************************************************************
 * STATIC FUNCTION PROTOTYPES
 ******************************************************************************
 */

static struct  timeval wclk_t = {0, 0};
static clock_t cput_t = 0;
static char pbuf[PBUFSZ] = {0};

/*
 ******************************************************************************
 * EXTERN FUNCTIONS
 ******************************************************************************
 */

/*
 * c0appz_dump_perf()
 * display/store performance results
 */
int c0appz_dump_perf(void)
{
	printf("%s",pbuf);
	return 0;
}

/*
 * ppf()
 * performance printf.
 */
int ppf(const char *fmt, ...)
{
	int n=0;
	int s=0;
	char buffer[128];
	va_list args;
	va_start(args, fmt);
	s = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	n = strlen(pbuf);
	snprintf(pbuf+n,PBUFSZ-n,buffer);
	return s;
}

/*
 * c0appz_timeout()
 * time out execution.
 */
int c0appz_timeout(uint64_t sz)
{
	double ct;        /* cpu time in seconds  */
	double wt;        /* wall time in seconds */
	double bw_ctime;  /* bandwidth in MBs     */
	double bw_wtime;  /* bandwidth in MBs     */
	struct timeval tv;

	/* cpu time */
	ct = (double)(clock() - cput_t) / CLOCKS_PER_SEC;
	bw_ctime = (double)(sz) / 1000000.0 / ct;

	/* wall time */
	gettimeofday(&tv, 0);
	wt  = (double)(tv.tv_sec - wclk_t.tv_sec);
	wt += (double)(tv.tv_usec - wclk_t.tv_usec)/1000000;
	bw_wtime  = (double)(sz) / 1000000.0 / wt;

/*
	fprintf(stderr,"[ cput: %10.4lf s %10.4lf MB/s ]", ct, bw_ctime);
	fprintf(stderr,"[ wclk: %10.4lf s %10.4lf MB/s ]", wt, bw_wtime);
	fprintf(stderr,"\n");
*/
/*
	int n = 0;
	n = strlen(pbuf);
	snprintf(pbuf+n,PBUFSZ-n,"[ cput:%10.4lf s %10.4lf MB/s ]",ct,bw_ctime);
	n = strlen(pbuf);
	snprintf(pbuf+n,PBUFSZ-n,"[ wclk:%10.4lf s %10.4lf MB/s ]",wt,bw_wtime);
	n = strlen(pbuf);
	snprintf(pbuf+n,PBUFSZ-n,"\n");
*/

	ppf("[ cput: %10.4lf s %10.4lf MB/s ]", ct, bw_ctime);
	ppf("[ wclk: %10.4lf s %10.4lf MB/s ]", wt, bw_wtime);
	ppf("\n");
	return 0;
}

/*
 * c0appz_timein()
 * time in execution.
 */
int c0appz_timein()
{
	cput_t = clock();
	gettimeofday(&wclk_t, 0);
	return 0;
}

/*
 ******************************************************************************
 * STATIC FUNCTIONS
 ******************************************************************************
 */


/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */

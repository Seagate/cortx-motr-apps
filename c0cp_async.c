/* -*- C -*- */
/*
 * COPYRIGHT 2018 SEAGATE LLC
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
 * http://www.seagate.com/contact
 *
 * Original author:  Abhishek Saha <abhishek.saha@seagate.com>
 * Original creation date: 02-Nov-2018
 *
 * Modifications:  Ganesan Umanesan <ganesan.umanesan@seagate.com>
 * Modification Date: 07-May-2020
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <inttypes.h>
#include <assert.h>
#include "c0appz.h"
#include "c0appz_internal.h"

extern int perf; /* show performance stats */
extern uint64_t qos_whgt_served;
extern uint64_t qos_whgt_remain;
extern uint64_t qos_laps_served;
extern uint64_t qos_laps_remain;
extern pthread_mutex_t qos_lock;  /* lock  qos_total_weight */
extern struct m0_fid *pool_fid;

char *prog;

int main(int argc, char **argv)
{
	int rc;
	int opt = 0;       /* options */
	int cont = 0;      /* continuous mode   */
	int force = 0;     /* overwrite object if already exists */
	char *fname;       /* input filename */
	char *fbuf;        /* file buffer */
	uint64_t idh;      /* object id high */
	uint64_t idl;      /* object id low */
	uint64_t pos;      /* starting position */
	uint64_t bsz;      /* block size */
	uint64_t m0bs;     /* m0 block size */
	uint64_t cnt;      /* count */
	uint64_t op_cnt;   /* number of parallel ops */
	struct stat64 fs;  /* file statistics */
	char str[256];

	prog = basename(strdup(argv[0]));

	while((opt = getopt(argc, argv, ":pfu:c:"))!=-1){
		switch(opt){
		case 'p':
			perf = 1;
			break;
		case 'f':
			force = 1;
			break;
		case 'u':
			if (sscanf(optarg, "%i", &unit_size) != 1) {
				fprintf(stderr, "invalid unit size\n");
				exit(1);
			}
			break;
		case 'c':
			cont = atoi(optarg);
			if (cont < 0) cont=0;
			if (!cont) {
				fprintf(stderr, "invalid -c value: %d\n", cont);
				exit(1);
			}
			break;
		case ':':
			fprintf(stderr,"option needs a value\n");
			break;
		case '?':
			fprintf(stderr,"unknown option: %c\n", optopt);
			break;
		default:
			fprintf(stderr,"unknown default option: %c\n", optopt);
			break;
		}
	}

	/* check input */
	if (argc - optind != 5) {
		fprintf(stderr,"Usage:\n"
			"%s [options] idh idl filename bsz opcnt\n", prog);
		return 111;
	}

	/* c0rcfile
	 * overwrite .cappzrc to a .[app]rc file.
	 */
	sprintf(str,"%s/.%src", dirname(argv[0]), prog);
	c0appz_setrc(str);
	c0appz_putrc();

	/* set input */
	if (sscanf(argv[optind+0], "%li", &idh) != 1) {
		ERR("invalid idhi - %s", argv[optind+0]);
		exit(1);
	}
	if (sscanf(argv[optind+1], "%li", &idl) != 1) {
		ERR("invalid idlo - %s", argv[optind+1]);
		exit(1);
	}
	fname = argv[optind+2];
	if (sscanf(argv[optind+3], "%li", &bsz) != 1) {
		ERR("invalid block size value: %s\n", argv[optind+3]);
		exit(1);
	}
	bsz *= 1024;

	op_cnt = atoll(argv[optind+4]);

	if (bsz == 0) {
		fprintf(stderr,"%s: bsz must be > 0\n", prog);
		exit(1);
	}

	rc = stat64(fname, &fs);
	if (rc != 0) {
		fprintf(stderr,"%s: %s: %s\n", prog, fname, strerror(errno));
		exit(1);
	}
	cnt = (fs.st_size + bsz - 1) / bsz;

	/* init */
	c0appz_timein();
	if(c0appz_init(0)!=0){
		fprintf(stderr,"error! clovis initialization failed.\n");
		return 222;
	}
	ppf("%6s","init");
	c0appz_timeout(0);

	m0bs = c0appz_m0bs(idh, idl, bsz * cnt, pool_fid);
	if (!m0bs) {
		fprintf(stderr,"%s(): error: c0appz_m0bs() failed.\n",
			__func__);
		c0appz_free();
		return 223;
	}

	/* create object */
	c0appz_timein();
	if (c0appz_cr(idh, idl, pool_fid, m0bs) != 0 && !force) {
		fprintf(stderr,"error! create object failed.\n");
		c0appz_free();
		return 333;
	}
	ppf("%6s","create");
	c0appz_timeout(0);

	/* continuous write */
	if (cont) {
		fbuf = malloc(fs.st_size + bsz - 1);
		if (!fbuf) {
			fprintf(stderr,"%s(): error: not enough memory.\n",
				__func__);
			rc = 111;
			goto end;
		}
		rc = c0appz_fr(fbuf, fname, bsz, cnt);
		if (rc != 0) {
			fprintf(stderr,"%s(): c0appz_fr failed: rc=%d\n",
				__func__, rc);
			rc = 555;
			goto end;
		}

		qos_whgt_served = 0;
		qos_whgt_remain = bsz * cnt * cont;
		qos_laps_served = 0;
		qos_laps_remain = cont;
		qos_pthread_start();
		c0appz_timein();

		for (pos = 0; cont > 0; cont--, pos += cnt * bsz) {
			rc = c0appz_mw_async(fbuf, idh, idl, pos, bsz, cnt,
					     op_cnt, m0bs);
			if (rc != 0) {
				fprintf(stderr, "%s: c0appz_mw_async() failed"
					" at pos %lu: %d\n", prog, pos, rc);
				break;
			}
			pthread_mutex_lock(&qos_lock);
			qos_laps_served++;
			qos_laps_remain--;
			pthread_mutex_unlock(&qos_lock);
		}

		ppf("%8s","write");
		c0appz_timeout(pos);
		qos_pthread_wait();
		printf("%" PRIu64 " x %" PRIu64 " = %" PRIu64 "\n", cnt, bsz,
		       cnt * bsz);
		printf("Total written: %lu bytes\n", pos);
		free(fbuf);
		goto end;
	}

	/* copy */
	qos_whgt_served = 0;
	qos_whgt_remain = bsz * cnt;
	qos_laps_served = 0;
	qos_laps_remain = 1;
	qos_pthread_start();
	c0appz_timein();
	if (c0appz_cp_async(idh, idl, fname, bsz, cnt, op_cnt, m0bs) != 0) {
		fprintf(stderr,"error! copy object failed.\n");
		qos_pthread_stop(0);
		c0appz_free();
		return 444;
	};
	pthread_mutex_lock(&qos_lock);
	qos_laps_served++;
	qos_laps_remain--;
	pthread_mutex_unlock(&qos_lock);
	ppf("%6s","copy");
	c0appz_timeout(bsz*cnt);
	qos_pthread_wait();
 end:
	/* free */
	c0appz_timein();
	c0appz_free();
	ppf("%6s","free");
	c0appz_timeout(0);

	/* success */
	c0appz_dump_perf();
	printf("%s %" PRIu64 "\n",fname,fs.st_size);
	fprintf(stderr,"%s success\n", prog);
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

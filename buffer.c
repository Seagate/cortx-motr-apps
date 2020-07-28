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
 * Original creation date: 21-May-2020
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/user.h>		/* PAGE_SIZE */
#include <pthread.h>
#include <inttypes.h>
#include <assert.h>

#include "c0appz.h"
#include "clovis/clovis.h"
#include "clovis/clovis_idx.h"
#include "layout/layout.h"	/* M0_DEFAULT_LAYOUT_ID */

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */
extern int qos_total_weight;	/* total bytes read or written  */
extern pthread_mutex_t qos_lock;	/* lock  qos_total_weight */

/*
 ******************************************************************************
 * STATIC VARIABLES
 ******************************************************************************
 */
static struct m0_indexvec extn;
static struct m0_bufvec data;
static struct m0_bufvec attr;

/*
 ******************************************************************************
 * STATIC FUNCTION PROTOTYPES
 ******************************************************************************
 */

static void freevecs(void);
static int initvecs(uint64_t pos, uint64_t bsz, uint64_t cnt);
static int bufvecw(const char *buf, uint64_t bsz, uint64_t cnt);
static int bufvecr(char *buf, uint64_t bsz, uint64_t cnt);

/*
 ******************************************************************************
 * EXTERN FUNCTIONS
 ******************************************************************************
 */

/*
 * c0appz_fr()
 * read from file.
 * copy cnt number of bsz size of blocks from input
 * file inf to a memory buffer buf. Assume buf is large
 * enough to copy bsz*cnt bytes.
 */
int c0appz_fr(char *buf, char *inf, uint64_t bsz, uint64_t cnt)
{
	uint64_t n;
	FILE *fp;

	/* open input file */
	fp = fopen(inf, "rb");
	if (fp == NULL) {
		fprintf(stderr, "%s(): error! - ", __FUNCTION__);
		fprintf(stderr, "could not open input file %s\n", inf);
		return 11;
	}

	/* read into buf */
	n = fread(buf, bsz, cnt, fp);
	if (n < cnt - 1) {
		fprintf(stderr, "%s(): error! - ", __FUNCTION__);
		fprintf(stderr, "reading from %s failed.\n", inf);
		fclose(fp);
		return 22;
	}

	/* success */
	fclose(fp);
	return 0;
}

/*
 * c0appz_fw()
 * write to file.
 * copy cnt number of bsz size of blocks from input
 * memory buffer to output file ouf. Assume ouf is
 * large enough to copy bsz*cnt bytes.
 */
int c0appz_fw(char *buf, char *ouf, uint64_t bsz, uint64_t cnt)
{
	uint64_t n;
	FILE *fp = NULL;

	/* open output file */
	fp = fopen(ouf, "wb");
	if (!fp) {
		fprintf(stderr, "%s(): error! - ", __FUNCTION__);
		fprintf(stderr, "could not open input file %s\n", ouf);
		return 11;
	}

	/* write to ouf */
	n = fwrite(buf, bsz, cnt, fp);
	if (n != cnt) {
		fprintf(stderr, "%s(): error! - ", __FUNCTION__);
		fprintf(stderr, "writing to %s failed.\n", ouf);
		fclose(fp);
		return 22;
	}

	/* success */
	fclose(fp);
	return 0;
}

/*
 * c0appz_mr()
 * read from mero object!
 * reads data from a mero object to memory buffer.
 * reads cnt number of blocks, each of size bsz from
 * pos (byte) position of the object
 */
int c0appz_mr(char *buf, uint64_t idhi, uint64_t idlo, uint64_t pos,
	      uint64_t bsz, uint64_t cnt, uint64_t m0bs)
{
	int rc;
	unsigned cnt_per_op;
	struct m0_uint128 id = { idhi, idlo };
	struct m0_clovis_obj obj = { };

	if (bsz < 1 || bsz % PAGE_SIZE) {
		fprintf(stderr,
			"%s(): bsz(%lu) must be multiple of %luK\n",
			__func__, m0bs, PAGE_SIZE / 1024);
		return -EINVAL;
	}

	if (m0bs < 1 || m0bs % bsz) {
		fprintf(stderr,
			"%s(): m0bs(%lu) must be multiple of bsz(%lu)\n",
			__func__, m0bs, bsz);
		return -EINVAL;
	}

	cnt_per_op = m0bs / bsz;

	/* Set the object entity we want to write */
	m0_clovis_obj_init(&obj, &clovis_uber_realm, &id,
			   M0_DEFAULT_LAYOUT_ID);

	rc = open_entity(&obj.ob_entity);
	if (rc != 0) {
		fprintf(stderr, "%s(): open_entity() failed: rc=%d\n",
			__func__, rc);
		return rc;
	}

	while (cnt > 0) {
		if (cnt < cnt_per_op)
			cnt_per_op = cnt;
		if (initvecs(pos, bsz, cnt_per_op) != 0) {
			fprintf(stderr, "%s(): error! not enough memory\n",
				__func__);
			rc = -ENOMEM;
			break;
		}

		rc = read_data_from_object(&obj, &extn, &data, &attr);
		if (rc != 0) {
			fprintf(stderr,
				"%s(): reading object failed: rc=%d\n",
				__func__, rc);
			freevecs();
			break;
		}

		/* copy to memory */
		bufvecr(buf, bsz, cnt_per_op);

		/* QOS */
		pthread_mutex_lock(&qos_lock);
		qos_total_weight += cnt_per_op * bsz;
		pthread_mutex_unlock(&qos_lock);
		/* END */

		freevecs();
		buf += cnt_per_op * bsz;
		pos += cnt_per_op * bsz;
		cnt -= cnt_per_op;
	}

	m0_clovis_entity_fini(&obj.ob_entity);

	return rc;
}

/*
 * c0appz_mw()
 * write to mero object!
 * writes data from memory buffer to a mero object.
 * writes cnt number of blocks, each of size bsz from
 * pos (byte) position of the object
 */
int c0appz_mw(const char *buf, uint64_t idhi, uint64_t idlo, uint64_t pos,
	      uint64_t bsz, uint64_t cnt, uint64_t m0bs)
{
	int rc;
	unsigned cnt_per_op;
	struct m0_uint128 id = { idhi, idlo };
	struct m0_clovis_obj obj = { };

	if (bsz < 1 || bsz % PAGE_SIZE) {
		fprintf(stderr,
			"%s(): bsz(%lu) must be multiple of %luK\n",
			__func__, m0bs, PAGE_SIZE / 1024);
		return -EINVAL;
	}

	if (m0bs < 1 || m0bs % bsz) {
		fprintf(stderr,
			"%s(): m0bs(%lu) must be multiple of bsz(%lu)\n",
			__func__, m0bs, bsz);
		return -EINVAL;
	}

	cnt_per_op = m0bs / bsz;

	/* Set the object entity we want to write */
	m0_clovis_obj_init(&obj, &clovis_uber_realm, &id,
			   M0_DEFAULT_LAYOUT_ID);

	rc = open_entity(&obj.ob_entity);
	if (rc != 0) {
		fprintf(stderr, "%s(): open_entity() failed: rc=%d\n",
			__func__, rc);
		return rc;
	}

	while (cnt > 0) {
		if (cnt < cnt_per_op)
			cnt_per_op = cnt;
		if (initvecs(pos, bsz, cnt_per_op) != 0) {
			fprintf(stderr, "%s(): error! not enough memory\n",
				__func__);
			rc = -ENOMEM;
			break;
		}
		bufvecw(buf, bsz, cnt_per_op);
		rc = write_data_to_object(&obj, &extn, &data, &attr);
		if (rc != 0) {
			fprintf(stderr,
				"%s(): writing object failed: rc=%d\n",
				__func__, rc);
			freevecs();
			break;
		}

		/* QOS */
		pthread_mutex_lock(&qos_lock);
		qos_total_weight += cnt_per_op * bsz;
		pthread_mutex_unlock(&qos_lock);
		/* END */

		freevecs();
		buf += cnt_per_op * bsz;
		pos += cnt_per_op * bsz;
		cnt -= cnt_per_op;
	}

	m0_clovis_entity_fini(&obj.ob_entity);

	return rc;
}

/*
 ******************************************************************************
 * STATIC FUNCTIONS
 ******************************************************************************
 */

/* initvecs() */
static int initvecs(uint64_t pos, uint64_t bsz, uint64_t cnt)
{
	int rc;
	int i;

	/* allocate buffers */
	rc = m0_bufvec_alloc(&data, cnt, bsz) ? :
	    m0_bufvec_alloc(&attr, cnt, 1) ? :
	    m0_indexvec_alloc(&extn, cnt);
	if (rc != 0) {
		freevecs();
		fprintf(stderr, "error! failed to allocate bufvecs!!\n");
		return rc;
	}

	/* prepare index */
	for (i = 0; i < cnt; i++) {
		extn.iv_index[i] = pos;
		extn.iv_vec.v_count[i] = bsz;
		attr.ov_vec.v_count[i] = 0;	/* no attributes */
		pos += bsz;
	}

	/* success */
	return 0;
}

/* bufvecw() */
static int bufvecw(const char *buf, uint64_t bsz, uint64_t cnt)
{
	int i;
	/* copy block by block */
	assert(data.ov_vec.v_nr == cnt);
	for (i = 0; i < cnt; i++) {
		memmove(data.ov_buf[i], buf, bsz);
		buf += bsz;
	}
	/* success */
	return 0;
}

/* bufvecr() */
static int bufvecr(char *buf, uint64_t bsz, uint64_t cnt)
{
	int i;
	/* copy block by block */
	assert(data.ov_vec.v_nr == cnt);
	for (i = 0; i < cnt; i++) {
		memmove(buf, data.ov_buf[i], bsz);
		buf += bsz;
	}
	/* success */
	return 0;
}

/* freevecs() */
static void freevecs(void)
{
	m0_bufvec_free(&data);
	m0_bufvec_free(&attr);
	m0_indexvec_free(&extn);
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

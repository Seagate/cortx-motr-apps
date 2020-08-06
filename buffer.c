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

#include "clovis/clovis.h"
#include "clovis/clovis_idx.h"
#include "layout/layout.h"	/* M0_DEFAULT_LAYOUT_ID */
#include "c0appz.h"
#include "c0appz_internal.h"

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */
extern int qos_total_weight;	/* total bytes read or written  */
extern pthread_mutex_t qos_lock;	/* lock  qos_total_weight */

/*
 ******************************************************************************
 * STATIC FUNCTION PROTOTYPES
 ******************************************************************************
 */

static uint64_t bufvecw(struct m0_bufvec *data, const char *buf,
			uint64_t bsz, uint32_t cnt);
static uint64_t bufvecr(const struct m0_bufvec *data, char *buf,
			uint64_t bsz, uint32_t cnt);

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
		fprintf(stderr, "could not open input file %s: %s\n",
			inf, strerror(errno));
		return 11;
	}

	/* read into buf */
	n = fread(buf, 1, bsz * cnt, fp);
	if (n <= bsz * (cnt - 1)) {
		fprintf(stderr, "%s(): error! - ", __FUNCTION__);
		fprintf(stderr, "reading from %s failed: expected at least %lu "
			", but read %lu bytes\n", inf, bsz * (cnt - 1) + 1, n);
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
		ERRS("could not open input file %s", ouf);
		return 11;
	}

	/* write to ouf */
	n = fwrite(buf, bsz, cnt, fp);
	if (n != cnt) {
		ERRS("writing to %s failed", ouf);
		fclose(fp);
		return 22;
	}

	/* success */
	fclose(fp);
	return 0;
}

/*
 * c0appz_mr()
 * Reads data from a mero object to memory buffer.
 * Reads cnt number of blocks, each of size bsz starting at
 * off (byte) offset of the object.
 */
int c0appz_mr(char *buf, uint64_t idhi, uint64_t idlo, uint64_t off,
	      uint64_t bsz, uint64_t cnt, uint64_t m0bs)
{
	int rc;
	unsigned cnt_per_op;
	struct m0_uint128 id = { idhi, idlo };
	struct m0_clovis_obj obj = { };
	struct m0_indexvec ext;
	struct m0_bufvec   data;
	struct m0_bufvec   attr;

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

	rc = alloc_segs(&data, &ext, &attr, bsz, cnt_per_op);
	if (rc != 0) {
		fprintf(stderr, "%s(): error! not enough memory\n",
			__func__);
		return rc;
	}

	/* Set the object entity we want to write */
	m0_clovis_obj_init(&obj, &clovis_uber_realm, &id,
			   M0_DEFAULT_LAYOUT_ID);

	rc = open_entity(&obj.ob_entity);
	if (rc != 0) {
		fprintf(stderr, "%s(): open_entity() failed: rc=%d\n",
			__func__, rc);
		goto free;
	}

	for (; cnt > 0; cnt -= cnt_per_op) {
		if (cnt < cnt_per_op)
			cnt_per_op = cnt;

		off += set_exts(&ext, off, bsz);

		rc = read_data_from_object(&obj, &ext, &data, &attr);
		if (rc != 0) {
			fprintf(stderr,
				"%s(): reading object failed: rc=%d\n",
				__func__, rc);
			free_segs(&data, &ext, &attr);
			break;
		}

		/* copy to memory */
		buf += bufvecr(&data, buf, bsz, cnt_per_op);

		/* QOS */
		pthread_mutex_lock(&qos_lock);
		qos_total_weight += cnt_per_op * bsz;
		pthread_mutex_unlock(&qos_lock);
		/* END */
	}
 free:
	free_segs(&data, &ext, &attr);
	m0_clovis_entity_fini(&obj.ob_entity);

	return rc;
}

/*
 * c0appz_mw()
 * Writes data from memory buffer to a mero object.
 * Writes cnt number of blocks, each of size bsz starting at
 * off (byte) offset of the object.
 */
int c0appz_mw(const char *buf, uint64_t idhi, uint64_t idlo, uint64_t off,
	      uint64_t bsz, uint64_t cnt, uint64_t m0bs)
{
	int rc;
	unsigned cnt_per_op;
	struct m0_uint128 id = { idhi, idlo };
	struct m0_clovis_obj obj = { };
	struct m0_indexvec ext;
	struct m0_bufvec   data;
	struct m0_bufvec   attr;

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

	rc = alloc_segs(&data, &ext, &attr, bsz, cnt_per_op);
	if (rc != 0) {
		fprintf(stderr, "%s(): error! not enough memory\n",
			__func__);
		return rc;
	}

	/* Set the object entity we want to write */
	m0_clovis_obj_init(&obj, &clovis_uber_realm, &id,
			   M0_DEFAULT_LAYOUT_ID);

	rc = open_entity(&obj.ob_entity);
	if (rc != 0) {
		fprintf(stderr, "%s(): open_entity() failed: rc=%d\n",
			__func__, rc);
		goto free;
	}

	for (; cnt > 0; cnt -= cnt_per_op) {
		if (cnt < cnt_per_op)
			cnt_per_op = cnt;

		off += set_exts(&ext, off, bsz);
		buf += bufvecw(&data, buf, bsz, cnt_per_op);
		rc = write_data_to_object(&obj, &ext, &data, &attr);
		if (rc != 0) {
			fprintf(stderr,
				"%s(): writing object failed: rc=%d\n",
				__func__, rc);
			free_segs(&data, &ext, &attr);
			break;
		}

		/* QOS */
		pthread_mutex_lock(&qos_lock);
		qos_total_weight += cnt_per_op * bsz;
		pthread_mutex_unlock(&qos_lock);
		/* END */
	}
 free:
	free_segs(&data, &ext, &attr);
	m0_clovis_entity_fini(&obj.ob_entity);

	return rc;
}

/*
 * c0appz_mw_async()
 * Writes data from memory buffer to a mero object in async mode op_cnt
 * Clovis operations at a time. Writes cnt number of blocks, each of size
 * bsz at off (byte) offset of the object.
 */
int c0appz_mw_async(const char *buf, uint64_t idhi, uint64_t idlo, uint64_t off,
		    uint64_t bsz, uint64_t cnt, uint32_t op_cnt, uint64_t m0bs)
{
	int                       rc = 0;
	uint32_t                  i;
	uint32_t                  nr_ops_sent;
	uint32_t                  cnt_per_op;
	struct m0_uint128         id = {idhi, idlo};
	struct clovis_aio_op     *aio;
	struct clovis_aio_opgrp   aio_grp;

	if (bsz < 1 || bsz % PAGE_SIZE) {
		fprintf(stderr, "%s(): bsz(%lu) must be multiple of %luK\n",
			__func__, m0bs, PAGE_SIZE / 1024);
		return -EINVAL;
	}

	if (m0bs < 1 || m0bs % bsz) {
		fprintf(stderr, "%s(): m0bs(%lu) must be multiple of bsz(%lu)\n",
			__func__, m0bs, bsz);
		return -EINVAL;
	}

	cnt_per_op = m0bs / bsz;

	/* Initialise operation group. */
	rc = clovis_aio_opgrp_init(&aio_grp, bsz, cnt_per_op, op_cnt);
	if (rc != 0) {
		fprintf(stderr, "%s(): clovis_aio_opgrp_init() failed: rc=%d\n",
			__func__, rc);
		return rc;
	}

	/* Open the object. */
	m0_clovis_obj_init(&aio_grp.cag_obj, &clovis_uber_realm,
			   &id, M0_DEFAULT_LAYOUT_ID);
	rc = open_entity(&aio_grp.cag_obj.ob_entity);
	if (rc != 0) {
		fprintf(stderr, "%s(): open_entity() failed: rc=%d\n",
			__func__, rc);
		goto fini;
	}

	while (cnt > 0) {
		/* Set each op. */
		nr_ops_sent = 0;
		for (i = 0; i < op_cnt && cnt > 0; i++, cnt -= cnt_per_op) {
			aio = &aio_grp.cag_aio_ops[i];
			if (cnt < cnt_per_op)
				cnt_per_op = cnt;

			off += set_exts(&aio->cao_ext, off, bsz);
			buf += bufvecw(&aio->cao_data, buf, bsz, cnt_per_op);

			/* Launch IO op. */
			rc = write_data_to_object_async(aio);
			if (rc != 0) {
				fprintf(stderr, "%s(): writing to object failed\n",
					__func__);
				break;
			}
			nr_ops_sent++;

			/* QOS */
			pthread_mutex_lock(&qos_lock);
			qos_total_weight += cnt_per_op * bsz;
			pthread_mutex_unlock(&qos_lock);
			/* END */
		}

		/* Wait for all ops to complete. */
		for (i = 0; i < nr_ops_sent; i++)
			m0_semaphore_down(&aio_grp.cag_sem);

		/* Finalise ops and group. */
		rc = rc ?: aio_grp.cag_rc;
		for (i = 0; i < nr_ops_sent; i++)
			clovis_aio_op_fini_free(aio_grp.cag_aio_ops + i);

		/* Not all ops are launched and executed successfully. */
		if (rc != 0)
			break;
	}
 fini:
	m0_clovis_entity_fini(&aio_grp.cag_obj.ob_entity);
	clovis_aio_opgrp_fini(&aio_grp);

	return rc;
}


/******************************************************************************
 * STATIC FUNCTIONS
 ******************************************************************************/

static uint64_t bufvecw(struct m0_bufvec *data, const char *buf,
			uint64_t bsz, uint32_t cnt)
{
	uint32_t i;

	if (cnt > data->ov_vec.v_nr)
		cnt = data->ov_vec.v_nr;
	/* copy block by block */
	for (i = 0; i < cnt; i++) {
		memmove(data->ov_buf[i], buf, bsz);
		buf += bsz;
	}

	return i * bsz;
}

static uint64_t bufvecr(const struct m0_bufvec *data, char *buf,
			uint64_t bsz, uint32_t cnt)
{
	uint32_t i;

	if (cnt > data->ov_vec.v_nr)
		cnt = data->ov_vec.v_nr;
	/* copy block by block */
	for (i = 0; i < cnt; i++) {
		memmove(buf, data->ov_buf[i], bsz);
		buf += bsz;
	}

	return i * bsz;
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

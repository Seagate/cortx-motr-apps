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
 *
 * Subsequent Modifications: Abhishek Saha <abhishek.saha@seagate.com>
 * Date of Modification: 02-Nov-2018
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <lib/semaphore.h>
#include <lib/trace.h>
#include <lib/memory.h>
#include <lib/mutex.h>
#include <sys/user.h>           /* PAGE_SIZE */

#include "c0appz.h"
#include "clovis/clovis.h"
#include "clovis/clovis_internal.h"
#include "clovis/clovis_idx.h"
#include "conf/confc.h"       /* m0_confc_open_sync */
#include "conf/dir.h"         /* m0_conf_dir_len */
#include "conf/helpers.h"     /* m0_confc_root_open */
#include "conf/diter.h"       /* m0_conf_diter_next_sync */
#include "conf/obj_ops.h"     /* M0_CONF_DIREND */
#include "spiel/spiel.h"      /* m0_spiel_process_lib_load */
#include "reqh/reqh.h"        /* m0_reqh */
#include "lib/types.h"        /* uint32_t */

#include "iscservice/isc.h"
#include "lib/buf.h"
#include "rpc/rpclib.h"
#include "c0appz_isc.h"

#ifndef DEBUG
#define DEBUG 0
#endif

#define MAXC0RC                	4
#define SZC0RCSTR              	256
#define SZC0RCFILE             	256
#define C0RCFLE                 "./.cappzrc"

/* static variables */
static struct m0_clovis          *clovis_instance = NULL;
static struct m0_clovis_container clovis_container;
static struct m0_clovis_config    clovis_conf;
static struct m0_idx_dix_config   dix_conf;
static struct m0_spiel            spiel_inst;

static char    c0rc[8][SZC0RCSTR];
static char    c0rcfile[SZC0RCFILE] = C0RCFLE;

struct clovis_aio_op;
struct clovis_aio_opgrp {
	struct m0_semaphore   cag_sem;

	struct m0_mutex       cag_mlock;
	uint32_t              cag_blocks_to_write;
	uint32_t              cag_blocks_written;
	int                   cag_rc;

	struct m0_clovis_obj  cag_obj;
	struct clovis_aio_op *cag_aio_ops;
};

struct clovis_aio_op {
	struct clovis_aio_opgrp *cao_grp;

	struct m0_clovis_op     *cao_op;
	struct m0_indexvec       cao_ext;
	struct m0_bufvec         cao_data;
	struct m0_bufvec         cao_attr;
};

/*
 ******************************************************************************
 * STATIC FUNCTION PROTOTYPES
 ******************************************************************************
 */
static char *trim(char *str);
static size_t read_data_from_file(FILE *fp, struct m0_bufvec *data, size_t bsz);
static size_t write_data_to_file(FILE *fp, struct m0_bufvec *data, size_t bsz);


static int write_data_to_object_async(struct clovis_aio_op *aio);
static int clovis_aio_vec_alloc(struct clovis_aio_op *aio,
				uint32_t blk_count, uint64_t blk_size);
static void clovis_aio_vec_free(struct clovis_aio_op *aio);
static int clovis_aio_opgrp_init(struct clovis_aio_opgrp *grp,
				 uint32_t blk_cnt, uint32_t op_cnt);
static void clovis_aio_opgrp_fini(struct clovis_aio_opgrp *grp);
static void clovis_aio_executed_cb(struct m0_clovis_op *op);
static void clovis_aio_stable_cb(struct m0_clovis_op *op);
static void clovis_aio_failed_cb(struct m0_clovis_op *op);

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */
struct m0_clovis_realm clovis_uber_realm;
unsigned unit_size = 0;
int perf=0;				/* performance option 		*/
extern int qos_total_weight; 		/* total bytes read or written 	*/
extern pthread_mutex_t qos_lock;	/* lock  qos_total_weight 	*/
enum {MAX_M0_BUFSZ = 128*1024*1024};    /* max bs for object store I/O  */

/*
 ******************************************************************************
 * EXTERN FUNCTIONS
 ******************************************************************************
 */

static uint64_t roundup_power2(uint64_t x)
{
	uint64_t power = 1;

	while (power < x)
		power *= 2;

	return power;
}

/**
 * Calculate the optimal block size for the object store I/O
 */
uint64_t c0appz_m0bs(uint64_t obj_sz, struct m0_fid *pool)
{
	int                     rc;
	unsigned long           usz; /* unit size */
	unsigned long           gsz; /* data units in parity group */
	uint64_t                max_bs;
	unsigned                lid;
	struct m0_reqh         *reqh = &clovis_instance->m0c_reqh;
	struct m0_pool_version *pver;
	struct m0_pdclust_attr *pa;

	if (obj_sz > MAX_M0_BUFSZ)
		obj_sz = MAX_M0_BUFSZ;

	rc = m0_pool_version_get(reqh->rh_pools, pool, &pver);
	if (rc != 0) {
		fprintf(stderr, "%s: m0_pool_version_get() failed: rc=%d\n",
			__func__, rc);
		return 0;
	}

	if (unit_size)
		lid = m0_clovis_layout_id(clovis_instance);
	else
		lid = m0_layout_find_by_buffsize(&reqh->rh_ldom, &pver->pv_id,
						 obj_sz);

	usz = m0_clovis_obj_layout_id_to_unit_size(lid);
	pa = &pver->pv_attr;
	gsz = usz * pa->pa_N;
	/* max 4-times pool-width deep, otherwise we may get -E2BIG */
	max_bs = usz * 4 * pa->pa_P * pa->pa_N / (pa->pa_N + 2 * pa->pa_K);

	if (obj_sz >= max_bs)
		return max_bs;
	else if (obj_sz <= gsz)
		return gsz;
	else
		return roundup_power2(obj_sz);
}

static void free_segs(struct m0_indexvec *ext, struct m0_bufvec *data,
		      struct m0_bufvec *attr)
{
	m0_indexvec_free(ext);
	m0_bufvec_free(data);
	m0_bufvec_free(attr);
}

static int alloc_segs(struct m0_indexvec *ext, struct m0_bufvec *data,
		      struct m0_bufvec *attr, uint64_t cnt, uint64_t bsz)
{
	int i, rc;

	rc = m0_bufvec_alloc(data, cnt, bsz) ?:
		m0_bufvec_alloc(attr, cnt, 1) ?:
		m0_indexvec_alloc(ext, cnt);
	if (rc != 0)
		goto err;

	/* We don't want any attributes */
	for (i = 0; i < cnt; i++)
		attr->ov_vec.v_count[i] = 0;

	return 0;
err:
	free_segs(ext, data, attr);
	return rc;
}

/*
 * c0appz_cp()
 * copy to an object.
 */
int c0appz_cp(uint64_t idhi, uint64_t idlo, char *filename,
	      uint64_t bsz, uint64_t cnt, uint64_t m0bs)
{
	int                rc=0;
	uint32_t           i;
	uint32_t           cnt_per_op;
	uint64_t           off = 0;
	struct m0_uint128  id = {idhi, idlo};
	struct m0_indexvec ext;
	struct m0_bufvec   data;
	struct m0_bufvec   attr;
	FILE              *fp;
	m0_time_t          st;
	m0_time_t          read_time = 0;
	m0_time_t          write_time = 0;
	double             time;
	double             fs_bw;
	double             clovis_bw;
	struct m0_clovis_obj obj = {};

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

	/* Open src file */
	fp = fopen(filename, "rb");
	if (fp == NULL) {
		fprintf(stderr,"%s(): error on opening input file %s: %s\n",
			__func__, filename, strerror(errno));
		return -errno;
	}

	/* Allocate data buffers, bufvec and indexvec for write */
	rc = alloc_segs(&ext, &data, &attr, cnt_per_op, bsz);
	if (rc != 0)
		goto out;

	/* Open the object entity we want to write to */
	m0_clovis_obj_init(&obj, &clovis_uber_realm, &id, M0_DEFAULT_LAYOUT_ID);
	rc = open_entity(&obj.ob_entity);
	if (rc != 0) {
		fprintf(stderr, "%s(): open_entity() failed: rc=%d\n",
			__func__, rc);
		goto free_vecs;
	}

	while (cnt > 0) {
		/*
		 * Prepare indexvec for write: <clovis_block_count> from the
		 * beginning of the object.
		 */
		if (cnt < cnt_per_op) {
			free_segs(&ext, &data, &attr);
			rc = alloc_segs(&ext, &data, &attr, cnt, bsz);
			if (rc != 0)
				break;
			cnt_per_op = cnt;
		}
		for (i = 0; i < cnt_per_op; i++) {
			ext.iv_index[i] = off;
			ext.iv_vec.v_count[i] = bsz;
			off += bsz;
		}

		/* Read data from source file. */
		st = m0_time_now();
		if ((rc = read_data_from_file(fp, &data, bsz)) != cnt_per_op) {
			fprintf(stderr, "%s(): reading from %s failed: "
				"expected %u, got %u records\n",
				__func__, filename, cnt_per_op, rc);
			rc = -EIO;
			break;
		}
		ext.iv_vec.v_count[cnt_per_op -1] =
			data.ov_vec.v_count[cnt_per_op -1];
		read_time = m0_time_add(read_time,
					m0_time_sub(m0_time_now(), st));

		/* Copy data to the object*/
		st = m0_time_now();
		rc = write_data_to_object(&obj, &ext, &data, &attr);
		if (rc != 0)
			fprintf(stderr, "%s(): writing to object failed\n",
				__func__);
		write_time = m0_time_add(write_time,
					 m0_time_sub(m0_time_now(), st));

		/* QOS */
		pthread_mutex_lock(&qos_lock);
		qos_total_weight += cnt_per_op * bsz;
		pthread_mutex_unlock(&qos_lock);
		/* END */

		if (rc != 0)
			break;

		cnt -= cnt_per_op;
	}

	m0_clovis_entity_fini(&obj.ob_entity);
free_vecs:
	free_segs(&ext, &data, &attr);
out:
	fclose(fp);

	if (perf && rc == 0) {
		time = (double) read_time / M0_TIME_ONE_SECOND;
		fs_bw = off / 1000000.0 / time;
		ppf("Mero I/O[ \033[0;31mOSFS: %10.4lf s %10.4lf MB/s\033[0m ]",time, fs_bw);
		time = (double) write_time / M0_TIME_ONE_SECOND;
		clovis_bw = off / 1000000.0 / time;
		ppf("[ \033[0;31mMERO: %10.4lf s %10.4lf MB/s\033[0m ]\n",time, clovis_bw);
	}

	return rc;
}

/*
 * c0appz_cp_async()
 * copy to an object from a file in an asynchronous manner
 */
int c0appz_cp_async(uint64_t idhi, uint64_t idlo, char *src, uint64_t bsz,
		    uint64_t cnt, uint32_t op_cnt, uint64_t m0bs)
{
	int                       rc = 0;
	uint32_t                  i;
	uint32_t                  nr_ops_sent;
	uint32_t                  cnt_per_op;
	uint64_t                  off = 0;
	struct m0_uint128         id = {idhi, idlo};
	struct clovis_aio_op     *aio;
	struct clovis_aio_opgrp   aio_grp;
	FILE                     *fp;

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

	/* open file */
	fp = fopen(src, "rb");
	if (fp == NULL) {
		fprintf(stderr,"%s(): failed to open output file %s: %s\n",
			__func__, src, strerror(errno));
		return -errno;
	}

	rc = clovis_aio_opgrp_init(&aio_grp, op_cnt, op_cnt);
	if (rc != 0)
		goto out;

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
		/* Initialise operation group. */
		if (cnt < op_cnt)
			op_cnt = cnt;

		/* Set each op. */
		nr_ops_sent = 0;
		for (i = 0; i < op_cnt; i++) {
			aio = &aio_grp.cag_aio_ops[i];
			if (cnt < cnt_per_op)
				cnt_per_op = cnt;
			rc = clovis_aio_vec_alloc(aio, cnt_per_op, bsz);
			if (rc != 0)
				break;

			for (i = 0; i < cnt_per_op; i++) {
				aio->cao_ext.iv_index[i] = off;
				aio->cao_ext.iv_vec.v_count[i] = bsz;
				off += bsz;

				aio->cao_attr.ov_vec.v_count[i] = 0;
			}

			/* Read data from source file. */
			if ((rc = read_data_from_file(fp, &aio->cao_data, bsz))
			    != cnt_per_op) {
				fprintf(stderr, "%s(): reading from %s failed"
					": expected %u, read %u records\n",
					__func__, src, cnt_per_op, rc);
				rc = -EIO;
				break;
			}

			/* Launch IO op. */
			rc = write_data_to_object_async(aio);
			if (rc != 0) {
				fprintf(stderr, "%s(): writing to object failed\n",
					__func__);
				break;
			}
			nr_ops_sent++;
		}

		/* Wait for all ops to complete. */
		for (i = 0; i < nr_ops_sent; i++)
			m0_semaphore_down(&aio_grp.cag_sem);

		/* Finalise ops and group. */
		rc = rc ?: aio_grp.cag_rc;
		for (i = 0; i < nr_ops_sent; i++) {
			aio = &aio_grp.cag_aio_ops[i];
			m0_clovis_op_fini(aio->cao_op);
			m0_clovis_op_free(aio->cao_op);
			clovis_aio_vec_free(aio);
		}

		/* Not all ops are launched and executed successfully. */
		if (rc != 0)
			break;

		/* QOS */
		pthread_mutex_lock(&qos_lock);
		qos_total_weight += op_cnt * bsz;
		pthread_mutex_unlock(&qos_lock);
		/* END */

		cnt -= nr_ops_sent * cnt_per_op;
	}

fini:
	m0_clovis_entity_fini(&aio_grp.cag_obj.ob_entity);
	clovis_aio_opgrp_fini(&aio_grp);
out:
	fclose(fp);
	return rc;
}

/*
 * c0appz_cat()
 * cat object.
 */
int c0appz_ct(uint64_t idhi, uint64_t idlo, char *filename,
	      uint64_t bsz, uint64_t cnt, uint64_t m0bs)
{
	int                rc=0;
	uint32_t           i;
	uint32_t           cnt_per_op;
	uint64_t           off = 0;
	struct m0_uint128  id = {idhi, idlo};
	struct m0_indexvec ext;
	struct m0_bufvec   data;
	struct m0_bufvec   attr;
	FILE              *fp;
	m0_time_t          st;
	m0_time_t          read_time = 0;
	m0_time_t          write_time = 0;
	double             time;
	double             fs_bw;
	double             clovis_bw;
	struct m0_clovis_obj obj = {};

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

	/* open output file */
	fp = fopen(filename, "w");
	if (fp == NULL) {
		fprintf(stderr,"error! could not open output file %s\n", filename);
		return 1;
	}

	/* Allocate data buffers, bufvec and indexvec for write. */
	rc = alloc_segs(&ext, &data, &attr, cnt_per_op, bsz);
	if (rc != 0)
		goto out;

	/* Open object. */
	m0_clovis_obj_init(&obj, &clovis_uber_realm, &id, M0_DEFAULT_LAYOUT_ID);
	rc = open_entity(&obj.ob_entity);
	if (rc != 0) {
		fprintf(stderr, "%s(): open_entity() failed: rc=%d\n",
			__func__, rc);
		goto free_vecs;
	}

	while (cnt > 0) {
		/*
		 * Prepare indexvec for write: <clovis_block_count> from the
		 * beginning of the object.
		 */
		if (cnt < cnt_per_op) {
			free_segs(&ext, &data, &attr);
			rc = alloc_segs(&ext, &data, &attr, cnt, bsz);
			if (rc != 0)
				break;
			cnt_per_op = cnt;
		}
		for (i = 0; i < cnt_per_op; i++) {
			ext.iv_index[i] = off;
			ext.iv_vec.v_count[i] = bsz;
			off += bsz;

			/* We don't want any attributes */
			attr.ov_vec.v_count[i] = 0;
		}

		/* Copy data from the object*/
		st = m0_time_now();
		rc = read_data_from_object(&obj, &ext, &data, &attr);
		read_time = m0_time_add(read_time,
					m0_time_sub(m0_time_now(), st));
		if (rc != 0) {
			fprintf(stderr, "%s(): reading from object failed: rc=%d\n",
				__func__, rc);
			break;
		}

		/* Output data to file. */
		st = m0_time_now();
		if (write_data_to_file(fp, &data, bsz) != cnt_per_op) {
			rc = -EIO;
			break;
		}
		write_time = m0_time_add(write_time,
					 m0_time_sub(m0_time_now(), st));

		/* QOS */
		pthread_mutex_lock(&qos_lock);
		qos_total_weight += cnt_per_op * bsz;
		pthread_mutex_unlock(&qos_lock);
		/* END */

		cnt -= cnt_per_op;
	}

	m0_clovis_entity_fini(&obj.ob_entity);
free_vecs:
	free_segs(&ext, &data, &attr);
out:
	/* block */
	qos_pthread_cond_wait();
	fprintf(stderr,"writing to file...\n");

	/* Fs will flush data back to device when closing a file. */
	st = m0_time_now();
	fclose(fp);
	write_time = m0_time_add(write_time,
				 m0_time_sub(m0_time_now(), st));
	if (perf && rc == 0) {
		time = (double) read_time / M0_TIME_ONE_SECOND;
		clovis_bw = off / 1000000.0 / time;
		ppf("Mero I/O[ \033[0;31mMERO: %10.4lf s %10.4lf MB/s\033[0m ]",time, clovis_bw);
		time = (double) write_time / M0_TIME_ONE_SECOND;
		fs_bw = off / 1000000.0 / time;
		ppf("[ \033[0;31mOSFS: %10.4lf s %10.4lf MB/s\033[0m ]\n",time, fs_bw);
	}

	return rc;
}

/*
 * c0appz_rm()
 * delete object.
 */
int c0appz_rm(uint64_t idhi,uint64_t idlo)
{
	int                  rc;
	struct m0_clovis_obj obj = {};
	struct m0_clovis_op *ops[1] = {NULL};
	struct m0_uint128    id;

	id.u_hi = idhi;
	id.u_lo = idlo;

	m0_clovis_obj_init(&obj, &clovis_uber_realm, &id, M0_DEFAULT_LAYOUT_ID);
	rc = open_entity(&obj.ob_entity);
	if (rc < 0) {
		fprintf(stderr,"error! [%d]\n", rc);
		fprintf(stderr,"object not found\n");
		return rc;
	}

	m0_clovis_entity_delete(&obj.ob_entity, &ops[0]);
	m0_clovis_op_launch(ops, ARRAY_SIZE(ops));
	rc = m0_clovis_op_wait(ops[0], M0_BITS(M0_CLOVIS_OS_FAILED,
					       M0_CLOVIS_OS_STABLE),
				   M0_TIME_NEVER);

	m0_clovis_op_fini(ops[0]);
	m0_clovis_op_free(ops[0]);
	m0_clovis_entity_fini(&obj.ob_entity);

	return rc;
}

int c0appz_rmach_bulk_cutoff(struct m0_rpc_link *link, uint32_t *bulk_cutoff)
{
	if (link == NULL || bulk_cutoff == NULL)
		return -EINVAL;
	*bulk_cutoff = link->rlk_conn.c_rpc_machine->rm_bulk_cutoff;	
	return 0;
}

static int c0appz_spiel_prepare(struct m0_spiel *spiel)
{
	struct m0_reqh *reqh = &clovis_instance->m0c_reqh;
	int             rc;

	rc = m0_spiel_init(spiel, reqh);
	if (rc != 0) {
		fprintf(stderr, "error! spiel initialisation failed.\n");
		return rc;
	}

	rc = m0_spiel_cmd_profile_set(spiel, clovis_conf.cc_profile);
	if (rc != 0) {
		fprintf(stderr, "error! spiel initialisation failed.\n");
		return rc;
	}
	rc = m0_spiel_rconfc_start(spiel, NULL);
	if (rc != 0) {
		fprintf(stderr, "error! starting of rconfc failed in spiel failed.\n");
		return rc;
	}

	return 0;
}

static void c0appz_spiel_destroy(struct m0_spiel *spiel)
{
	m0_spiel_rconfc_stop(spiel);
	m0_spiel_fini(spiel);
}

static bool conf_obj_is_svc(const struct m0_conf_obj *obj)
{
	return m0_conf_obj_type(obj) == &M0_CONF_SERVICE_TYPE;
}

struct m0_rpc_link * c0appz_isc_rpc_link_get(struct m0_fid *svc_fid)
{
	struct m0_reqh *reqh = &clovis_instance->m0c_reqh;
	struct m0_reqh_service_ctx *ctx;
	struct m0_pools_common *pc = reqh->rh_pools;

	m0_tl_for(pools_common_svc_ctx, &pc->pc_svc_ctxs, ctx) {
		if (ctx->sc_type == M0_CST_ISCS &&
		    m0_fid_eq(&ctx->sc_fid, svc_fid))
			return &ctx->sc_rlink;
	} m0_tl_endfor;
	return NULL;
}
/*
 * Loads a library into m0d instances.
 */
int c0appz_isc_api_register(const char *libpath)
{
	int                     rc;
	struct m0_reqh         *reqh = &clovis_instance->m0c_reqh;
	struct m0_confc        *confc;
	struct m0_conf_root    *root;
	struct m0_conf_process *proc;
	struct m0_conf_service *svc;
	struct m0_conf_diter    it;

	rc = c0appz_spiel_prepare(&spiel_inst);
	if (rc != 0) {
		fprintf(stderr, "error! spiel initialization failed");
		return rc;
	}

	confc = m0_reqh2confc(reqh);
	rc = m0_confc_root_open(confc, &root);
	if (rc != 0) {
		c0appz_spiel_destroy(&spiel_inst);
		return rc;
	}

	rc = m0_conf_diter_init(&it, confc,
				&root->rt_obj,
				M0_CONF_ROOT_NODES_FID,
				M0_CONF_NODE_PROCESSES_FID,
				M0_CONF_PROCESS_SERVICES_FID);
	if (rc != 0) {
		m0_confc_close(&root->rt_obj);
		c0appz_spiel_destroy(&spiel_inst);
		return rc;
	}

	while (M0_CONF_DIRNEXT ==
	       (rc = m0_conf_diter_next_sync(&it, conf_obj_is_svc))) {

		svc = M0_CONF_CAST(m0_conf_diter_result(&it), m0_conf_service);
		if (svc->cs_type != M0_CST_ISCS)
			continue;
		proc = M0_CONF_CAST(m0_conf_obj_grandparent(&svc->cs_obj),
				    m0_conf_process);
		rc = m0_spiel_process_lib_load(&spiel_inst, &proc->pc_obj.co_id,
					       libpath);
		if (rc != 0) {
			fprintf(stderr, "error! loading the library %s failed "
				        "for process "FID_F": rc=%d\n", libpath,
					FID_P(&proc->pc_obj.co_id), rc);
			m0_conf_diter_fini(&it);
			m0_confc_close(&root->rt_obj);
			c0appz_spiel_destroy(&spiel_inst);
			return rc;
		}
	}
	m0_conf_diter_fini(&it);
	m0_confc_close(&root->rt_obj);
	c0appz_spiel_destroy(&spiel_inst);
	return 0;
}

int c0appz_isc_nxt_svc_get(struct m0_fid *svc_fid, struct m0_fid *nxt_fid,
			   enum m0_conf_service_type s_type)
{
	struct m0_reqh             *reqh       = &clovis_instance->m0c_reqh;
	struct m0_reqh_service_ctx *ctx;
	struct m0_pools_common     *pc         = reqh->rh_pools;
	struct m0_fid               null_fid   = M0_FID0;
	struct m0_fid               current_fid = *svc_fid;

	m0_tl_for(pools_common_svc_ctx, &pc->pc_svc_ctxs, ctx) {
		if (ctx->sc_type == s_type && m0_fid_eq(&current_fid,
							&null_fid)) {
			*nxt_fid = ctx->sc_fid;
			return 0;
		} else if (ctx->sc_type == s_type &&
		           m0_fid_eq(svc_fid, &ctx->sc_fid))
			current_fid = null_fid;
	} m0_tl_endfor;
	*nxt_fid = M0_FID0;
	return -ENOENT;
}

int c0appz_isc_req_prepare(struct c0appz_isc_req *req, struct m0_buf *args,
			   const struct m0_fid *comp_fid,
			   struct m0_buf *reply_buf, struct m0_fid *svc_fid,
			   uint32_t reply_len)
{

	struct m0_fop_isc  *fop_isc = &req->cir_isc_fop;
	struct m0_fop      *arg_fop = &req->cir_fop;
	struct m0_rpc_link *link;
	int                 rc;

	req->cir_args = args;
	req->cir_result = reply_buf;
	fop_isc->fi_comp_id = *comp_fid;
	req->cir_rpc_link = c0appz_isc_rpc_link_get(svc_fid);
	if (req->cir_rpc_link == NULL) {
		fprintf(stderr, "error! isc request can not be prepared for"
			"process "FID_F, FID_P(svc_fid));
		return -EINVAL;
	}
	link = req->cir_rpc_link;
	m0_rpc_at_init(&fop_isc->fi_args);
	rc = m0_rpc_at_add(&fop_isc->fi_args, args, &link->rlk_conn);
	if (rc != 0) {
		m0_rpc_at_fini(&fop_isc->fi_args);
		fprintf(stderr, "error! m0_rpc_at_add() failed with %d\n", rc);
		return rc;
	}
	/* Initialise the reply RPC AT buffer to be received.*/
	m0_rpc_at_init(&fop_isc->fi_ret);
	rc = m0_rpc_at_recv(&fop_isc->fi_ret, &link->rlk_conn,
			    reply_len, false);
	if (rc != 0) {
		m0_rpc_at_fini(&fop_isc->fi_args);
		m0_rpc_at_fini(&fop_isc->fi_ret);
		fprintf(stderr, "error! m0_rpc_at_recv() failed with %d\n", rc);
		return rc;
	}
	m0_fop_init(arg_fop, &m0_fop_isc_fopt, fop_isc, m0_fop_release);
	return rc;
}

int c0appz_isc_req_send_sync(struct c0appz_isc_req *req)
{
	struct m0_fop         *reply_fop;
	struct m0_fop_isc_rep  isc_reply;
	int                    rc;

	rc = m0_rpc_post_sync(&req->cir_fop, &req->cir_rpc_link->rlk_sess, NULL,
			      M0_TIME_IMMEDIATELY);
	if (rc != 0) {
		fprintf(stderr, "error! request could not be sent");
		return rc;
	}
	reply_fop = m0_rpc_item_to_fop(req->cir_fop.f_item.ri_reply);
	isc_reply = *(struct m0_fop_isc_rep *)m0_fop_data(reply_fop);
	req->cir_rc = isc_reply.fir_rc;
	rc = m0_rpc_at_rep_get(&req->cir_isc_fop.fi_ret, &isc_reply.fir_ret,
			       req->cir_result);
	if (rc != 0)
		fprintf(stderr, "\nerror! m0_rpc_at_get returned %d\n", rc);
	return req->cir_rc == 0 ? rc : req->cir_rc;
}

void c0appz_isc_req_fini(struct c0appz_isc_req *req)
{
	struct m0_fop *reply_fop;

	reply_fop = m0_rpc_item_to_fop(req->cir_fop.f_item.ri_reply);
	if (reply_fop != NULL)
		m0_fop_put_lock(reply_fop);
	m0_rpc_at_fini(&req->cir_isc_fop.fi_args);
	m0_rpc_at_fini(&req->cir_isc_fop.fi_ret);
}

/*
 * c0appz_ex()
 * object exists test.
 */
int c0appz_ex(uint64_t idhi,uint64_t idlo)
{
	int                  rc;
	struct m0_clovis_obj obj = {};
	struct m0_uint128    id;

	id.u_hi = idhi;
	id.u_lo = idlo;

	m0_clovis_obj_init(&obj, &clovis_uber_realm, &id, M0_DEFAULT_LAYOUT_ID);
	rc = open_entity(&obj.ob_entity);
	if (rc != 0)
		return 0;

	/* success */
	return 1;
}

/*
 * c0appz_init()
 * init clovis resources.
 */
int c0appz_init(int idx)
{
	int   i;
	int   rc;
	int   lid;
	FILE *fp;
	char *str = NULL;
	char* filename = C0RCFLE;
	char  buf[SZC0RCSTR];

	/* read c0rc file */
	filename = c0rcfile;
	fp = fopen(filename, "r");
	if (fp == NULL) {
		fprintf(stderr,"error! could not open resource file %s\n", filename);
		return 1;
	}

	/* move fp */
	i = 0;
	while ((i != idx * MAXC0RC) && (fgets(buf, SZC0RCSTR, fp) != NULL)) {
		str = trim(buf);
		if (str[0] == '#') continue;     /* ignore comments */
		if (strlen(str) == 0) continue;  /* ignore empty space */
		i++;
	}

	/* read c0rc */
	i = 0;
	while (fgets(buf, SZC0RCSTR, fp) != NULL) {

#if DEBUG
	fprintf(stderr,"rd:->%s<-", buf);
	fprintf(stderr,"\n");
#endif

	str = trim(buf);
	if (str[0] == '#') continue;		/* ignore comments */
	if (strlen(str) == 0) continue;		/* ignore empty space */

	memset(c0rc[i], 0x00, SZC0RCSTR);
	strncpy(c0rc[i], str, SZC0RCSTR);

#if DEBUG
	fprintf(stderr,"wr:->%s<-", c0rc[i]);
	fprintf(stderr,"\n");
#endif

	i++;
	if (i == MAXC0RC) break;
	}
	fclose(fp);

	/* idx check */
	if (i != MAXC0RC) {
		fprintf(stderr,"error! [[ %d ]] wrong resource index.\n", idx);
		return 2;
	}

	clovis_conf.cc_is_oostore            = true;
	clovis_conf.cc_is_read_verify        = false;
	clovis_conf.cc_local_addr            = c0rc[0];
	clovis_conf.cc_ha_addr               = c0rc[1];
	clovis_conf.cc_profile               = c0rc[2];
	clovis_conf.cc_process_fid           = c0rc[3];
#if 0
	/* set to default values */
	clovis_conf.cc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	clovis_conf.cc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
#endif
	/* set to Sage cluster specific values */
	clovis_conf.cc_tm_recv_queue_min_len = 64;
	clovis_conf.cc_max_rpc_msg_size      = 65536;
	clovis_conf.cc_layout_id             = 0;
	if (unit_size) {
		lid = m0_clovis_obj_unit_size_to_layout_id(unit_size * 1024);
		if (lid == 0) {
			fprintf(stderr, "invalid unit size %u, it should be: "
				"power of 2, >= 4 and <= 4096\n", unit_size);
			return -EINVAL;
		}
		clovis_conf.cc_layout_id = lid;
	}

	/* IDX_MERO */
	clovis_conf.cc_idx_service_id   = M0_CLOVIS_IDX_DIX;
	dix_conf.kc_create_meta = false;
	clovis_conf.cc_idx_service_conf = &dix_conf;

#if DEBUG
	fprintf(stderr,"\n---\n");
	fprintf(stderr,"%s,", (char *)clovis_conf.cc_local_addr);
	fprintf(stderr,"%s,", (char *)clovis_conf.cc_ha_addr);
	fprintf(stderr,"%s,", (char *)clovis_conf.cc_profile);
	fprintf(stderr,"%s,", (char *)clovis_conf.cc_process_fid);
	fprintf(stderr,"%s,", (char *)clovis_conf.cc_idx_service_conf);
	fprintf(stderr,"\n---\n");
#endif

	/* clovis instance */
	rc = m0_clovis_init(&clovis_instance, &clovis_conf, true);
	if (rc != 0) {
		fprintf(stderr,"failed to initilise Clovis\n");
		return rc;
	}

	/* And finally, clovis root realm */
	m0_clovis_container_init(&clovis_container,
				 NULL, &M0_CLOVIS_UBER_REALM,
				 clovis_instance);
	rc = clovis_container.co_realm.re_entity.en_sm.sm_rc;
	if (rc != 0) {
		fprintf(stderr,"failed to open uber realm\n");
		return rc;
	}

	/* success */
	clovis_uber_realm = clovis_container.co_realm;
	return 0;
}

/*
 * c0appz_free()
 * free clovis resources.
 */
int c0appz_free(void)
{
	m0_clovis_fini(clovis_instance, true);
	memset(c0rcfile, 0, sizeof(c0rcfile));
	memset(c0rc, 0, sizeof(c0rc));
	return 0;
}

/*
 * c0appz_setrc()
 * set c0apps resource filename
 */
int c0appz_setrc(char *rcfile)
{
	char buf1[256];
	char buf2[256];

	/* null */
	if (!rcfile) {
		fprintf(stderr, "error! null rc filename.\n");
		return 1;
	}

	/* add hostname */
	memset(buf1, 0x00, 256);
	memset(buf2, 0x00, 256);
	gethostname(buf1, 256);
	sprintf(buf2,"%s/%s", rcfile, buf1);

	/* update rc filename */
	memset(c0rcfile, 0x00, SZC0RCFILE);
	strncpy(c0rcfile, buf2, strlen(buf2));

	/* success */
	return 0;
}

/*
 * c0appz_putrc()
 * print c0apps resource filename
 */
int c0appz_putrc(void)
{
	/* print rc filename */
	fprintf(stderr, "%s", c0rcfile);
	fprintf(stderr, "\n");
	return 0;
}

/*
 * open_entity()
 * open clovis entity.
 */
int open_entity(struct m0_clovis_entity *entity)
{
	int                  rc;
	struct m0_clovis_op *op = NULL;

	m0_clovis_entity_open(entity, &op);
	m0_clovis_op_launch(&op, 1);
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_FAILED,
					       M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER) ?: m0_clovis_rc(op);
	m0_clovis_op_fini(op);
	m0_clovis_op_free(op);

	return rc;
}

/**
 * Create clovis object.
 */
int c0appz_cr(uint64_t idhi, uint64_t idlo, struct m0_fid *pool, uint64_t bsz)
{
	int                     rc;
	unsigned                lid;
	struct m0_reqh         *reqh = &clovis_instance->m0c_reqh;
	struct m0_pool_version *pver;
	struct m0_clovis_op    *op = NULL;
	struct m0_uint128       id = {idhi, idlo};
	struct m0_clovis_obj    obj = {};

	rc = m0_pool_version_get(reqh->rh_pools, pool, &pver);
	if (rc != 0) {
		fprintf(stderr, "%s: m0_pool_version_get() failed: rc=%d\n",
			__func__, rc);
		return rc;
	}

	if (unit_size)
		lid = m0_clovis_layout_id(clovis_instance);
	else
		lid = m0_layout_find_by_buffsize(&reqh->rh_ldom, &pver->pv_id,
						 bsz);

	printf("pool="FID_F" width=%u parity=(%u+%u) unit=%dK m0bs=%luK\n",
	       FID_P(&pver->pv_pool->po_id), pver->pv_attr.pa_P,
	       pver->pv_attr.pa_N, pver->pv_attr.pa_K,
	       m0_clovis_obj_layout_id_to_unit_size(lid) / 1024, bsz / 1024);

	m0_clovis_obj_init(&obj, &clovis_uber_realm, &id, lid);

	rc = open_entity(&obj.ob_entity);
	if (rc == 0) {
		fprintf(stderr,"%s: object already exists!\n", __func__);
		return 1;
	}

	/* create object */
	rc = m0_clovis_entity_create(pool, &obj.ob_entity, &op);
	if (rc != 0) {
		fprintf(stderr,"%s(): entity_create() failed: rc=%d\n",
			__func__, rc);
		goto err;
	}

	m0_clovis_op_launch(&op, 1);

	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_FAILED,
					   M0_CLOVIS_OS_STABLE),
			       m0_time_from_now(3,0)) ?: m0_clovis_rc(op);

	m0_clovis_op_fini(op);
	m0_clovis_op_free(op);
err:
	m0_clovis_entity_fini(&obj.ob_entity);

	if (rc != 0)
		fprintf(stderr,"%s() failed: rc=%d, check pool parameters\n",
			__func__, rc);
	return rc;
}

/*
 * read_data_from_file()
 * reads data from file and populates buffer.
 */
static size_t read_data_from_file(FILE *fp, struct m0_bufvec *data, size_t bsz)
{
	size_t i;
	size_t read;
	unsigned nr_blocks;

	nr_blocks = data->ov_vec.v_nr;
	for (i = 0; i < nr_blocks; i++) {
		read = fread(data->ov_buf[i], 1, bsz, fp);
		if (read < bsz) {
			if (read > 0)
				i++;
			break;
		}

		if (feof(fp))
			break;
	}

	return i;
}

static size_t write_data_to_file(FILE *fp, struct m0_bufvec *data, size_t bsz)
{
	size_t i;
	size_t wrtn;
	unsigned nr_blocks;

	nr_blocks = data->ov_vec.v_nr;
	for (i = 0; i < nr_blocks; i++) {
		wrtn = fwrite(data->ov_buf[i], 1, data->ov_vec.v_count[i], fp);
		if (wrtn != data->ov_vec.v_count[i])
			break;
	}

	return i;
}

/*
 * write_data_to_object()
 * writes data to an object
 */
int write_data_to_object(struct m0_clovis_obj *obj,
			 struct m0_indexvec *ext,
			 struct m0_bufvec *data,
			 struct m0_bufvec *attr)
{
	int                  rc;
	struct m0_clovis_op *op = NULL;

	/* Create the write request */
	m0_clovis_obj_op(obj, M0_CLOVIS_OC_WRITE,
			 ext, data, attr, 0, &op);

	/* Launch the write request*/
	m0_clovis_op_launch(&op, 1);

	/* wait */
	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_FAILED,
					   M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER) ?: m0_clovis_rc(op);

	m0_clovis_op_fini(op);
	m0_clovis_op_free(op);

	if (rc != 0)
		fprintf(stderr,"%s() failed: rc=%d\n", __func__, rc);

	return rc;
}

/*
 * write_data_to_object_async()
 * writes to and object in async manner
 */
static int write_data_to_object_async(struct clovis_aio_op *aio)
{
	struct m0_clovis_obj    *obj;
	struct m0_clovis_op_ops *op_ops;
	struct clovis_aio_opgrp *grp;

	grp = aio->cao_grp;
	obj = &grp->cag_obj;
	M0_ALLOC_PTR(op_ops);
	if (op_ops == NULL) {
		fprintf(stderr,"%s() failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	/* Create an WRITE op. */
	m0_clovis_obj_op(obj, M0_CLOVIS_OC_WRITE,
			 &aio->cao_ext, &aio->cao_data,
			 &aio->cao_attr, 0, &aio->cao_op);
	aio->cao_op->op_datum = aio;

	/* Set op's Callbacks */
	op_ops->oop_executed = clovis_aio_executed_cb;
	op_ops->oop_stable = clovis_aio_stable_cb;
	op_ops->oop_failed = clovis_aio_failed_cb;
	m0_clovis_op_setup(aio->cao_op, op_ops, 0);

	/* Launch the write request */
	m0_clovis_op_launch(&aio->cao_op, 1);

	return 0;
}

/*
 * read_data_from_object()
 * read data from an object
 */
int read_data_from_object(struct m0_clovis_obj *obj,
			  struct m0_indexvec *ext,
			  struct m0_bufvec *data,
			  struct m0_bufvec *attr)
{
	int                  rc;
	struct m0_clovis_op *op = NULL;

	/* Creat, launch and wait on an READ op. */
	m0_clovis_obj_op(obj, M0_CLOVIS_OC_READ,
			 ext, data, attr, 0, &op);

	m0_clovis_op_launch(&op, 1);

	rc = m0_clovis_op_wait(op, M0_BITS(M0_CLOVIS_OS_FAILED,
					   M0_CLOVIS_OS_STABLE),
			       M0_TIME_NEVER) ?: m0_clovis_rc(op);

	/* Finalise and release op. */
	m0_clovis_op_fini(op);
	m0_clovis_op_free(op);

	if (rc != 0)
		fprintf(stderr,"%s() failed: rc=%d\n", __func__, rc);

	return rc;
}


static int clovis_aio_vec_alloc(struct clovis_aio_op *aio,
				uint32_t blk_count, uint64_t blk_size)
{
	int rc = 0;

	rc = m0_bufvec_alloc(&aio->cao_data, blk_count, blk_size) ?:
	     m0_bufvec_alloc(&aio->cao_attr, blk_count, 1) ?:
	     m0_indexvec_alloc(&aio->cao_ext, blk_count);
	if (rc != 0) {
		m0_bufvec_free(&aio->cao_data);
		m0_bufvec_free(&aio->cao_attr);
	}
	return rc;
}

static void clovis_aio_vec_free(struct clovis_aio_op *aio)
{
	/* Free bufvec's and indexvec's */
	m0_indexvec_free(&aio->cao_ext);
	m0_bufvec_free(&aio->cao_data);
	m0_bufvec_free(&aio->cao_attr);
}

static int clovis_aio_opgrp_init(struct clovis_aio_opgrp *grp,
				 uint32_t blk_cnt, uint32_t op_cnt)
{
	int                   i;
	struct clovis_aio_op *ops;

	M0_SET0(grp);

	M0_ALLOC_ARR(ops, op_cnt);
	if (ops == NULL)
		return -ENOMEM;
	grp->cag_aio_ops = ops;
	for (i = 0; i < op_cnt; i++)
		ops[i].cao_grp = grp;

	m0_mutex_init(&grp->cag_mlock);
	m0_semaphore_init(&grp->cag_sem, 0);
	grp->cag_blocks_to_write = blk_cnt;
	grp->cag_blocks_written = 0;

	return 0;
}

static void clovis_aio_opgrp_fini(struct clovis_aio_opgrp *grp)
{
	m0_mutex_fini(&grp->cag_mlock);
	m0_semaphore_fini(&grp->cag_sem);
	grp->cag_blocks_to_write = 0;
	grp->cag_blocks_written = 0;

	m0_free(grp->cag_aio_ops);
}

static void clovis_aio_executed_cb(struct m0_clovis_op *op)
{
	/** Stuff to do when OP is in excecuted state */
}

static void clovis_aio_stable_cb(struct m0_clovis_op *op)
{
	struct clovis_aio_opgrp *grp;

	grp = ((struct clovis_aio_op *)op->op_datum)->cao_grp;
	m0_mutex_lock(&grp->cag_mlock);
	if (op->op_rc == 0) {
		grp->cag_blocks_written += 0;
		grp->cag_blocks_to_write   += 0;
		grp->cag_rc = grp->cag_rc ?: op->op_rc;
	}
	m0_mutex_unlock(&grp->cag_mlock);

	m0_semaphore_up(&grp->cag_sem);
}

static void clovis_aio_failed_cb(struct m0_clovis_op *op)
{
	struct clovis_aio_opgrp *grp;

	m0_console_printf("Write operation failed!");
	grp = ((struct clovis_aio_op *)op->op_datum)->cao_grp;
	m0_mutex_lock(&grp->cag_mlock);
	grp->cag_rc = grp->cag_rc ?: op->op_rc;
	m0_mutex_unlock(&grp->cag_mlock);

	m0_semaphore_up(&grp->cag_sem);
}

/*
 ******************************************************************************
 * UTILITY FUNCTIONS
 ******************************************************************************
 */

/*
 * trim()
 * trim leading/trailing white spaces.
 */
static char *trim(char *str)
{
	char *end;

	/* null */
	if (!str) return str;

	/* trim leading spaces */
	while (isspace((unsigned char)*str)) str++;

	/* all spaces */
	if (*str == 0) return str;

	/* trim trailing spaces, and */
	/* write new null terminator */
	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end)) end--;
	*(end + 1) = 0;

	/* success */
	return str;
}
/*
 ******************************************************************************
 * END
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

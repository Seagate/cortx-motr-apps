/* -*- C -*- */
/*
 * Copyright (c) 2018-2020 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 *
 * Original author:  Nachiket Sahasrabuddhe <nachiket.sahasrabuddhe@seagate.com>
 * Original creation date: 06-Sep-2018
 */

#include <libgen.h>      /* dirname */

#include "lib/memory.h"  /* m0_alloc m0_free */
#include "rpc/conn.h"    /* m0_rpc_conn_addr */
#include "rpc/session.h"    /* iop_session */
#include "xcode/xcode.h"
#include "conf/schema.h" /* M0_CST_ISCS */
#include "layout/layout.h"  /* M0_DEFAULT_LAYOUT_ID */
#include "layout/plan.h"  /* m0_layout_plan_build */

#include "c0appz.h"
#include "c0appz_internal.h" /* uber_realm */
#include "c0appz_isc.h"  /* c0appz_isc_req */
#include "isc/libdemo.h"     /* mm_result */
#include "isc/libdemo_xc.h"     /* isc_args_xc */


enum isc_comp_type {
	ICT_PING,
	ICT_MIN,
	ICT_MAX,
};

/** Arguments for min/max operations */
struct mm_args {
	/** Length of an array. */
	uint32_t  ma_len;
	/** Array of doubles. */
	double   *ma_arr;
	/** Number of isc services. */
	uint32_t  ma_svc_nr;
	/** Length of a chunk per service. */
	uint32_t  ma_chunk_len;
	/** A service currently fed with input. */
	uint32_t  ma_curr_svc_id;
};

static void fid_get(const char *f_name, struct m0_fid *fid)
{
	uint32_t f_key = m0_full_name_hash((const unsigned char*)f_name,
					    strlen(f_name));
	uint32_t f_cont = m0_full_name_hash((const unsigned char *)"libdemo",
					    strlen("libdemo"));

	m0_fid_set(fid, f_cont, f_key);
}

static int op_type_parse(const char *op_name)
{
	if (op_name == NULL)
		return -EINVAL;
	else if (!strcmp(op_name, "ping"))
		return ICT_PING;
	else if (!strcmp(op_name, "min"))
		return ICT_MIN;
	else if (!strcmp(op_name, "max"))
		return ICT_MAX;
	else
		return -EINVAL;

}

static int file_to_array(const char *file_name, void **arr, uint32_t *arr_len)
{
	FILE     *fd;
	uint32_t  i;
	int       rc;
	double   *val_arr;

	fd = fopen(file_name, "r");
	if (fd == NULL) {
		fprintf(stderr, "error! Could not open file c0isc_data\n");
		return -EINVAL;
	}
	fscanf(fd, "%d", arr_len);
	/* XXX: Fix sizeof (double) with appropriate macro. */
	M0_ALLOC_ARR(val_arr, *arr_len);
	for (i = 0; i < *arr_len; ++i) {
		rc = fscanf(fd, "%lf", &val_arr[i]);
		if (rc == EOF) {
			fprintf(stderr, "data file (%s) does not contain the "
				"specified number of elements: %d\n",
				file_name, *arr_len);
			m0_free(val_arr);
			fclose(fd);
			return -EINVAL;
		}
	}
	*arr = val_arr;
	fclose(fd);
	return 0;
}

static uint32_t isc_services_count(void)
{
	struct m0_fid start_fid = M0_FID0;
	struct m0_fid proc_fid;
	uint32_t      svc_nr = 0;
	int           rc = 0;

	while (rc == 0) {
		rc = c0appz_isc_nxt_svc_get(&start_fid, &proc_fid,
					    M0_CST_ISCS);
		if (rc == 0)
			++svc_nr;
		start_fid = proc_fid;
	}
	return svc_nr;
}

static int op_init(enum isc_comp_type type, void **inp_args)
{
	struct mm_args *in_info;
	double         *arr;
	uint32_t        arr_len;
	int             rc;

	switch (type) {
	case ICT_PING:
		return 0;
	case ICT_MIN:
	case ICT_MAX:
		M0_ALLOC_PTR(in_info);
		if (in_info == NULL)
			return -ENOMEM;
		rc = file_to_array("c0isc_data", (void **)&arr, &arr_len);
		if (rc != 0)
			return rc;
		in_info->ma_arr = arr;
		in_info->ma_len = arr_len;
		in_info->ma_curr_svc_id = 0;
		in_info->ma_svc_nr = isc_services_count();
		in_info->ma_chunk_len = arr_len / in_info->ma_svc_nr;
		*inp_args = in_info;

		return 0;
	default:
		fprintf(stderr, "Invalid operation %d\n", type);
		return EINVAL;
	}
}

static void op_fini(enum isc_comp_type op_type, struct mm_args *in_info,
		    void *out_args)
{
	switch (op_type) {
	case ICT_PING:
		break;
	case ICT_MIN:
	case ICT_MAX:
		if (in_info == NULL)
			break;
		m0_free(in_info->ma_arr);
		m0_free(in_info);
		m0_free(out_args);
	}
}

static int minmax_input_prepare(struct m0_buf *out, struct m0_fid *comp_fid,
				struct m0_layout_io_plop *iop,
				uint32_t *reply_len, enum isc_comp_type type)
{
	int           rc;
	struct m0_buf buf = M0_BUF_INIT0;
	struct isc_targs ta = {};

	if (iop->iop_ext.iv_vec.v_nr > 1) {
		fprintf(stderr, "no more than 1 segment for now\n");
		return -EINVAL;
	}
	ta.ist_cob = iop->iop_base.pl_ent;
	rc = m0_indexvec_mem2wire(&iop->iop_ext, 1, 0, &ta.ist_ioiv);
	if (rc != 0)
		return rc;
	rc = m0_xcode_obj_enc_to_buf(&M0_XCODE_OBJ(isc_targs_xc, &ta),
				     &buf.b_addr, &buf.b_nob);
	if (rc != 0)
		return rc;

	*out = M0_BUF_INIT0; /* to avoid panic */
	rc = m0_buf_copy_aligned(out, &buf, M0_0VEC_SHIFT);
	m0_buf_free(&buf);

	if (type == ICT_MIN)
		fid_get("arr_min", comp_fid);
	else
		fid_get("arr_max", comp_fid);

	*reply_len = CBL_DEFAULT_MAX;

	return rc;
}

static int ping_input_prepare(struct m0_buf *buf, struct m0_fid *comp_fid,
			      uint32_t *reply_len, enum isc_comp_type type)
{
	char *greeting;

	*buf = M0_BUF_INIT0;
	greeting = m0_strdup("Hello");
	if (greeting == NULL)
		return -ENOMEM;

	m0_buf_init(buf, greeting, strlen(greeting));
	fid_get("hello_world", comp_fid);
	*reply_len = CBL_DEFAULT_MAX;

	return 0;
}

static int input_prepare(struct m0_buf *buf, struct m0_fid *comp_fid,
			 struct m0_layout_io_plop *iop, uint32_t *reply_len,
			 enum isc_comp_type type)
{
	switch (type) {
	case ICT_PING:
		return ping_input_prepare(buf, comp_fid, reply_len, type);
	case ICT_MIN:
	case ICT_MAX:
		return minmax_input_prepare(buf, comp_fid, iop,
					    reply_len, type);
	}
	return -EINVAL;
}

static struct mm_result *
op_result(struct mm_result *x, struct mm_result *y, enum isc_comp_type op_type)
{
	int               rc;
	int               len;
	struct mm_result *res;
	char             *buf;
	double            x_rval;
	double            y_lval;

	len = x->mr_rbuf.i_len + y->mr_lbuf.i_len;
	buf = malloc(x->mr_rbuf.i_len + y->mr_lbuf.i_len + 1);
	if (buf == NULL) {
		fprintf(stderr, "failed to allocate %d of memory for result\n",
			                            len);
		return NULL;
	}

	memcpy(buf, x->mr_rbuf.i_buf, x->mr_rbuf.i_len);
	memcpy(buf + x->mr_rbuf.i_len, y->mr_lbuf.i_buf, y->mr_lbuf.i_len);
	buf[x->mr_rbuf.i_len + y->mr_lbuf.i_len] = '\0';

	rc = sscanf(buf, "%lf%n", &x_rval, &len);
	if (rc < 1) {
		fprintf(stderr, "failed to read the resulting xr-value\n");
		return NULL;
	}

	y->mr_idx     += x->mr_idx_max;
	y->mr_idx_max += x->mr_idx_max;

	//printf("buf=%s x_rval=%lf\n", buf, x_rval);

	rc = sscanf(buf + len, "%lf", &y_lval);
	if (rc < 1) {
		y_lval = x_rval;
	} else {
		y->mr_idx++;
		y->mr_idx_max++;
	}

	//printf("y_lval=%lf\n", y_lval);

	switch (op_type) {
	case ICT_MIN:
		res = x->mr_val <= y->mr_val ? x : y;
		res->mr_val = min3(res->mr_val, x_rval, y_lval);
		break;
	case ICT_MAX:
		res = x->mr_val >= y->mr_val ? x : y;
		res->mr_val = max3(res->mr_val, x_rval, y_lval);
		break;
	default:
		res = NULL;
	}

	m0_free(buf);

	return res;
}

enum elm_order {
	ELM_FIRST,
	ELM_LAST
};

static void set_idx(struct mm_result *res, enum elm_order e)
{
	if (ELM_FIRST == e)
		res->mr_idx = 0;
	else
		res->mr_idx = res->mr_idx_max;
}

static void check_edge_val(struct mm_result *res, enum elm_order e,
			   enum isc_comp_type type)
{
	const char *buf;
	double      val;

	if (ELM_FIRST == e)
		buf = res->mr_lbuf.i_buf;
	else // last
		buf = res->mr_rbuf.i_buf;

	if (sscanf(buf, "%lf", &val) < 1) {
		fprintf(stderr, "failed to parse egde value=%s\n", buf);
		return;
	}

	//printf("edge val=%lf\n", val);

	if (ICT_MIN == type && val < res->mr_val) {
		res->mr_val = val;
		set_idx(res, e);
	} else if (ICT_MAX == type && val > res->mr_val) {
		res->mr_val = val;
		set_idx(res, e);
	}
}

static void *minmax_output_prepare(struct m0_buf *result,
				   bool last_unit,
				   struct mm_result *prev,
				   enum isc_comp_type type)
{
	int               rc;
	struct mm_result  new = {};
	struct mm_result *res;

	rc = m0_xcode_obj_dec_from_buf(&M0_XCODE_OBJ(mm_result_xc, &new),
				       result->b_addr, result->b_nob);
	if (rc != 0) {
		fprintf(stderr, "failed to parse result: rc=%d\n", rc);
		goto out;
	}
	if (prev == NULL) {
		M0_ALLOC_PTR(prev);
		check_edge_val(&new, ELM_FIRST, type);
		*prev = new;
		goto out;
	}

	if (last_unit)
		check_edge_val(&new, ELM_LAST, type);

	/* Copy the current resulting value. */
	res = op_result(prev, &new, type);
	if (res == NULL)
		goto out;
	*prev = *res;
 out:
	/* Print the output. */
	if (last_unit)
		fprintf(stderr, "idx=%lu val=%lf\n",
			prev->mr_idx, prev->mr_val);

	m0_free(new.mr_lbuf.i_buf);
	m0_free(new.mr_rbuf.i_buf);

	return prev;
}

/**
 * Apart from processing the output this can deserialize the buffer into
 * a structure relevant to the result of invoked computation.
 * and return the same.
 */
static void* output_process(struct m0_buf *result, bool last,
			    void *out, enum isc_comp_type type)
{
	switch (type) {
	case ICT_PING:
		printf ("Hello-%s @%s\n", (char*)result->b_addr, (char*)out);
		memset(result->b_addr, 'a', result->b_nob);
		return NULL;
	case ICT_MIN:
	case ICT_MAX:
		return minmax_output_prepare(result, last, out, type);
	}
	return NULL;
}

char *prog;

const char *help_str = "\
Usage: %s op_name obj_id\n\
\n\
  Supported operations: ping, min, max.\n\
\n\
  obj_id is two uint64 numbers in format: hi:lo.\n\
\n\
  Refer to README.isc for more usage details.\n";

static void usage()
{
	fprintf(stderr, help_str, prog);
	exit(1);
}

/**
 * Read obj id in the format [hi:]lo.
 * Return how many numbers were read.
 */
int read_id(const char *s, struct m0_uint128 *id)
{
	int res;
	long long hi, lo;

	res = sscanf(s, "%lli:%lli", &hi, &lo);
	if (res == 1) {
		id->u_hi = 0;
		id->u_lo = hi;
	} else if (res == 2) {
		id->u_hi = hi;
		id->u_lo = lo;
	}

	return res;
}

int main(int argc, char **argv)
{
	int                    rc;
	struct m0_op          *op = NULL;
	void                  *inp_args; /* computation-specific input args */
	void                  *out_args; /* output specific to a computation. */
	struct m0_layout_plan *plan;
	struct m0_layout_plop *plop = NULL;
	struct m0_layout_plop *prev_plop;
	struct m0_layout_io_plop *iopl;
	const char            *conn_addr = NULL;
	struct m0_buf          buf;
	struct m0_fid          comp_fid;
	struct c0appz_isc_req *req;
	struct m0_uint128      obj_id;
	struct m0_indexvec     ext;
	struct m0_bufvec       data;
	struct m0_bufvec       attr;
	uint32_t               reply_len;
	int                    op_type;
	int                    segs_nr = 2;
	int                    reqs_nr = 0;
	struct m0_obj          obj = {};

	prog = basename(strdup(argv[0]));

	if (argc < 2)
		usage();

	c0appz_timein();

	c0appz_setrc(prog);
	c0appz_putrc();

	op_type = op_type_parse(argv[1]);
	if (op_type == -EINVAL)
		usage();
	if (argc < 3)
		usage();
	if (read_id(argv[2], &obj_id) < 1)
		usage();

	m0trace_on = true;

	rc = c0appz_init(0);
	if (rc != 0) {
		fprintf(stderr,"c0appz_init() failed: %d\n", rc);
		usage();
	}

	if (isc_services_count() == 0) {
		fprintf(stderr, "ISC services are not started\n");
		usage();
	}

	m0_xc_isc_libdemo_init();

	rc = alloc_segs(&data, &ext, &attr, 4096, segs_nr);
	if (rc != 0) {
		fprintf(stderr, "failed to alloc_segs: rc=%d\n", rc);
		usage();
	}
	set_exts(&ext, 0, 4096);

	m0_obj_init(&obj, &uber_realm, &obj_id, M0_DEFAULT_LAYOUT_ID);
	rc = open_entity(&obj.ob_entity);
	if (rc != 0) {
		fprintf(stderr, "failed to open object: rc=%d\n", rc);
		usage();
	}

	rc = m0_obj_op(&obj, M0_OC_READ, &ext, &data, &attr, 0, 0, &op);
	if (rc != 0) {
		fprintf(stderr, "failed to create op: rc=%d\n", rc);
		usage();
	}

	plan = m0_layout_plan_build(op);
	if (plan == NULL) {
		fprintf(stderr, "failed to build access plan\n");
		usage();
	}

	out_args = NULL;
	inp_args = NULL;

	/* Initialise the  parameters for operation. */
	rc = op_init(op_type, &inp_args);
	while (rc == 0) {
		M0_ALLOC_PTR(req);
		if (req == NULL) {
			fprintf(stderr, "request allocation failed\n");
			break;
		}
		prev_plop = plop;
		rc = m0_layout_plan_get(plan, 0, &plop);
		if (rc != 0) {
			fprintf(stderr, "failed to get plop: rc=%d\n", rc);
			usage();
		}

		if (plop->pl_type == M0_LAT_DONE) {
			m0_layout_plop_done(plop);
			break;
		}
		if (plop->pl_type == M0_LAT_OUT_READ) {
			/* XXX just to be sure, for now only */
			M0_ASSERT(prev_plop != NULL &&
				  prev_plop->pl_type == M0_LAT_READ);
			m0_layout_plop_done(plop);
			continue;
		}

		M0_ASSERT(plop->pl_type == M0_LAT_READ); /* XXX for now */

		iopl = container_of(plop, struct m0_layout_io_plop, iop_base);

		printf("req=%d goff=%lu\n", reqs_nr, iopl->iop_goff);

		/* Prepare arguments for computation. */
		rc = input_prepare(&buf, &comp_fid, iopl, &reply_len, op_type);
		if (rc != 0) {
			m0_layout_plop_done(plop);
			fprintf(stderr, "input preparation failed: %d\n", rc);
			break;
		}
		rc = c0appz_isc_req_prepare(req, &buf, &comp_fid, iopl,
					    reply_len);
		if (rc != 0) {
			m0_buf_free(&buf);
			m0_layout_plop_done(plop);
			m0_free(req);
			fprintf(stderr, "request preparation failed: %d\n", rc);
			break;
		}

		rc = c0appz_isc_req_send(req);
		conn_addr = m0_rpc_conn_addr(iopl->iop_session->s_conn);
		if (rc != 0) {
			fprintf(stderr, "error from %s received: rc=%d\n",
				conn_addr, rc);
			break;
		}
		reqs_nr++;
	}

	/* wait for all replies */
	while (reqs_nr-- > 0)
		m0_semaphore_down(&isc_sem);

	/* process the replies */
	m0appz_isc_reqs_teardown(req) {
		if (rc == 0 && req->cir_rc == 0) {
			if (op_type == ICT_PING)
				out_args = (void*)conn_addr;
			out_args = output_process(&req->cir_result,
						  --segs_nr == 0 ? true : false,
						  out_args, op_type);
		}
		m0_layout_plop_done(req->cir_plop);
		c0appz_isc_req_fini(req);
		m0_free(req);
	}

	op_fini(op_type, inp_args, out_args);

	/* free resources*/
	c0appz_free();

	/* time out */
	c0appz_timeout(0);

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

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
 * http://www.xyratex.com/contact
 *
 * Original author:  Nachiket Sahasrabuddhe <nachiket.sahasrabuddhe@seagate.com>
 * Original creation date: 06-Sep-2018
 */

#include "conf/schema.h" /* M0_CST_ISCS */
#include "lib/memory.h"  /* m0_alloc m0_free */

#include "c0appz.h"
#include "c0appz_isc.h"  /* c0appz_isc_req */
#include "libdemo.h"     /* mm_result */


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

static int op_type_get(const char *op_name)
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


	fd = fopen("c0isc_data", "r");
	if (fd == NULL) {
		fprintf(stderr, "error! Could not open file c0isc_data\n");
		return -EINVAL;
	}
	fscanf(fd, "%d", (int *)arr_len);
	/* XXX: Fix sizeof (double) with appropriate macro. */
	val_arr = m0_alloc(arr_len[0] * sizeof (double));
	for (i = 0; i < arr_len[0]; ++i) {
		rc = fscanf(fd, "%lf", &val_arr[i]);
		if (rc == EOF) {
			fprintf(stderr, "File does not contain specified number"
					"of elements");
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

static int op_init(enum isc_comp_type type, void **ip_args)
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
		rc = file_to_array("c0isc_data", (void **)&arr, &arr_len);
		if (rc != 0)
			return rc;
		in_info->ma_arr = arr;
		in_info->ma_len = arr_len;
		in_info->ma_curr_svc_id = 0;
		in_info->ma_svc_nr = isc_services_count();
		*ip_args = in_info;

		return 0;
	default:
		fprintf(stderr, "Invalid operation %d\n", type);
		return EINVAL;
	}
}

static void op_fini(enum isc_comp_type op_type, void *ip_args, void *op_args)
{
	struct mm_args *in_info;

	switch (op_type) {
	case ICT_PING:
		break;
	case ICT_MIN:
	case ICT_MAX:
		if (ip_args == NULL)
			break;
		in_info = ip_args;
		m0_free(in_info->ma_arr);
		M0_SET0(in_info);
		m0_free(op_args);
	}
}

static bool is_last_service(struct mm_args *in_info)
{
	return in_info->ma_curr_svc_id + 1 == in_info->ma_svc_nr;
}

static uint32_t buf_len_calc(struct mm_args *in_info)
{
	uint32_t chunk_size;
	uint32_t buf_len;

	/* All services except one get chunk_size of an array. */
	chunk_size = in_info->ma_len / in_info->ma_svc_nr;

	/* Need to incorporate the remainder into last service. */
	buf_len = is_last_service(in_info) ?
		in_info->ma_len - in_info->ma_curr_svc_id *chunk_size :
		chunk_size;
	return buf_len;
}

static uint32_t offset_calc(struct mm_args *in_info)
{
	return in_info->ma_curr_svc_id * in_info->ma_len / in_info->ma_svc_nr;

}

static int minmax_input_prepare(struct m0_buf *buf, struct m0_fid *comp_fid,
				uint32_t *reply_len, enum isc_comp_type type,
				struct mm_args *in_info)
{
	struct m0_buf buf_local = M0_BUF_INIT0;
	uint32_t      buf_len;
	uint32_t      offset;
	int           rc;


	*buf = M0_BUF_INIT0;
	/** Calculate parameters relevant to array to be communicated. */
	buf_len = buf_len_calc(in_info);
	offset  = offset_calc(in_info);

	/** A local buffer pointing to appropriate offset in the array. */
	m0_buf_init(&buf_local, in_info->ma_arr + offset, buf_len *
		    sizeof in_info->ma_arr[0]);
	rc = m0_buf_copy_aligned(buf, &buf_local, M0_0VEC_SHIFT);
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
	if (greeting == NULL) {
		fprintf(stderr, "error! Out of memory for %d",
			ICT_PING);
		return -ENOMEM;
	}
	m0_buf_init(buf, greeting, strlen(greeting));
	fid_get("hello_world", comp_fid);
	*reply_len = CBL_DEFAULT_MAX;

	return 0;
}

static int input_prepare(struct m0_buf *buf, struct m0_fid *comp_fid,
			 uint32_t *reply_len, enum isc_comp_type type,
			 void *ip_args)
{

	switch (type) {
	case ICT_PING:
		return ping_input_prepare(buf, comp_fid, reply_len, type);
	case ICT_MIN:
	case ICT_MAX:
		return minmax_input_prepare(buf, comp_fid, reply_len, type,
					    ip_args);
	}
	return -EINVAL;
}

struct mm_result *op_result(struct mm_result *x, struct mm_result *y,
			    enum isc_comp_type op_type)
{
	switch (op_type) {
	case ICT_MIN:
		return x->mr_val <= y->mr_val ? x : y;
	case ICT_MAX:
		return x->mr_val >= y->mr_val ? x : y;
	default:
		return NULL;
	}
}

static void *minmax_output_prepare(struct m0_buf *result,
				   struct mm_args *in_info,
				   struct mm_result *prev,
				   enum isc_comp_type type)
{
	struct mm_result *new;

	if (prev == NULL) {
		M0_ALLOC_PTR(prev);
		memcpy(prev, result->b_addr, sizeof prev[0]);
		++in_info->ma_curr_svc_id;
		return prev;
	}
	new = result->b_addr;
	/** Service sent index from sub-array. */
	new->mr_idx = new->mr_idx + offset_calc(in_info);
	/* Copy the current optimal value. */
	*prev =  *op_result(prev, new, type);
	/* Print the output. */
	if (is_last_service(in_info))
		fprintf(stderr, "\nidx=%d\tval=%lf\n", prev->mr_idx,
			prev->mr_val);
	/** Bookkeep the count of services communicated so far. */
	++in_info->ma_curr_svc_id;
	return prev;
}

/**
 * Apart from processing the output this can deserialize the buffer into
 * a structure relevant to the result of invoked computation.
 * and return the same.
 */
static void* output_process(struct m0_buf *result, void *in_args,
			    void *op_args, enum isc_comp_type type)
{
	switch (type) {
	case ICT_PING:
		printf ("\nHello-%s@"FID_F "\n", (char *)result->b_addr,
			FID_P((struct m0_fid *)op_args));
		memset(result->b_addr, 'a', result->b_nob);
		return NULL;
	case ICT_MIN:
	case ICT_MAX:
		return minmax_output_prepare(result, in_args, op_args, type);
	}
	return NULL;
}

static void usage_print(char *bin_name)
{
	fprintf(stderr,"Usage:\n");
	fprintf(stderr,"%s op_name\n", bin_name);
	fprintf	(stderr,"supported operations: ping, min, max\n");
	fprintf(stderr, "See README");
}

/* main */
int main(int argc, char **argv)
{
	struct m0_buf          buf;
	struct m0_buf          result;
	struct m0_fid          comp_fid;
	struct c0appz_isc_req  req;
	/* Holds arguments specific to an operation. */
	void                  *ip_args;
	/* Holds output specific to a computation. */
	void                  *op_args;
	struct m0_fid          svc_fid;
	struct m0_fid          start_fid = M0_FID0;
	uint32_t               reply_len;
	int                    op_type;
	int                    rc;

	/* check input */
	if (argc != 2) {
		usage_print(basename(argv[0]));
		return -1;
	}

	/* time in */
	c0appz_timein();

	/* c0rcfile
	 * overwrite .cappzrc to a .[app]rc file.
	 */
	char str[256];
	sprintf(str,"%s/.%src", dirname(argv[0]), basename(argv[0]));
	c0appz_setrc(str);
	c0appz_putrc();

	op_type = op_type_get(argv[1]);
	if (op_type == -EINVAL) {
		usage_print(basename(argv[0]));
		return -EINVAL;
	}

	/* initialize resources */
	if (c0appz_init(0) != 0) {
		fprintf(stderr,"error! clovis initialization failed.\n");
		return -2;
	}

	if (isc_services_count() == 0) {
		fprintf(stderr, "\nISC services are not started\n");
		rc = -EINVAL;
		goto out;
	}

	op_args = NULL;
	ip_args = NULL;

	/* Initialise the  parameters for operation. */
	rc = op_init(op_type, &ip_args);
	while (rc == 0) {
		/* Prepare arguments for computation. */
		rc = input_prepare(&buf, &comp_fid, &reply_len,
				   op_type, ip_args);
		if (rc != 0) {
			fprintf(stderr, "\nerror! Input preparation failed: %d",
				rc);
			break;
		}
		rc = c0appz_isc_nxt_svc_get(&start_fid, &svc_fid,
					     M0_CST_ISCS);
		if (rc != 0) {
			m0_buf_free(&buf);
			break;
		}
		rc = c0appz_isc_req_prepare(&req, &buf, &comp_fid, &result,
					    &svc_fid, reply_len);
		if (rc != 0) {
			fprintf(stderr, "\nerror! request preparation failed:"
				"%d", rc);
			goto out;
		}
		/*
		 * XXX: To achieve parallelism across services need to change
		 * this to async.
		 */
		rc = c0appz_isc_req_send_sync(&req);
		if (rc == 0) {
			if (op_type == ICT_PING)
				op_args = &svc_fid;
			op_args = output_process(&result, ip_args, op_args,
						 op_type);
		} else
			fprintf(stderr, "Error %d received\n Check if "
				"the relevant library is loaded\n", rc);
		c0appz_isc_req_fini(&req);
		start_fid = svc_fid;
	}
	/* Ignore the case when all services are iterated. */
	if (rc == -ENOENT)
		rc = 0;
	op_fini(op_type, ip_args, op_args);
out:
	/* free resources*/
	c0appz_free();

	/* time out */
	c0appz_timeout(0);

	/* success */
	if (rc == 0)
		fprintf(stderr,"%s success\n",basename(argv[0]));
	else
		fprintf(stderr,"%s fail\n",basename(argv[0]));
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

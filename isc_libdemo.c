#include "lib/misc.h"       /* m0_full_name_hash */
#include "fid/fid.h"        /* m0_fid */
#include "lib/buf.h"        /* m0_buf */
#include "lib/string.h"     /* m0_strdup */
#include "iscservice/isc.h" /* m0_isc_comp_register */
#include "fop/fom.h"        /* M0_FSO_AGAIN */

#include "libdemo.h"

static void fid_get(const char *f_name, struct m0_fid *fid)
{
	uint32_t f_key = m0_full_name_hash((const unsigned char*)f_name,
					    strlen(f_name));
	uint32_t f_cont = m0_full_name_hash((const unsigned char *)"libdemo",
					    strlen("libdemo"));

	m0_fid_set(fid, f_cont, f_key);
}

static bool is_valid_string(struct m0_buf *in)
{
	return m0_buf_streq(in, "hello") || m0_buf_streq(in, "Hello") ||
		m0_buf_streq(in, "HELLO");
}

int hello_world(struct m0_buf *in, struct m0_buf *out,
	        struct m0_isc_comp_private *comp_data, int *rc)
{
	char *out_str;

	if (is_valid_string(in)) {
		/*
		 * Note: The out buffer allocated here is freed
		 * by iscservice, and a computation shall not free
		 * it in the end of computation.
		 */
		out_str = m0_strdup("world");
		if (out_str != NULL)
			m0_buf_init(out, (void *)out_str, strlen(out_str));
		else
			*rc = -ENOMEM;
		*rc = 0;
	} else
		*rc = -EINVAL;
	return M0_FSO_AGAIN;
}

int arr_min(struct m0_buf *in, struct m0_buf *out,
	    struct m0_isc_comp_private *comp_data, int *ret)
{
	uint32_t          arr_len;
	uint32_t          i;
	double           *arr;
	struct mm_result  curr_min;
	struct m0_buf     buf_local;
	int               rc;

	/* The default error code. */
	*ret = 0;

	if (in->b_nob == 0) {
		*out = M0_BUF_INIT0;
		*ret = -EINVAL;
		return M0_FSO_AGAIN;
	}
	arr_len = in->b_nob / sizeof arr[0];
	arr     = in->b_addr;
	curr_min.mr_idx = 0;
	curr_min.mr_val = arr[0];

	for (i = 1; i < arr_len; ++i) {
		if (arr[i] < curr_min.mr_val) {
			curr_min.mr_idx = i;
			curr_min.mr_val = arr[i];
		}
	}

	m0_buf_init(&buf_local, &curr_min, sizeof curr_min);
	rc = m0_buf_copy_aligned(out, &buf_local, M0_0VEC_SHIFT);
	if (rc != 0)
		*ret = rc;
	return M0_FSO_AGAIN;
}

int arr_max(struct m0_buf *in, struct m0_buf *out,
	    struct m0_isc_comp_private *comp_data, int *ret)
{
	uint32_t          arr_len;
	uint32_t          i;
	double           *arr;
	struct mm_result  curr_max;
	struct m0_buf     buf_local;
	int rc;

	/* The default error code. */
	*ret = 0;

	if (in->b_nob == 0) {
		*out = M0_BUF_INIT0;
		*ret = -EINVAL;
		return M0_FSO_AGAIN;
	}
	arr_len = in->b_nob / sizeof arr[0];
	arr     = in->b_addr;
	curr_max.mr_idx = 0;
	curr_max.mr_val = arr[0];

	for (i = 1; i < arr_len; ++i) {
		if (arr[i] > curr_max.mr_val) {
			curr_max.mr_idx = i;
			curr_max.mr_val = arr[i];
		}
	}

	m0_buf_init(&buf_local, &curr_max, sizeof curr_max);
	rc = m0_buf_copy_aligned(out, &buf_local, M0_0VEC_SHIFT);
	if (rc != 0)
		*ret = rc;
	return M0_FSO_AGAIN;
}

static void comp_reg(const char *f_name, int (*ftn)(struct m0_buf *arg_in,
						    struct m0_buf *args_out,
					            struct m0_isc_comp_private
					            *comp_data, int *rc))
{
	struct m0_fid comp_fid;
	int           rc;

	fid_get(f_name, &comp_fid);
	rc = m0_isc_comp_register(ftn, f_name, &comp_fid);
	if (rc == -EEXIST)
		fprintf(stderr, "Computation already exists");
	else if (rc == -ENOMEM)
		fprintf(stderr, "Out of memory");
}

void mero_lib_init(void)
{
	comp_reg("hello_world", hello_world);
	comp_reg("arr_min", arr_min);
	comp_reg("arr_max", arr_max);
}

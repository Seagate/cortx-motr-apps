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

#include "lib/misc.h"       /* m0_full_name_hash */
#include "fid/fid.h"        /* m0_fid */
#include "lib/buf.h"        /* m0_buf */
#include "lib/string.h"     /* m0_strdup */
#include "iscservice/isc.h" /* m0_isc_comp_register */
#include "fop/fom.h"        /* M0_FSO_AGAIN */

#include "isc/libdemo.h"
#include "isc/libdemo_xc.h"

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

enum op {MIN, MAX};

int arr_minmax(enum op op, struct m0_buf *in, struct m0_buf *out,
	       struct m0_isc_comp_private *comp_data, int *rc)
{
	uint32_t          arr_len;
	uint32_t          i;
	double           *arr;
	struct isc_args   a;
	struct mm_result  curr_min;

	*rc = m0_xcode_obj_dec_from_buf(&M0_XCODE_OBJ(isc_args_xc, &a),
				       in->b_addr, in->b_nob);
	if (*rc != 0)
		return M0_FSO_AGAIN;

	arr_len = a.ia_len;
	arr     = a.ia_arr;
	curr_min.mr_idx = 0;
	curr_min.mr_val = arr[0];

	for (i = 1; i < arr_len; ++i) {
		if (op == MIN ? arr[i] < curr_min.mr_val :
		                arr[i] > curr_min.mr_val) {
			curr_min.mr_idx = i;
			curr_min.mr_val = arr[i];
		}
	}

	*rc = m0_buf_new_aligned(out, &curr_min, sizeof curr_min,
				 M0_0VEC_SHIFT);

	return M0_FSO_AGAIN;
}

int arr_min(struct m0_buf *in, struct m0_buf *out,
	    struct m0_isc_comp_private *comp_data, int *rc)
{
	return arr_minmax(MIN, in, out, comp_data, rc);
}

int arr_max(struct m0_buf *in, struct m0_buf *out,
	    struct m0_isc_comp_private *comp_data, int *rc)
{
	return arr_minmax(MAX, in, out, comp_data, rc);
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

void motr_lib_init(void)
{
	comp_reg("hello_world", hello_world);
	comp_reg("arr_min", arr_min);
	comp_reg("arr_max", arr_max);
	m0_xc_isc_libdemo_init();
}

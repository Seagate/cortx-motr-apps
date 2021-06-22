/* -*- C -*- */
/*
 * Copyright (c) 2017-2020 Seagate Technology LLC and/or its Affiliates
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

#include "lib/types.h"
#include "fid/fid.h"
#include "fid/fid_xc.h"
#include "lib/vec.h"
#include "lib/vec_xc.h"
#include "xcode/xcode_attr.h"

struct isc_buf {
	uint32_t  i_len;
	char     *i_buf;
} M0_XCA_SEQUENCE;

/**
 * Holds the result of min and max computations.
 *
 * The left and right cuts of the values which crossed
 * the unit boundaries should be glued by the client code
 * to restore the missing values in order to include them
 * in the final computation also.
 */
struct mm_result {
	uint64_t       mr_idx;
	uint64_t       mr_idx_max;
	double         mr_val;
	/** right cut of the value on the left side of unit */
	struct isc_buf mr_lbuf;
	/** left  cut of the value on the right side of unit */
	struct isc_buf mr_rbuf;
} M0_XCA_RECORD;

struct isc_arr {
	uint32_t ia_len;
	double  *ia_arr;
} M0_XCA_SEQUENCE;

/** Arguments to the target ISC service. */
struct isc_targs {
	struct m0_fid         ist_cob;
	struct m0_io_indexvec ist_ioiv;
	struct isc_arr        ist_arr;
} M0_XCA_RECORD;

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */

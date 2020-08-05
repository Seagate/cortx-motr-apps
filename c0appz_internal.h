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
 * Original author:  Andriy Tkachuk <andriy.tkachuk@seagate.com>
 * Original creation date: 29-Jul-2020
 */

#ifndef __C0APPZ_INTERNAL_H__
#define __C0APPZ_INTERNAL_H__

#include <stdint.h>
#include "clovis/clovis.h"

extern unsigned unit_size; /* in KiBs, default 4 */
extern struct m0_clovis_realm clovis_uber_realm;

struct clovis_aio_op;
struct clovis_aio_opgrp {
	struct m0_semaphore   cag_sem;
	struct m0_mutex       cag_mlock;
	uint32_t              cag_op_cnt;
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


int write_data_to_object(struct m0_clovis_obj *o, struct m0_indexvec *ext,
			 struct m0_bufvec *data, struct m0_bufvec *attr);
int read_data_from_object(struct m0_clovis_obj *o, struct m0_indexvec *ext,
			  struct m0_bufvec *data,struct m0_bufvec *attr);

int ppf(const char *fmt, ...);

int write_data_to_object_async(struct clovis_aio_op *aio);

int clovis_aio_opgrp_init(struct clovis_aio_opgrp *grp, uint64_t bsz,
			  uint32_t cnt_per_op, uint32_t op_cnt);
void clovis_aio_opgrp_fini(struct clovis_aio_opgrp *grp);
void clovis_aio_op_fini_free(struct clovis_aio_op *aio);

int alloc_segs(struct m0_bufvec *data, struct m0_indexvec *ext,
	       struct m0_bufvec *attr, uint64_t bsz, uint32_t cnt);
void free_segs(struct m0_bufvec *data, struct m0_indexvec *ext,
	       struct m0_bufvec *attr);
uint64_t set_exts(struct m0_indexvec *ext, uint64_t off, uint64_t bsz);


#endif /* __C0APPZ_INTERNAL_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */

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
 * Original creation date: 29-May-2020
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "clovis/clovis.h"
#include "clovis/clovis_idx.h"

struct c0pool {
	struct m0_fid pool_fid;
	uint64_t pool_bsz;
};

static struct c0pool c0p[10];

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */
struct m0_fid *pool_fid = NULL;
uint64_t pool_bsz = 0;

/*
 ******************************************************************************
 * EXTERN FUNCTIONS
 ******************************************************************************
 */

/* c0appz_pool_set() */
int c0appz_pool_set(int pid)
{
	assert(pid>=0);
	assert(pid<=2);
	pool_bsz = c0p[pid].pool_bsz;
	pool_fid = &c0p[pid].pool_fid;
	return 0;
}

/* c0appz_pool_ini() */
int c0appz_pool_ini(void)
{

	/*
	 * use a Tier 1 NVM pool
	 */

	/* pool 0 */
	c0p[0].pool_fid.f_container = 0x6f00000000000001;
	c0p[0].pool_fid.f_key = 0x6bf;
	c0p[0].pool_bsz = 4096*2;

	/*
	 * use a Tier 2 SSD pool
	 */

	/* pool 1 */
	c0p[1].pool_fid.f_container = 0x6f00000000000001;
	c0p[1].pool_fid.f_key = 0x6d5;
	c0p[1].pool_bsz = 4096*8;

	/*
	 * use a Tier 3 HDD pool
	 */

	/* pool 2 */
	c0p[2].pool_fid.f_container = 0x6f00000000000001;
	c0p[2].pool_fid.f_key = 0x6f4;
	c0p[2].pool_bsz = 4096*8;


    return 0;
}


/*
 ******************************************************************************
 * STATIC FUNCTIONS
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

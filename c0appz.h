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
 */

#include <stdint.h>

int c0appz_init(int idx);
int c0appz_free(void);
int c0appz_rm(int64_t idhi, int64_t idlo);
int c0appz_cat(int64_t idhi, int64_t idlo, int bsz, int cnt, char *filename);
int c0appz_cp(int64_t idhi, int64_t idlo, char *filename, int bsz, int cnt);
int c0appz_cp_async(int64_t idhi, int64_t idlo, char *filename,
		    int bsz, int cnt, int op_cnt);
int c0appz_setrc(char *rcfile);
int c0appz_putrc(void);
int c0appz_timeout(uint64_t sz);
int c0appz_timein();

/*
 ** ECMWF REQUIREMENTS
 ** synchronous/asynchronous get/put etc.
 ** Note to Nikita: c0appz uses int64_t idhi, int64_t idlo to refer Mero object
 ** ids due to popular demand from other Sage developers. Please use the same
 ** convention below. ECMWF has been informed about it.
 */

#if 0
/* generate a unique object id */
int c0appz_generate_id(struct m0_uint128* id);
/* synchronous put a full object */
int c0appz_put(char* buffer, size_t size, struct m0_uint128* id);
/* synchronous get a full object */
int c0appz_get(char* buffer, size_t* size, struct m0_uint128* id);
/* asynchronous put a full object, allocates op */
struct m0_clovis_op* c0appz_async_put(char* buffer, size_t size, struct m0_uint128* id);
/* asynchronous get a full object, allocates op */
struct m0_clovis_op* c0appz_async_get(char* buffer, size_t size, struct m0_uint128* id);
/* wait for an operation to complete and check status */
int c0appz_wait(struct m0_clovis_op* op);
/* free ops */
void c0appz_free_op(struct m0_clovis_op*);
/* flush list of objects */
int c0appz_flush(struct m0_uint128* id, size_t size);
#endif


int c0appz_generate_id(int64_t *idh, int64_t *idl);



/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */

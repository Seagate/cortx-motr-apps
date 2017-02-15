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
 * Original author:  Juan   Gonzalez <juan.gonzalez@seagate.com>
 *                   James  Morse    <james.s.morse@seagate.com>
 *                   Sining Wu       <sining.wu@seagate.com>
 * Original creation date: 30-Oct-2014
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <assert.h>

#include "clovis/clovis.h"
#include "clovis/clovis_idx.h"

/* Clovis parameters */
static char *clovis_local_addr;
static char *clovis_ha_addr;
static char *clovis_confd_addr;
static char *clovis_prof;
static char *clovis_proc_fid;
static char *clovis_id;
static char *clovis_block_size;
static char *clovis_block_count;
static char *clovis_index_dir = "/tmp/";

static struct m0_clovis          *clovis_instance = NULL;
static struct m0_clovis_container clovis_container;
static struct m0_clovis_realm     clovis_uber_realm;
static struct m0_clovis_config    clovis_conf;

extern struct m0_addb_ctx m0_clovis_addb_ctx;

static int init_clovis(void)
{
	int rc;

	clovis_conf.cc_is_oostore            = false;
	clovis_conf.cc_is_read_verify        = false;
	clovis_conf.cc_local_addr            = clovis_local_addr;
	clovis_conf.cc_ha_addr               = clovis_ha_addr;
	clovis_conf.cc_confd                 = clovis_confd_addr;
	clovis_conf.cc_profile               = clovis_prof;
	clovis_conf.cc_process_fid           = clovis_proc_fid;
	clovis_conf.cc_tm_recv_queue_min_len = 16;
	clovis_conf.cc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	clovis_conf.cc_idx_service_id        = M0_CLOVIS_IDX_MOCK;
	clovis_conf.cc_idx_service_conf      = clovis_index_dir;
	clovis_conf.cc_layout_id 	     = 0;

	/* Clovis instance */
	rc = m0_clovis_init(&clovis_instance, &clovis_conf, true);
	if (rc != 0) {
		printf("Failed to initilise Clovis\n");
		goto err_exit;
	}

	/* And finally, clovis root realm */
	m0_clovis_container_init(&clovis_container, 
				 NULL, &M0_CLOVIS_UBER_REALM,
				 clovis_instance);
	rc = clovis_container.co_realm.re_entity.en_sm.sm_rc;

	if (rc != 0) {
		printf("Failed to open uber realm\n");
		goto err_exit;
	}
	
	clovis_uber_realm = clovis_container.co_realm;
	return 0;

err_exit:
	return rc;
}

static void fini_clovis(void)
{
	m0_clovis_fini(&clovis_instance, true);
}

static int cat()
{
	int                     i;
	int                     j;
	int                     rc;
	struct m0_uint128       id;
	struct m0_clovis_op    *ops[1] = {NULL};
	struct m0_clovis_obj    obj;
	uint64_t                last_index;
	struct m0_indexvec      ext;
	struct m0_bufvec        data;
	struct m0_bufvec        attr;

	/* Initialise ids */
	id = M0_CLOVIS_ID_APP;
	id.u_lo = atoi(clovis_id);

	/* we want to read <clovis_block_count> from the beginning of the object */
	rc = m0_indexvec_alloc(&ext, atoi(clovis_block_count));
	if (rc != 0)
		return rc;

	/*
	 * this allocates <clovis_block_count> * 4K buffers for data, and initialises
	 * the bufvec for us.
	 */
	rc = m0_bufvec_alloc(&data, atoi(clovis_block_count), atoi(clovis_block_size));
	if (rc != 0)
		return rc;
	rc = m0_bufvec_alloc(&attr, atoi(clovis_block_count), 1);
	if(rc != 0)
		return rc;

	last_index = 0;
	for (i = 0; i < atoi(clovis_block_count); i++) {
		ext.iv_index[i] = last_index ;
		ext.iv_vec.v_count[i] = atoi(clovis_block_size);
		last_index += atoi(clovis_block_size);
		
		/* we don't want any attributes */
		attr.ov_vec.v_count[i] = 0;

	}

	/* Read the requisite number of blocks from the entity */
	m0_clovis_obj_init(&obj, &clovis_uber_realm, &id);
	/* Create the read request */
	m0_clovis_obj_op(&obj, M0_CLOVIS_OC_READ, &ext, &data, &attr, 0, &ops[0]);
	assert(rc == 0);
	assert(ops[0] != NULL);
	assert(ops[0]->op_sm.sm_rc == 0);

	m0_clovis_op_launch(ops, 1);

	/* wait */
	rc = m0_clovis_op_wait(ops[0],
			    M0_BITS(M0_CLOVIS_OS_FAILED,
				    M0_CLOVIS_OS_STABLE),
		     M0_TIME_NEVER);
	assert(rc == 0);
	assert(ops[0]->op_sm.sm_state == M0_CLOVIS_OS_STABLE);
	assert(ops[0]->op_sm.sm_rc == 0);

	/* putchar the output */
	for (i = 0; i < atoi(clovis_block_count); i++) {
		for(j = 0; j < data.ov_vec.v_count[i]; j++) {
			putchar(((char *)data.ov_buf[i])[j]);
		}
	}

	/* fini and release */
	m0_clovis_op_fini(ops[0]);
	m0_clovis_op_free(ops[0]);
	m0_clovis_entity_fini(&obj.ob_entity);

	m0_indexvec_free(&ext);
	m0_bufvec_free(&data);
	m0_bufvec_free(&attr);
	
	return 0;
}

int main(int argc, char **argv)
{
	int rc;

	/* Get input parameters */
	if (argc < 10) {
		printf("Usage: c0cat laddr ha_addr confd_addr prof_opt proc_fid "
		       "index_dir object_id block_size block_count\n\n");
		printf("NAME:c0cat\n\n"
		       "Read Mero object and print contents on screen\n\n"
		       "DESCRIPTION:\n"
                       "laddr      : Local endpoint for Clovis application\n"
			             "\t     Format:- NID_NODE:12345:44:101\n"
			             "\t     Find NID_NODE using: 'sudo lctl list_nids'\n"
                       "ha_addr    : HA service endpoint\n"
				     "\t     Format:- NID_NODE:12345:45:1\n"
				     "\t     Find HA service endpoint using `systemctl status mero-server-ha`\n"
                       "confd_addr : configuration service endpoint\n"
				     "\t     Format:- NID_NODE:12345:44:101\n"
				     "\t     Find configuration service endpoint using `systemctl status mero-server-confd`\n"
		       "prof_opt   : It's a profile id assigned to Clovis application.\n"
				     "\t     It should always be '<0x7000000000000001:0>'\n"
		       "proc_fid   : It's a process fid used by Clovis application\n"
				     "\t     It should awlays be '<0x7200000000000000:0>'\n"
		       "index_dir  : It's an index directory used by clovis mock index implementation\n"
				     "\t     It can be any existing directory. (preferably /tmp)\n"
		       "object_id  : Identifier for a Mero object.\n"
				     "\t     It has to be a number > 1048576\n"
		       "block_size : Unit of a object in which it is downloaded from Mero(preferably 4096)\n"
		       "block_count: Number of units each of 'block_size'\n");

		return -1;	
	}

	clovis_local_addr = argv[1];;
	clovis_ha_addr = argv[2];
	clovis_confd_addr = argv[3];
	clovis_prof = argv[4];
	clovis_proc_fid = argv[5];
	clovis_index_dir = argv[6];
	clovis_id = argv[7];
	clovis_block_size = argv[8];
	clovis_block_count = argv[9];

	/* Initilise mero and Clovis */
	rc = init_clovis();
	if (rc < 0) {
		printf("clovis_init failed!\n");
		return rc;
	}

	/* Read from the object */
	cat();

	/* Clean-up */
	fini_clovis();

	return 0;
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

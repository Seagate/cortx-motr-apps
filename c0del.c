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

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <assert.h>

#include "clovis/clovis.h"
#include "clovis/clovis_idx.h"

/* Clovis parameters */
static char *clovis_local_addr;
static char *clovis_ha_addr;
static char *clovis_prof;
static char *clovis_proc_fid;
static char *clovis_id;
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
	m0_clovis_fini(clovis_instance, true);
}

int open_entity(struct m0_clovis_entity *entity)
{
        int                                 rc;
        struct m0_clovis_op *ops[1] = {NULL};

        m0_clovis_entity_open(entity, &ops[0]);
        m0_clovis_op_launch(ops, 1);
        m0_clovis_op_wait(ops[0], M0_BITS(M0_CLOVIS_OS_FAILED,
                                          M0_CLOVIS_OS_STABLE),
                                          M0_TIME_NEVER);
        rc = m0_clovis_rc(ops[0]);
        m0_clovis_op_fini(ops[0]);
        m0_clovis_op_free(ops[0]);
        ops[0] = NULL;

        return rc;
}


static int delete_object(struct m0_uint128 id)
{
	int                  rc;
	struct m0_clovis_obj obj;
	struct m0_clovis_op *ops[1] = {NULL};

	memset(&obj, 0, sizeof(struct m0_clovis_obj));
	m0_clovis_obj_init(&obj, &clovis_uber_realm, &id,
			   m0_clovis_default_layout_id(clovis_instance));
	rc = open_entity(&obj.ob_entity);
	if (rc < 0) return rc;

	m0_clovis_entity_delete(&obj.ob_entity, &ops[0]);
	m0_clovis_op_launch(ops, ARRAY_SIZE(ops));
	rc = m0_clovis_op_wait(
		ops[0], M0_BITS(M0_CLOVIS_OS_FAILED, M0_CLOVIS_OS_STABLE),
		m0_time_from_now(3,0));

	m0_clovis_op_fini(ops[0]);
	m0_clovis_op_free(ops[0]);
	m0_clovis_entity_fini(&obj.ob_entity);

	return rc;
}


int main(int argc, char **argv)
{
	int rc;

	/* Get input parameters */
	if (argc < 7) {
		printf(
				"Usage: c0del laddr ha_addr prof_opt proc_fid "
				"index_dir object_id\n\n"
				);
		printf(
				"NAME:c0del\n\n"
				"Read Mero object and print contents on screen\n\n"
				"DESCRIPTION:\n"
				"laddr      : Local endpoint for Clovis application\n"
			    "\t     Format:- NID_NODE:12345:44:101\n"
				"\t     Find NID_NODE using: 'sudo lctl list_nids'\n"
              	"ha_addr    : HA service endpoint\n"
				"\t     Format:- NID_NODE:12345:45:1\n"
				"\t     Find HA service endpoint using `systemctl status mero-server-ha`\n"
				"prof_opt   : It's a profile id assigned to Clovis application.\n"
				"\t     It should always be '<0x7000000000000001:0>'\n"
				"proc_fid   : It's a process fid used by Clovis application\n"
				"\t     It should awlays be '<0x7200000000000000:0>'\n"
				"index_dir  : It's an index directory used by clovis mock index implementation\n"
				"\t     It can be any existing directory. (preferably /tmp)\n"
				"object_id  : Identifier for a Mero object.\n"
				"\t     It has to be a number > 1048576\n"
				);

		return -1;	
	}

	clovis_local_addr = argv[1];;
	clovis_ha_addr = argv[2];
	clovis_prof = argv[3];
	clovis_proc_fid = argv[4];
	clovis_index_dir = argv[5];
	clovis_id = argv[6];

	/* Initilise mero and Clovis */
	rc = init_clovis();
	if (rc < 0) {
		printf("clovis_init failed!\n");
		return rc;
	}

	/* delete object */
	struct m0_uint128 id;
	id = M0_CLOVIS_ID_APP;
	id.u_lo = atoi(clovis_id);
	rc = delete_object(id);
	if (rc != 0)
	{
		fprintf(stderr, "Can't create object!\n");
	}

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

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

#ifndef DEBUG
#define DEBUG 0
#endif

#define MAXCHAR 256
#define C2RCFLE "./.c2rc"
char c2rc[8][MAXCHAR];

/* static variables */
static struct m0_clovis          *clovis_instance = NULL;
static struct m0_clovis_container clovis_container;
static struct m0_clovis_realm     clovis_uber_realm;
static struct m0_clovis_config    clovis_conf;


/*
 * open_entity()
 * open clovis entity.
 */
static int open_entity(struct m0_clovis_entity *entity)
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


/*
 * objdel()
 * delete object.
 */
int objdel(int64_t idhi, int64_t idlo)
{
	int rc;
	struct m0_clovis_obj obj;
	struct m0_clovis_op *ops[1] = {NULL};
	struct m0_uint128 id;

	id.u_hi = idhi;
	id.u_lo = idlo;

	memset(&obj, 0, sizeof(struct m0_clovis_obj));
	m0_clovis_obj_init(&obj, &clovis_uber_realm, &id,
			   m0_clovis_default_layout_id(clovis_instance));
	rc = open_entity(&obj.ob_entity);
	if (rc < 0) {
		printf("error! [%d]\n", rc);
		printf("object not found\n");
		return rc;
	}

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



/*
 * c2init()
 * init clovis resources.
 */
int c2init(void)
{
	int rc;
    FILE *fp;
    char str[MAXCHAR];
    char* filename = C2RCFLE;
    int i;

	/* read c2rc file */
    fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("error! could not open resource file %s\n",filename);
        return 1;
    }

    i = 0;
    while (fgets(str, MAXCHAR, fp) != NULL) {
    	str[strlen(str) - 1] = '\0';
    	memset(c2rc[i], 0x00, MAXCHAR);
    	strncpy(c2rc[i], str, MAXCHAR);

#if DEBUG
    	printf("%s", str);
    	printf("%s", c2rc[i]);
    	printf("\n");
#endif

    	i++;
    	if(i==8) break;
    }
    fclose(fp);

	clovis_conf.cc_is_oostore            = false;
	clovis_conf.cc_is_read_verify        = false;
	clovis_conf.cc_local_addr            = c2rc[0];	/* clovis_local_addr	*/
	clovis_conf.cc_ha_addr               = c2rc[1];	/* clovis_ha_addr		*/
	clovis_conf.cc_profile               = c2rc[2];	/* clovis_prof			*/
	clovis_conf.cc_process_fid           = c2rc[3];	/* clovis_proc_fid		*/
	clovis_conf.cc_idx_service_conf      = c2rc[4]; /* clovis_index_dir		*/
	clovis_conf.cc_tm_recv_queue_min_len = 16;
	clovis_conf.cc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	clovis_conf.cc_idx_service_id        = M0_CLOVIS_IDX_MOCK;
	clovis_conf.cc_layout_id 	     	 = 0;

#if DEBUG
	printf("---\n");
    printf("%s", (char *)clovis_conf.cc_local_addr);
    printf("%s", (char *)clovis_conf.cc_ha_addr);
    printf("%s", (char *)clovis_conf.cc_profile);
    printf("%s", (char *)clovis_conf.cc_process_fid);
    printf("%s", (char *)clovis_conf.cc_idx_service_conf);
	printf("---\n");
#endif

	/* clovis instance */
	rc = m0_clovis_init(&clovis_instance, &clovis_conf, true);
	if (rc != 0) {
		printf("Failed to initilise Clovis\n");
		return rc;
	}

	/* And finally, clovis root realm */
	m0_clovis_container_init(&clovis_container, 
				 NULL, &M0_CLOVIS_UBER_REALM,
				 clovis_instance);
	rc = clovis_container.co_realm.re_entity.en_sm.sm_rc;
	if (rc != 0) {
		printf("Failed to open uber realm\n");
		return rc;
	}

	/* success */
	clovis_uber_realm = clovis_container.co_realm;
	return 0;
}

/*
 * c2free()
 * free clovis resources.
 */
void c2free(void)
{
	m0_clovis_fini(clovis_instance, true);
	return;
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

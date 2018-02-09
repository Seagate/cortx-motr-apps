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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <assert.h>
#include <time.h>

#include "clovis/clovis.h"
#include "clovis/clovis_idx.h"

#ifndef DEBUG
#define DEBUG 0
#endif

#define SZC0RCSTR  256
#define SZC0RCFILE 256
#define C0RCFLE "./.cappzrc"
#define CLOVIS_MAX_BLOCK_COUNT (200)

/* static variables */
static struct m0_clovis          *clovis_instance = NULL;
static struct m0_clovis_container clovis_container;
static struct m0_clovis_realm     clovis_uber_realm;
static struct m0_clovis_config    clovis_conf;
static struct m0_idx_dix_config   dix_conf;

static char c0rc[8][SZC0RCSTR];
static char c0rcfile[SZC0RCFILE] = C0RCFLE;
static clock_t cpu_t = 0;

/*
 *******************************************************************************
 * STATIC FUNCTION PROTOTYPES
 *******************************************************************************
 */
static char *trim(char *str);
static int open_entity(struct m0_clovis_entity *entity);
static int create_object(struct m0_uint128 id);
static int read_data_from_file(FILE *fp, struct m0_bufvec *data, int bsz);
static int write_data_to_object(struct m0_uint128 id, struct m0_indexvec *ext,
				struct m0_bufvec *data, struct m0_bufvec *attr);

/*
 *******************************************************************************
 * EXTERN FUNCTIONS
 *******************************************************************************
 */

/*
 * c0appz_timeout()
 * time out execution.
 */
int c0appz_timeout(int sz)
{
	double t = (double)(clock() - cpu_t) / CLOCKS_PER_SEC;
	double b = (double)(sz) / (t * 1000000);
	fprintf(stderr,"[cpu: %0.2lf s %0.2lf Mbs]\n", t, b);
	return 0;
}

/*
 * c0appz_timein()
 * time in execution.
 */
int c0appz_timein()
{
	cpu_t = clock();
	return 0;
}

/*
 * c0appz_cp()
 * copy to an object.
 */
int c0appz_cp(int64_t idhi, int64_t idlo, char *filename, int bsz, int cnt)
{
	int                i;
	int                rc;
	int                block_count;
	uint64_t           last_index;
	struct m0_uint128  id;
	struct m0_indexvec ext;
	struct m0_bufvec   data;
	struct m0_bufvec   attr;
	FILE              *fp;

	/* open src file */
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        fprintf(stderr,"error! could not open input file %s\n",filename);
        return 1;
    }

	/* ids */
	id.u_hi = idhi;
	id.u_lo = idlo;

    /* create object */
	rc = create_object(id);
	if (rc != 0) {
		fprintf(stderr, "can't create object!\n");
		fclose(fp);
		return rc;
	}

	last_index = 0;
	while (cnt > 0) {
		block_count = (cnt > CLOVIS_MAX_BLOCK_COUNT)?
			      CLOVIS_MAX_BLOCK_COUNT:cnt;

		/* Allocate block_count * 4K data buffer. */
		rc = m0_bufvec_alloc(&data, block_count, bsz);
		if (rc != 0)
			return rc;

		/* Allocate bufvec and indexvec for write. */
		rc = m0_bufvec_alloc(&attr, block_count, 1);
		if(rc != 0)
			return rc;

		rc = m0_indexvec_alloc(&ext, block_count);
		if (rc != 0)
			return rc;

		/*
		 * Prepare indexvec for write: <clovis_block_count> from the
		 * beginning of the object.
		 */
		for (i = 0; i < block_count; i++) {
			ext.iv_index[i] = last_index ;
			ext.iv_vec.v_count[i] = bsz;
			last_index += bsz;

			/* we don't want any attributes */
			attr.ov_vec.v_count[i] = 0;
		}

		/* Read data from source file. */
		rc = read_data_from_file(fp, &data, bsz);
		assert(rc == block_count);

		/* Copy data to the object*/
		rc = write_data_to_object(id, &ext, &data, &attr);
		if (rc != 0) {
			fprintf(stderr, "writing to object failed!\n");
			return rc;
		}

		/* Free bufvec's and indexvec's */
		m0_indexvec_free(&ext);
		m0_bufvec_free(&data);
		m0_bufvec_free(&attr);

		cnt -= block_count;
	}

    fclose(fp);
    return 0;
}

/*
 * c0appz_cat()
 * cat object.
 */
int c0appz_cat(int64_t idhi, int64_t idlo, int bsz, int cnt)
{
	int                  i;
	int                  j;
	int                  rc;
	struct m0_uint128    id;
	struct m0_clovis_op *ops[1] = {NULL};
	struct m0_clovis_obj obj;
	uint64_t             last_index;
	struct m0_indexvec   ext;
	struct m0_bufvec     data;
	struct m0_bufvec     attr;

	/* ids */
	id.u_hi = idhi;
	id.u_lo = idlo;

	/* we want to read <clovis_block_count> from the beginning of the object */
	rc = m0_indexvec_alloc(&ext, cnt);
	if (rc != 0)
		return rc;

	/*
	 * this allocates <clovis_block_count> * 4K buffers for data, and initialises
	 * the bufvec for us.
	 */
	rc = m0_bufvec_alloc(&data, cnt, bsz);
	if (rc != 0)
		return rc;
	rc = m0_bufvec_alloc(&attr, cnt, 1);
	if(rc != 0)
		return rc;

	last_index = 0;
	for (i = 0; i < cnt; i++) {
		ext.iv_index[i] = last_index ;
		ext.iv_vec.v_count[i] = bsz;
		last_index += bsz;

		/* we don't want any attributes */
		attr.ov_vec.v_count[i] = 0;

	}

	M0_SET0(&obj);
	/* Read the requisite number of blocks from the entity */
	m0_clovis_obj_init(&obj, &clovis_uber_realm, &id,
			   m0_clovis_default_layout_id(clovis_instance));

	/* open entity */
	rc = open_entity(&obj.ob_entity);
	if (rc < 0) {
		fprintf(stderr,"error! [%d]\n", rc);
		fprintf(stderr,"object not found\n");
		return rc;
	}


	/* Create the read request */
	m0_clovis_obj_op(&obj, M0_CLOVIS_OC_READ, &ext, &data, &attr, 0, &ops[0]);
	assert(rc == 0);
	assert(ops[0] != NULL);
	assert(ops[0]->op_sm.sm_rc == 0);

	m0_clovis_op_launch(ops, 1);

	/* wait */
	rc = m0_clovis_op_wait(
				ops[0],
			    M0_BITS(M0_CLOVIS_OS_FAILED,
				M0_CLOVIS_OS_STABLE),
				M0_TIME_NEVER
				);
	assert(rc == 0);
	assert(ops[0]->op_sm.sm_state == M0_CLOVIS_OS_STABLE);
	assert(ops[0]->op_sm.sm_rc == 0);

	/* putchar the output */
	for (i = 0; i < cnt; i++) {
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


/*
 * c0appz_rm()
 * delete object.
 */
int c0appz_rm(int64_t idhi, int64_t idlo)
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
		fprintf(stderr,"error! [%d]\n", rc);
		fprintf(stderr,"object not found\n");
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
 * c0appz_init()
 * init clovis resources.
 */
int c0appz_init(void)
{
    int   i;
	int   rc;
    FILE *fp;
    char *str=NULL;
    char  buf[SZC0RCSTR];
    char* filename = C0RCFLE;

	/* read c0rc file */
    filename = c0rcfile;
    fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr,"error! could not open resource file %s\n",filename);
        return 1;
    }

    i = 0;
    while (fgets(buf, SZC0RCSTR, fp) != NULL) {

    	#if DEBUG
    	fprintf(stderr,"rd:->%s<-", buf);
    	fprintf(stderr,"\n");
		#endif

    	str = trim(buf);
    	if(str[0] == '#') continue;		/* ignore comments 		*/
    	if(strlen(str) == 0) continue;	/* ignore empty space 	*/

    	memset(c0rc[i], 0x00, SZC0RCSTR);
    	strncpy(c0rc[i], str, SZC0RCSTR);

		#if DEBUG
    	fprintf(stderr,"wr:->%s<-", c0rc[i]);
    	fprintf(stderr,"\n");
		#endif

    	i++;
    	if(i==8) break;
    }
    fclose(fp);

    clovis_conf.cc_is_oostore            = true;
    clovis_conf.cc_is_read_verify        = false;
    clovis_conf.cc_local_addr            = c0rc[0];
    clovis_conf.cc_ha_addr               = c0rc[1];
    clovis_conf.cc_profile               = c0rc[2];
    clovis_conf.cc_process_fid           = c0rc[3];
    clovis_conf.cc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
    clovis_conf.cc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;

    /* IDX_MERO */
    clovis_conf.cc_idx_service_id   = M0_CLOVIS_IDX_DIX;
    dix_conf.kc_create_meta = false;
    clovis_conf.cc_idx_service_conf = &dix_conf;

	#if DEBUG
	fprintf(stderr,"\n---\n");
    fprintf(stderr,"%s,", (char *)clovis_conf.cc_local_addr);
    fprintf(stderr,"%s,", (char *)clovis_conf.cc_ha_addr);
    fprintf(stderr,"%s,", (char *)clovis_conf.cc_profile);
    fprintf(stderr,"%s,", (char *)clovis_conf.cc_process_fid);
    fprintf(stderr,"%s", (char *)clovis_conf.cc_idx_service_conf);
	fprintf(stderr,"\n---\n");
	#endif

	/* clovis instance */
	rc = m0_clovis_init(&clovis_instance, &clovis_conf, true);
	if (rc != 0) {
		fprintf(stderr,"failed to initilise Clovis\n");
		return rc;
	}

	/* And finally, clovis root realm */
	m0_clovis_container_init(&clovis_container, 
				 NULL, &M0_CLOVIS_UBER_REALM,
				 clovis_instance);
	rc = clovis_container.co_realm.re_entity.en_sm.sm_rc;
	if (rc != 0) {
		fprintf(stderr,"failed to open uber realm\n");
		return rc;
	}

	/* success */
	clovis_uber_realm = clovis_container.co_realm;
	return 0;
}

/*
 * c0appz_free()
 * free clovis resources.
 */
int c0appz_free(void)
{
	m0_clovis_fini(clovis_instance, true);
	return 0;
}

/*
 * c0appz_setrc()
 * set c0apps resource filename
 */
int c0appz_setrc(char *rcfile)
{
	/* null */
	if(!rcfile) {
		fprintf(stderr, "error! null rc filename.\n");
		return 1;
	}

	/* update rc filename */
	memset(c0rcfile, 0x00, SZC0RCFILE);
	strncpy(c0rcfile, rcfile, strlen(rcfile));

	/* success */
	return 0;
}

/*
 * c0appz_putrc()
 * print c0apps resource filename
 */
int c0appz_putrc(void)
{
	/* print rc filename */
	fprintf(stderr, "%s", c0rcfile);
	fprintf(stderr, "\n");
	return 0;
}

/*
 *******************************************************************************
 * STATIC FUNCTIONS
 *******************************************************************************
 */

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
	m0_clovis_op_wait(
			ops[0],
			M0_BITS(M0_CLOVIS_OS_FAILED,M0_CLOVIS_OS_STABLE),
			M0_TIME_NEVER
			);
	rc = m0_clovis_rc(ops[0]);
	m0_clovis_op_fini(ops[0]);
	m0_clovis_op_free(ops[0]);
	ops[0] = NULL;

	return rc;
}

/*
 * create_object()
 * create clovis object.
 */
static int create_object(struct m0_uint128 id)
{
	int                  rc;
	struct m0_clovis_obj obj;
	struct m0_clovis_op *ops[1] = {NULL};

	memset(&obj, 0, sizeof(struct m0_clovis_obj));

	m0_clovis_obj_init(&obj, &clovis_uber_realm, &id,
			   m0_clovis_default_layout_id(clovis_instance));

	rc = open_entity(&obj.ob_entity);
	if (!(rc < 0)) {
		fprintf(stderr,"error! [%d]\n", rc);
		fprintf(stderr,"object already exists\n");
		return 1;
	}

	m0_clovis_entity_create(&obj.ob_entity, &ops[0]);

	m0_clovis_op_launch(ops, ARRAY_SIZE(ops));

	rc = m0_clovis_op_wait(
		ops[0], M0_BITS(M0_CLOVIS_OS_FAILED, M0_CLOVIS_OS_STABLE),
		m0_time_from_now(3,0));

	m0_clovis_op_fini(ops[0]);
	m0_clovis_op_free(ops[0]);
	m0_clovis_entity_fini(&obj.ob_entity);

	return 0;
}

static int read_data_from_file(FILE *fp, struct m0_bufvec *data, int bsz)
{
	int i;
	int rc;
	int nr_blocks;

	nr_blocks = data->ov_vec.v_nr;
	for (i = 0; i < nr_blocks; i++) {
		rc = fread(data->ov_buf[i], bsz, 1, fp);
		if (rc != 1)
			break;

		if (feof(fp))
			break;
	}

	return i;
}

static int write_data_to_object(struct m0_uint128 id,
				struct m0_indexvec *ext,
				struct m0_bufvec *data,
				struct m0_bufvec *attr)
{
	int                  rc;
	struct m0_clovis_obj obj;
	struct m0_clovis_op *ops[1] = {NULL};

	memset(&obj, 0, sizeof(struct m0_clovis_obj));

	/* Set the object entity we want to write */
	m0_clovis_obj_init(&obj, &clovis_uber_realm, &id,
			   m0_clovis_default_layout_id(clovis_instance));

	open_entity(&obj.ob_entity);

	/* Create the write request */
	m0_clovis_obj_op(&obj, M0_CLOVIS_OC_WRITE,
			 ext, data, attr, 0, &ops[0]);

	/* Launch the write request*/
	m0_clovis_op_launch(ops, 1);

	/* wait */
	rc = m0_clovis_op_wait(ops[0],
				M0_BITS(M0_CLOVIS_OS_FAILED,
				M0_CLOVIS_OS_STABLE),
				M0_TIME_NEVER);

	/* fini and release */
	m0_clovis_op_fini(ops[0]);
	m0_clovis_op_free(ops[0]);
	m0_clovis_entity_fini(&obj.ob_entity);

	return rc;
}


/*
 *******************************************************************************
 * UTILITY FUNCTIONS
 *******************************************************************************
 */

/*
 * trim()
 * trim leading/trailing white spaces.
 */
static char *trim(char *str)
{
	char *end;

	/* null */
	if(!str) return str;

	/* trim leading spaces */
	while(isspace((unsigned char)*str)) str++;

	/* all spaces */
	if(*str == 0) return str;

	/* trim trailing spaces, and */
	/* write new null terminator */
	end = str + strlen(str) - 1;
	while(end > str && isspace((unsigned char)*end)) end--;
	*(end+1) = 0;

	/* success */
	return str;
}

/*
 *******************************************************************************
 * END
 *******************************************************************************
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

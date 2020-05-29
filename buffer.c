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
 * Original creation date: 21-May-2020
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include <assert.h>

#include "c0appz.h"
#include "c0params.h"
#include "clovis/clovis.h"
#include "clovis/clovis_idx.h"

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */
extern int qos_total_weight; 		/* total bytes read or written 	*/
extern pthread_mutex_t qos_lock;	/* lock  qos_total_weight 		*/

/*
 ******************************************************************************
 * STATIC VARIABLES
 ******************************************************************************
 */
static	struct m0_indexvec extn;
static	struct m0_bufvec   data;
static	struct m0_bufvec   attr;

/*
 ******************************************************************************
 * STATIC FUNCTION PROTOTYPES
 ******************************************************************************
 */

static int freevecs(void);
static int initvecs(uint64_t pos,uint64_t bsz,uint64_t cnt);
static int bufvecw(const char *buf,uint64_t bsz,uint64_t cnt);
static int bufvecr(char *buf,uint64_t bsz,uint64_t cnt);

/*
 ******************************************************************************
 * EXTERN FUNCTIONS
 ******************************************************************************
 */

/*
 * c0appz_fr()
 * read from file.
 * copy cnt number of bsz size of blocks from input
 * file inf to a memory buffer buf. Assume buf is large
 * enough to copy bsz*cnt bytes.
 */
int c0appz_fr(char *buf, char *inf, uint64_t bsz, uint64_t cnt)
{
	uint64_t n;
	FILE *fp=NULL;

	/* open input file */
	fp=fopen(inf,"rb");
	if(!fp){
		fprintf(stderr,"%s(): error! - ",__FUNCTION__);
		fprintf(stderr,"could not open input file %s\n", inf);
		return 11;
	}

	/* read into buf */
	n=fread(buf,bsz,cnt,fp);
	if(n!=cnt){
		fprintf(stderr,"%s(): error! - ",__FUNCTION__);
		fprintf(stderr,"reading from %s failed.\n",inf);
		fclose(fp);
		return 22;
	}

	/* success */
	fclose(fp);
    return 0;
}

/*
 * c0appz_fw()
 * write to file.
 * copy cnt number of bsz size of blocks from input
 * memory buffer to output file ouf. Assume ouf is
 * large enough to copy bsz*cnt bytes.
 */
int c0appz_fw(char *buf, char *ouf, uint64_t bsz, uint64_t cnt)
{
	uint64_t n;
	FILE *fp=NULL;

	/* open output file */
	fp=fopen(ouf,"wb");
	if(!fp){
		fprintf(stderr,"%s(): error! - ",__FUNCTION__);
		fprintf(stderr,"could not open input file %s\n", ouf);
		return 11;
	}

	/* write to ouf */
	n=fwrite(buf,bsz,cnt,fp);
	if(n!=cnt){
		fprintf(stderr,"%s(): error! - ",__FUNCTION__);
		fprintf(stderr,"writing to %s failed.\n",ouf);
		fclose(fp);
		return 22;
	}

	/* success */
	fclose(fp);
    return 0;
}

/*
 * c0appz_mr()
 * read from mero object!
 * reads data from a mero object to memory buffer.
 * reads cnt number of blocks, each of size bsz from
 * pos (byte) position of the object
 */
int c0appz_mr(char *buf,uint64_t idhi,uint64_t idlo,uint64_t pos,uint64_t bsz,uint64_t cnt)
{
	struct m0_uint128 id;
	uint64_t max_bcnt_per_op;
	uint64_t block_count;

	/* ids */
	id.u_hi = idhi;
	id.u_lo = idlo;

	/* max_bcnt_per_op */
	assert(CLOVIS_MAX_PER_WIO_SIZE>bsz);
	max_bcnt_per_op = CLOVIS_MAX_PER_WIO_SIZE / bsz;
	max_bcnt_per_op = max_bcnt_per_op < CLOVIS_MAX_BLOCK_COUNT ?
			max_bcnt_per_op :
			CLOVIS_MAX_BLOCK_COUNT;

	while(cnt>0){
	    block_count = cnt > max_bcnt_per_op ? max_bcnt_per_op : cnt;
	    if(initvecs(pos,bsz,block_count)!=0){
	    	fprintf(stderr, "error! not enough memory!!\n");
			return 11;
	    }

		if(read_data_from_object(id, &extn, &data, &attr)!=0){
			fprintf(stderr, "reading from Mero object failed!\n");
			freevecs();
			return 22;
		}

		/* copy to memory */
		bufvecr(buf,bsz,block_count);

		/* QOS */
		pthread_mutex_lock(&qos_lock);
		qos_total_weight += block_count * bsz;
		pthread_mutex_unlock(&qos_lock);
		/* END */

		freevecs();
		buf += block_count*bsz;
		pos += block_count*bsz;
		cnt -= block_count;
	}

	return 0;
}

/*
 * c0appz_mw()
 * write to mero object!
 * writes data from memory buffer to a mero object.
 * writes cnt number of blocks, each of size bsz from
 * pos (byte) position of the object
 */
int c0appz_mw(const char *buf,uint64_t idhi,uint64_t idlo,uint64_t pos,uint64_t bsz,uint64_t cnt)
{
	struct m0_uint128 id;
	uint64_t max_bcnt_per_op;
	uint64_t block_count;

	/* ids */
	id.u_hi = idhi;
	id.u_lo = idlo;

	/* max_bcnt_per_op */
	assert(CLOVIS_MAX_PER_WIO_SIZE>bsz);
	max_bcnt_per_op = CLOVIS_MAX_PER_WIO_SIZE / bsz;
	max_bcnt_per_op = max_bcnt_per_op < CLOVIS_MAX_BLOCK_COUNT ?
			max_bcnt_per_op :
			CLOVIS_MAX_BLOCK_COUNT;

	while(cnt>0){
	    block_count = cnt > max_bcnt_per_op ? max_bcnt_per_op : cnt;
	    if(initvecs(pos,bsz,block_count)!=0){
	    	fprintf(stderr, "error! not enough memory!!\n");
			return 11;
	    }
	    bufvecw(buf,bsz,block_count);
		if(write_data_to_object(id, &extn, &data, &attr)!=0){
			fprintf(stderr, "writing to Mero object failed!\n");
			freevecs();
			return 22;
		}

		/* QOS */
		pthread_mutex_lock(&qos_lock);
		qos_total_weight += block_count * bsz;
		pthread_mutex_unlock(&qos_lock);
		/* END */

		freevecs();
		buf += block_count*bsz;
		pos += block_count*bsz;
		cnt -= block_count;
	}

	return 0;
}

/*
 ******************************************************************************
 * STATIC FUNCTIONS
 ******************************************************************************
 */

/* initvecs() */
static int initvecs(uint64_t pos,uint64_t bsz,uint64_t cnt)
{
	int r=0;
	int i=0;

    /* allocate buffers */
    r = m0_bufvec_alloc(&data, cnt, bsz) ?:
    		m0_bufvec_alloc(&attr, cnt, 1) ?:
    				m0_indexvec_alloc(&extn, cnt);
    if(r){
    	freevecs();
    	fprintf(stderr, "error! failed to allocate bufvecs!!\n");
    	return 1;
    }

    /* prepare index */
	for(i=0; i<cnt; i++)
	{
		extn.iv_index[i] = pos;
		extn.iv_vec.v_count[i] = bsz;
		attr.ov_vec.v_count[i] = 0; /* no attributes */
		pos += bsz;
	}

	/* success */
	return 0;
}

/* bufvecw() */
static int bufvecw(const char *buf,uint64_t bsz,uint64_t cnt)
{
	int i=0;
	/* copy block by block */
	assert(data.ov_vec.v_nr == cnt);
	for (i=0; i<cnt; i++){
		memmove(data.ov_buf[i],buf,bsz);
		buf += bsz;
	}
	/* success */
	return 0;
}

/* bufvecr() */
static int bufvecr(char *buf,uint64_t bsz,uint64_t cnt)
{
	int i=0;
	/* copy block by block */
	assert(data.ov_vec.v_nr == cnt);
	for (i=0; i<cnt; i++){
		memmove(buf,data.ov_buf[i],bsz);
		buf += bsz;
	}
	/* success */
	return 0;
}

/* freevecs() */
static int freevecs(void)
{
	m0_bufvec_free(&data);
	m0_bufvec_free(&attr);
	m0_indexvec_free(&extn);
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

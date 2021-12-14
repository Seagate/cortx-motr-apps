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
 * Original author:  Ganesan Umanesan <ganesan.umanesan@seagate.com>
 * Original creation date: 06-Dec-2021
 */

#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <pthread.h>
#include "c0appz.h"
#include "c0appz_internal.h"
#include "list.h"

struct list *flist; /* file list 			*/
struct list *tlist; /* available threads 	*/
struct list *plist; /* active thread pool 	*/

struct fBlock {
	uint64_t idh;	/* object id high    	*/
	uint64_t idl;   /* object is low     	*/
	uint64_t bsz;   /* block size        	*/
	uint64_t cnt;  	/* count             	*/
	uint64_t fsz;  	/* file size           	*/
	uint64_t ocnt;	/* OP count    	 		*/
	uint64_t m0bs;	/* m0 block size     	*/
	uint64_t pos;   /* starting position 	*/
	int	pool;		/* pool ID				*/
	char fbuf[256];	/* file buffer     		*/
	int threadidx;	/* thread id			*/
};

#define MAXTHREADS 100
pthread_t fthrd[MAXTHREADS];
struct fBlock fthrd_blk[MAXTHREADS];
pthread_mutex_t tlist_lock=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t flist_lock=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gidlo_lock=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t print_lock=PTHREAD_MUTEX_INITIALIZER;

/* QOS */
extern uint64_t qos_whgt_served;
extern uint64_t qos_whgt_remain;
extern uint64_t qos_laps_served;
extern uint64_t qos_laps_remain;
extern pthread_mutex_t qos_lock;

/* object ID */
uint64_t g_idlo;

static void pack(uint64_t idh, uint64_t idl, char *fbuf, uint64_t bsz,
		int pool, uint64_t m0bs, int idx);


/*
 * dir_qos_init()
 * Note. total bytes copied/read depends on bsz
 * because Motr does block aligned IO.
 * */
static int dir_qos_init(uint64_t bsz)
{
	struct list *ptr;
	struct stat64 fs;

	qos_whgt_served = 0;
	qos_whgt_remain = 0;
	qos_laps_served = 0;
	qos_laps_remain = 0;

	ptr = flist;
	while(ptr!=NULL) {
		qos_laps_remain++;
		stat64(ptr->data, &fs);
		qos_whgt_remain += bsz*((fs.st_size + bsz - 1) / bsz);
		ptr = ptr->next;
	}

	return 0;
}

static void *release_idx(void *idx)
{
	pthread_mutex_lock(&tlist_lock);
	push(&tlist,(void *)idx,sizeof(int));
	pthread_mutex_unlock(&tlist_lock);
	return NULL;
}

static void *tfunc_c0appz_cp(void *block)
{
	int rc=0;
	struct fBlock *b = (struct fBlock *)block;
	pthread_cleanup_push((void *)release_idx, (void *)&(b->threadidx));

	/* output meta data */
	pthread_mutex_lock(&print_lock);
	printf("%s ", b->fbuf);
	printf("%" PRIu64 " " "%" PRIu64 " ", b->idh,b->idl);
	printf("%" PRIu64, b->fsz);
	printf("\n");
	pthread_mutex_unlock(&print_lock);

	/* write file */
	rc = c0appz_cp(b->idh, b->idl, b->fbuf, b->bsz, b->cnt, b->m0bs);
	if (rc != 0) {
		ERR("copying failed: rc=%d\n", rc);
		rc = 222;
	}
	/* QOS */
	pthread_mutex_lock(&qos_lock);
	qos_laps_served++;
	qos_laps_remain--;
	pthread_mutex_unlock(&qos_lock);
	/* QOS */


	/* schedule work */
	while(flist) {
		pthread_mutex_lock(&flist_lock);
		pop(&flist,(void *)b->fbuf);
		pthread_mutex_unlock(&flist_lock);
		//printf("[%d] [%s]\n", b->threadidx,b->fbuf);

		/* update idlo */
		pthread_mutex_lock(&gidlo_lock);
		b->idl = g_idlo;
		g_idlo++;
		pthread_mutex_unlock(&gidlo_lock);

		/* output meta data */
		pthread_mutex_lock(&print_lock);
		printf("%s ", b->fbuf);
		printf("%" PRIu64 " " "%" PRIu64 " ", b->idh,b->idl);
		printf("%" PRIu64, b->fsz);
		printf("\n");
		pthread_mutex_unlock(&print_lock);

		/* write file */
		pack(b->idh, b->idl, b->fbuf, b->bsz, b->pool, b->m0bs, b->threadidx);
		rc = c0appz_cp(b->idh, b->idl, b->fbuf, b->bsz, b->cnt, b->m0bs);
		if (rc != 0) {
			ERR("copying failed: rc=%d\n", rc);
			rc = 222;
		}
		/* QOS */
		pthread_mutex_lock(&qos_lock);
		qos_laps_served++;
		qos_laps_remain--;
		pthread_mutex_unlock(&qos_lock);
		/* QOS */
	}

	pthread_cleanup_pop(1);
    return NULL;
}

static void pack(uint64_t idh, uint64_t idl, char *fbuf, uint64_t bsz,
		int pool, uint64_t m0bs, int idx)
{
	int rc=0;
	struct stat64 fs;

	rc = stat64(fbuf, &fs);
	if (rc != 0) {
		ERRS("%s", fbuf);
		rc = 555;
	}
	fthrd_blk[idx].cnt  = (fs.st_size + bsz - 1) / bsz;
	fthrd_blk[idx].fsz  = fs.st_size;
	fthrd_blk[idx].ocnt = 0;
	fthrd_blk[idx].pos  = 0;

	fthrd_blk[idx].idh = idh;
	fthrd_blk[idx].idl = idl;
	fthrd_blk[idx].bsz = bsz;
	fthrd_blk[idx].pool = pool;
	fthrd_blk[idx].m0bs = m0bs;
	fthrd_blk[idx].threadidx = idx;

	memcpy(fthrd_blk[idx].fbuf, fbuf, strlen(fbuf)+1);
	return;
}

static void dir_extract_files(char *dirstr)
{
	DIR *d;
	struct dirent *dir;
	char filename[256];
	char dirname[256];
	int rc=0;

	/* check directory name */
	memcpy(dirname, dirstr, sizeof(dirname));
	if(*(dirstr+strlen(dirstr)-1)!='/')
		snprintf(dirname, sizeof(dirname), "%s/", dirstr);
	printf("[ %s ]\n", dirname);

	/* list files */
	linit(&flist);
	d = opendir(dirname);
	if (d) {
		while ((dir = readdir(d)) != NULL) {
			if(dir-> d_type != DT_DIR) {
				rc = snprintf(filename, sizeof(filename), "%s%s", dirname, dir->d_name);
				if (rc < 0) {
					ERR("snprintf failed: rc=%d\n", rc);
					rc = 333;
				}
				push(&flist,(void *)filename,strlen(filename)+1);
			}
			else {
				if (!strcmp(dir->d_name,".") || !strcmp(dir->d_name,"..")) continue;
				// printf("[%s]\n", dir->d_name);
				continue;
			}
		}
		closedir(d);
	}

	return;
}

/*
 * c0appz_cp_dir_sthread()
 * copy entire directory to objects using a single thread,
 *  that is, no multi-threading.
 */
int c0appz_cp_dir_sthread(uint64_t idhi, uint64_t idlo, char *dirname,
						uint64_t bsz, int pool, uint64_t m0bs)
{
	int i=0;
	int rc=0;
	int sz=0;
	struct stat64 fs;
	char fbuf[256];
	uint64_t cnt=0;

	dir_extract_files(dirname);
	//list_print_str(flist);
	sz = lsize(&flist);

	/* delete objects */
	printf("deleting %d existing objects...\n", sz);
	for(i=0; i<sz; i++) {
		if(!(i%100) && (i/100>0)) printf("%d objects deleted\n",i);
		rc = c0appz_rm(idhi, idlo+i);
		if (rc < 0) {
			ERR("object delete failed: rc=%d\n", rc);
			rc = 333;
		}
	}
	printf("done\n");

	/* create objects */
	printf("creating %d new objects...\n", sz);
	for(i=0; i<sz; i++) {
		if(!(i%100) && (i/100>0)) printf("%d objects created\n",i);
		rc = c0appz_cr(idhi, idlo+i, pool, m0bs);
		if (rc < 0) {
			ERR("object create failed: rc=%d\n", rc);
			rc = 333;
		}
	}
	printf("done\n");

	/* qos */
	dir_qos_init(bsz);
	printf("%" PRIu64 " " "%" PRIu64 "\n", qos_whgt_remain,qos_whgt_served);

	/* write files */
	while(flist)  {
		pop(&flist,(void *)fbuf);
		stat64(fbuf, &fs);
		cnt = (fs.st_size + bsz - 1) / bsz;

		/* output meta data */
		printf("%s ", fbuf);
		printf("%" PRIu64 " " "%" PRIu64 " ", idhi,idlo);
		printf("%" PRIu64, fs.st_size);
		printf("\n");

		/* write file */
		rc = c0appz_cp(idhi, idlo, fbuf, bsz, cnt, m0bs);
		if (rc != 0) {
			ERR("copying failed: rc=%d\n", rc);
			rc = 222;
		}
		idlo++;

	}

	return 0;
}

/*
 * c0appz_cp_dir_mthread()
 * copy entire directory to objects using multiple threads
 */
int c0appz_cp_dir_mthread(uint64_t idhi, uint64_t idlo, char *dirname,
						uint64_t bsz, int pool, uint64_t m0bs, int numthreads)
{
	int i=0;
	char fbuf[256];
	int idx;
	int rc=0;
	int sz=0;

	if(numthreads > MAXTHREADS) {
		numthreads = MAXTHREADS;
		fprintf(stderr,"WARN! num of threads is set to %d!!\n", MAXTHREADS);
	}

	dir_extract_files(dirname);
	//list_print_str(flist);
	sz = lsize(&flist);

	/* setup thread pool */
	linit(&tlist);
	for(i=0; i<numthreads; i++) {
		push(&tlist,(void *)&i,sizeof(int));
	}
	//list_print_int(tlist);
	linit(&plist);

	/* delete objects */
	printf("deleting %d existing objects...\n", sz);
	for(i=0; i<sz; i++) {
		if(!(i%100) && (i/100>0)) printf("%d objects deleted\n",i);
		rc = c0appz_rm(idhi, idlo+i);
		if (rc < 0) {
			ERR("object delete failed: rc=%d\n", rc);
			rc = 333;
		}
	}
	printf("done\n");

	/* create objects */
	printf("creating %d new objects...\n", sz);
	for(i=0; i<sz; i++) {
		if(!(i%100) && (i/100>0)) printf("%d objects created\n",i);
		rc = c0appz_cr(idhi, idlo+i, pool, m0bs);
		if (rc < 0) {
			ERR("object create failed: rc=%d\n", rc);
			rc = 333;
		}
	}
	printf("done\n");


	/* setup ids */
	g_idlo = idlo;

	/* qos */
	dir_qos_init(bsz);
	printf("%" PRIu64 " " "%" PRIu64 "\n", qos_whgt_remain,qos_whgt_served);

	/* schedule threads */
	while((flist) && (tlist))  {
		pop(&tlist,(void *)(&idx));
		pthread_mutex_lock(&flist_lock);
		pop(&flist,(void *)fbuf);
		pthread_mutex_unlock(&flist_lock);
		//printf("[%d] [%s]\n", idx,fbuf);

		/* update idlo */
		pthread_mutex_lock(&gidlo_lock);
		idlo = g_idlo;
		g_idlo++;
		pthread_mutex_unlock(&gidlo_lock);

		/* start threads */
		pack(idhi, idlo, fbuf, bsz, pool, m0bs, idx);
		push(&plist,(void *)&idx,sizeof(int));
		pthread_create(&(fthrd[idx]),NULL,&tfunc_c0appz_cp,(void *)(fthrd_blk+idx));
	}

	return 0;
}

int c0appz_cp_dir_mthread_wait()
{
	int idx;
	while(plist!=NULL) {
		pop(&plist,(void *)(&idx));
		pthread_join(fthrd[idx],NULL);
	}
	lfree(&tlist);
	lfree(&plist);
	return 0;
}


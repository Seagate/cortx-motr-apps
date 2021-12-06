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

#define MAXTHREADS 50
pthread_t fthrd[MAXTHREADS];
struct fBlock fthrd_blk[MAXTHREADS];
pthread_mutex_t tlist_lock=PTHREAD_MUTEX_INITIALIZER;

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
	printf("%s ", b->fbuf);
	printf("%" PRIu64 " " "%" PRIu64 " ", b->idh,b->idl);
	printf("%" PRIu64, b->fsz);
	printf("\n");

	/* create object */
	rc = c0appz_cr(b->idh, b->idl, b->pool, b->m0bs);
	if (rc < 0) {
		ERR("object create failed: rc=%d\n", rc);
		rc = 333;
	}

	/* write file */
	rc = c0appz_cp(b->idh, b->idl, b->fbuf, b->bsz, b->cnt, b->m0bs);
	if (rc != 0) {
		ERR("copying failed: rc=%d\n", rc);
		rc = 222;
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

	fthrd_blk[idx].idh  = idh;
	fthrd_blk[idx].idl  = idl;
	fthrd_blk[idx].bsz  = bsz;
	fthrd_blk[idx].pool = pool;
	fthrd_blk[idx].m0bs = m0bs;
	fthrd_blk[idx].threadidx = idx;

	memcpy(fthrd_blk[idx].fbuf, fbuf, strlen(fbuf)+1);
	return;
}

void static dir_extract_files(char *dirname)
{
	DIR *d;
	struct dirent *dir;
	char filename[256];

	linit(&flist);
	d = opendir(dirname);
	if (d) {
		while ((dir = readdir(d)) != NULL) {
			if(dir-> d_type != DT_DIR) {
				snprintf(filename, 256, "%s%s", dirname, dir->d_name);
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
 * c0appz_cp_dir_mthread()
 * copy entire directory to objects using multiple threads
 */
int c0appz_cp_dir_mthread(uint64_t idhi, uint64_t idlo, char *dirname,
						uint64_t bsz, int pool, uint64_t m0bs, int numthreads)
{
	int i=0;
	char fbuf[256];
	int idx;

	if(numthreads > MAXTHREADS) {
		numthreads = MAXTHREADS;
		fprintf(stderr,"WARN! num of threads is set to %d!!\n", MAXTHREADS);
	}

	dir_extract_files(dirname);
	//list_print_str(flist);

	/* setup thread pool */
	linit(&tlist);
	for(i=0; i<numthreads; i++) {
		push(&tlist,(void *)&i,sizeof(int));
	}
	//list_print_int(tlist);
	linit(&plist);

	/* schedule threads */
	while((flist) && (tlist))  {
		pop(&tlist,(void *)(&idx));
		pop(&flist,(void *)fbuf);
		//printf("[%d] [%s]\n", idx,fbuf);

		pack(idhi, idlo, fbuf, bsz, pool, m0bs, idx);
		push(&plist,(void *)&idx,sizeof(int));
		pthread_create(&(fthrd[idx]),NULL,&tfunc_c0appz_cp,(void *)(fthrd_blk+idx));

		idlo++;
		if(!flist) break;
		if(!tlist) sleep(1);
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

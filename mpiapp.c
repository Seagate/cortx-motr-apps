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
 * Original creation date: 24-Jan-2017
 */

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include "c0appz.h"

/* main */
int main(int argc, char **argv)
{
	int wsize;
	int wrank;
	char pname[MPI_MAX_PROCESSOR_NAME];
	int pnlen;

	/* MPI start */
	MPI_Init(NULL, NULL);
	MPI_Comm_size(MPI_COMM_WORLD, &wsize);
	MPI_Comm_rank(MPI_COMM_WORLD, &wrank);
	MPI_Get_processor_name(pname, &pnlen);

	/*
	 * rank index
	 * gather all node ids and ranks
	 * and determine rank index.
	 */
	int node;
	node = atoi((char *)(pname+7));
	node = (node * 1000) + wrank;
	int *nptr = (int *)malloc(sizeof(int) * wsize);
	MPI_Allgather(&node, 1, MPI_INT, nptr, 1, MPI_INT, MPI_COMM_WORLD);
	MPI_Barrier(MPI_COMM_WORLD);

	int  idx;
	int *ptr;
	idx = 0;
	ptr = nptr;
	while(ptr != nptr + wsize) {
		if((*ptr/1000 == node/1000) && (node > *ptr)) idx++;
		ptr++;
	}
	printf("[%s] [%d] rank = %d idx = %d\n", pname,wsize,wrank,idx);

	/* time in */
	c0appz_timein();

	/* c0rcfile
	 * overwrite .cappzrc to a .[app]rc file.
	 */
	char str[256];
	sprintf(str,".%src",basename(argv[0]));
	c0appz_setrc(str);
	c0appz_putrc();

	/* initialize resources */
	if (c0appz_init(idx) != 0) {
		fprintf(stderr,"error! clovis initialization failed.\n");
		return -2;
	}

	/*
	 * do something
	 */

	/* free resources*/
	c0appz_free();

	/* time out */
	c0appz_timeout(0);

	/* success */
	fprintf(stderr,"%s success\n",basename(argv[0]));
	
	/* MPI end */
	MPI_Finalize();
	
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

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
 * Original creation date: 18-May-2020
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/*
 ******************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************
 */
int qos_total_weight=0; 	/* total bytes read or written 	*/
pthread_mutex_t qos_lock;	/* lock  qos_total_weight 		*/
static pthread_t tid;		/* real-time bw thread			*/
extern int perf; 			/* option performance 			*/

/*
 ******************************************************************************
 * STATIC FUNCTION PROTOTYPES
 ******************************************************************************
 */
static int qos_print_bw(void);
static void *disp_realtime_bw(void *arg);

/*
 ******************************************************************************
 * EXTERN FUNCTIONS
 ******************************************************************************
 */


/* qos_pthread_start() */
int qos_pthread_start(void)
{
	if(!perf) return 0;
	pthread_create(&tid,NULL,&disp_realtime_bw,NULL);
    return 0;
}

/* qos_pthread_stop() */
int qos_pthread_stop(int s)
{
	if(s) return 0;
	if(!perf) return 0;
//	pthread_join(tid,NULL);
	pthread_cancel(tid);
    return 0;
}

/*
 ******************************************************************************
 * STATIC FUNCTIONS
 ******************************************************************************
 */

/*
 * qos_print_bw()
 */
static int qos_print_bw(void)
{
	double bw=0;
	bw=(double)qos_total_weight/1000000;
	/* reset total weight */
	pthread_mutex_lock(&qos_lock);
	qos_total_weight=0;
	pthread_mutex_unlock(&qos_lock);
	printf("bw = %08.4f MB/s\n",bw);
	return 0;
}

/*
 * disp_realtime_bw()
 * thread function that displays real-time
 * bandwidth
 */
static void *disp_realtime_bw(void *arg)
{
    while(1)
    {
        qos_print_bw();
        sleep(1);
    }
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

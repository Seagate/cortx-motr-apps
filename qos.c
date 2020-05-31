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
#include <inttypes.h>
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

uint64_t qos_whgt_served=0;
uint64_t qos_whgt_remain=0;
uint64_t qos_laps_served=0;
uint64_t qos_laps_remain=0;

/*
 ******************************************************************************
 * STATIC FUNCTION PROTOTYPES
 ******************************************************************************
 */
static int qos_print_bw(void);
static void *disp_realtime_bw(void *arg);
static int progress_rt(char *s);
static int progress_rb(char *s);

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
	if(qos_whgt_remain>0) return 0;
	pthread_cancel(tid);
    return 0;
}

/* qos_pthread_wait() */
int qos_pthread_wait()
{
	pthread_join(tid,NULL);
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
	double pr=0;
	char s[16];
	uint64_t tot1=0;
	uint64_t tot2=0;

	bw=(double)qos_total_weight/1000000;
	tot1 = qos_laps_served+qos_laps_remain;
	tot2 = qos_whgt_served+qos_whgt_remain;

	/* reset total weight */
	pthread_mutex_lock(&qos_lock);
	qos_whgt_served += qos_total_weight;
	qos_whgt_remain -= qos_total_weight;
	qos_total_weight=0;
//	qos_laps_served = (tot1*qos_whgt_served)/tot2;
//	qos_laps_remain = tot1- qos_laps_served;
	pthread_mutex_unlock(&qos_lock);

	/* print */
	pr=100*qos_whgt_served/(qos_whgt_served+qos_whgt_remain);
	printf("bw = %08.4f MB/s\n",bw);
	sprintf(s,"%02d/%02d",(int)qos_laps_served,(int)(qos_laps_served+qos_laps_remain));
	progress_rt(s);
	sprintf(s,"%3d%%",(int)pr);
	progress_rb(s);
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
        if(qos_whgt_remain<=0) pthread_cancel(tid);
        sleep(1);
    }
    return 0;
}

/*
 * progress_rb()
 * Update progress information at the right
 * bottom corner of the console.
 */
static int progress_rb(char *s)
{
//	return 0;
	printf("\033[s");
	printf("\033[99999;99999H");
	printf("\033[8D");
//	printf("\033[1A\033[K\033[1B");
	printf("[ %5s ]",s);
	printf("\033[u");
	return 0;
}

/*
 * progress_rt()
 * Update progress information at the right
 * top corner of the console.
 */
static int progress_rt(char *s)
{
//	return 0;
	printf("\033[s");
	printf("\033[99999;99999H");
	printf("\033[8D");
	printf("\033[1A");
	printf("\033[1A\033[K\033[1B");
	printf("[ %5s ]",s);
	printf("\033[u");
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

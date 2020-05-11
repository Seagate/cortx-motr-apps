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
 *
 * compile:
 * gcc -Wall -lssl -lcrypto c0fidgen.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <inttypes.h>
#include <time.h>
#include <openssl/md5.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>

#ifndef DEBUG
#define DEBUG 0
#endif

#define FGENRC ".c0fgenrc"

/* function prototypes */
int m_addr(char *mbuf, int msz);

/* main */
int main(int argc, char **argv)
{
	uint64_t  idh;	/* object id high 	*/
	uint64_t  idl;	/* object id low	*/
    FILE *fp;
    MD5_CTX c;
    char buf[512];
    unsigned char chksum[16];

	idh = 0;
	idl = 0;

	/* init */
    MD5_Init(&c);

    /* utc */
    memset(buf, 0x00, 512);
    sprintf(buf, "%d\n", (int)time(NULL));
	#if DEBUG
    fprintf(stderr, "%s:\n",__FUNCTION__);
    fprintf(stderr,"%s",buf);
    fprintf(stderr,"size = %d\n",(int)strlen(buf));
	#endif
    MD5_Update(&c, buf, strlen(buf));

    /* srandom */
	srandom(0);
    memset(buf, 0x00, 512);
    sprintf(buf, "%d\n", (int)random());
	#if DEBUG
    fprintf(stderr, "%s:\n",__FUNCTION__);
    fprintf(stderr,"%s",buf);
    fprintf(stderr,"size = %d\n",(int)strlen(buf));
	#endif
    MD5_Update(&c, buf, strlen(buf));

    /*
     * TO DO
     * add more salt here.
     */

    /* local MAC/IP addresses */
	if(m_addr(buf,512)!=0)
	{
		fprintf(stderr,"error! insufficient salt.\n");
		fprintf(stderr,"ioctl() could not obtain mac addresses.\n");
		return -2;
	}
	#if DEBUG
	fprintf(stderr, "%s:\n",__FUNCTION__);
	fprintf(stderr,"%s",buf);
	fprintf(stderr,"size = %d\n",(int)strlen(buf));
	#endif
    MD5_Update(&c, buf, strlen(buf));

    /* host/user */
    sprintf(buf, "%s/%s\n", getenv("HOSTNAME"),getenv("USER"));
	#if DEBUG
    fprintf(stderr, "%s:\n",__FUNCTION__);
    fprintf(stderr,"%s",buf);
    fprintf(stderr,"size = %d\n",(int)strlen(buf));
	#endif
    MD5_Update(&c, buf, strlen(buf));

    char fname[128] = {0};
    char *homed = getenv("HOME");
	snprintf(fname,128,"%s/%s",homed,FGENRC);

	/*
	 * create file anyway
	 * if it does not exist
	 * */
    fp = fopen(fname, "a");
    fclose(fp);

	/* read counter */
    fp = fopen(fname, "r");
    if(!fp)
    {
        fprintf(stderr,"error! could not open resource file %s\n", fname);
        fprintf(stderr,"touch %s\n", fname);
        return -1;
    }

    memset(buf, 0x00, 512);
    fgets(buf, 512, fp);
    fclose(fp);

    sprintf(buf, "%d\n", (int)atoi(buf));
	#if DEBUG
    fprintf(stderr, "%s:\n",__FUNCTION__);
    fprintf(stderr,"%s",buf);
    fprintf(stderr,"size = %d\n",(int)strlen(buf));
	#endif
    MD5_Update(&c, buf, strlen(buf));

    /* write counter */
    fp = fopen(fname, "w");
    if(!fp)
    {
        fprintf(stderr,"error! could not open resource file %s\n", fname);
        return -1;
    }

    fprintf(fp, "%d\n", (int)atoi(buf)+1);
    fclose(fp);


	/* final */
    MD5_Final(chksum, &c);

	#if DEBUG
    int n;
    fprintf(stderr, "md5: ");
    for(n=0; n<MD5_DIGEST_LENGTH; n++) fprintf(stderr, "%02x", chksum[n]);
    fprintf(stderr, "\n");
	#endif

	#if DEBUG
	fprintf(stderr, "[%" PRIu64 ", " "%" PRIu64 "]", idh, idl);
	fprintf(stderr, "\n");
	#endif

	memmove(&idh, &chksum[0], sizeof(uint64_t));
	memmove(&idl, &chksum[8], sizeof(uint64_t));
	printf("%" PRIu64 " " "%" PRIu64, idh,idl);
	printf("\n");

	/* success */
	#if DEBUG
	fprintf(stderr,"%s success\n",basename(argv[0]));
	#endif
	return 0;
}

/*
 * m_addr()
 */
int m_addr(char *mbuf, int msz)
{
	char buf[8192] = {0};
	struct ifconf ifc = {0};
	struct ifreq *ifr = NULL;
	int sock = 0;						/* socket				*/
	int nifs = 0;						/* number of interfaces	*/
	char macp[19];						/* mac address buffer	*/
	char ip[INET6_ADDRSTRLEN] = {0};	/* ip address			*/
	int i = 0;
	struct ifreq *item;
	struct sockaddr *addr;

	/* clear buffer */
	memset(mbuf, 0x00, msz);

	/* get a socket handle. */
	sock = socket(PF_INET, SOCK_DGRAM, 0);
	if(sock < 0)
	{
		perror("socket");
		return 1;
	}

	/* query available interfaces. */
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if(ioctl(sock, SIOCGIFCONF, &ifc) < 0)
	{
		perror("ioctl(SIOCGIFCONF)");
		return 2;
	}

	/* iterate through the list of interfaces. */
	ifr = ifc.ifc_req;
	nifs = ifc.ifc_len / sizeof(struct ifreq);

	for(i = 0; i < nifs; i++)
	{
		item = &ifr[i];
		addr = &(item->ifr_addr);

		/* get the IP address*/
		if(ioctl(sock, SIOCGIFADDR, item) < 0)
		{
			perror("ioctl(OSIOCGIFADDR)");
		}

		if(!inet_ntop(AF_INET,
				&(((struct sockaddr_in *)addr)->sin_addr),ip,sizeof(ip)))
		{
			perror("inet_ntop");
			continue;
		}

		/* get the MAC address */
		if(ioctl(sock, SIOCGIFHWADDR, item) < 0)
		{
			perror("ioctl(SIOCGIFHWADDR)");
			return 3;
		}

		/* display result */
		sprintf(macp, "%02x:%02x:%02x:%02x:%02x:%02x",
				(unsigned char)item->ifr_hwaddr.sa_data[0],
				(unsigned char)item->ifr_hwaddr.sa_data[1],
				(unsigned char)item->ifr_hwaddr.sa_data[2],
				(unsigned char)item->ifr_hwaddr.sa_data[3],
				(unsigned char)item->ifr_hwaddr.sa_data[4],
				(unsigned char)item->ifr_hwaddr.sa_data[5]
				);

		#if DEBUG
		fprintf(stderr, "%s: ",__FUNCTION__);
		fprintf(stderr,"mac -> [%s] ip -> [%16s]\n", macp, ip);
		#endif
		sprintf(mbuf+strlen(mbuf),"mac -> [%s] ip -> [%16s]\n", macp, ip);

	}

	/* success */
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

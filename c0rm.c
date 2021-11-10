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
 *
 * Original author:  Ganesan Umanesan <ganesan.umanesan@seagate.com>
 * Original creation date: 10-Jan-2017
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include "c0appz.h"
#include "c0appz_internal.h"

/*
 ******************************************************************************
 * EXTERN VARIABLES
 ******************************************************************************
 */
extern int perf; /* performance */

const char *help_c0rm_txt = "\
Usage:\n\
	\n\
c0rm [option] idh idl\n\
c0rm 1234 56789\n\
c0rm -y 1234 56789\n\
\n\
idh - Motr fid high\n\
idl - Motr fid low\n\
\n\
option is:\n\
	-y | yes\n\
	-p | performance\n\
	-t | create m0trace.pid file\n\
	-i | n socket index, [0-3] currently\n\
		\n\
The -y option forces deleting an object if that object already exists.\n\
The -p option enables performance monitoring. It collects performance stats\n\
such as cpu time, wall clock time, bandwidth, etc., and displays them at the\n\
end of the execution.\n\
c0rm -p 1234 56789\n";

char *prog;

int help()
{
	fprintf(stderr,"%s\n%s\n", help_c0rm_txt, c0appz_help_txt);
	exit(1);
}

int main(int argc, char **argv)
{
	uint64_t idh;	/* object id high 	*/
	uint64_t idl;	/* object id low 	*/
	int opt=0;		/* options			*/
	int idx=0;		/* socket index		*/
	int rc=0;
	int yes=0;

	prog = basename(strdup(argv[0]));

	/* getopt */
	while((opt = getopt(argc, argv, ":i:pyt"))!=-1){
		switch(opt){
			case 'p':
				perf = 1;
				break;
			case 'y':
				yes = 1;
				break;
			case 't':
				m0trace_on = true;
				break;
			case 'i':
				idx = atoi(optarg);
				if (idx < 0 || idx > 3) {
					ERR("invalid socket index: %s (allowed \n"
					    "values are 0, 1, 2, or 3 atm)\n", optarg);
					help();
				}
				break;
			case ':':
				fprintf(stderr,"option needs a value\n");
				help();
				break;
			case '?':
				fprintf(stderr,"unknown option: %c\n", optopt);
				help();
				break;
			default:
				fprintf(stderr,"unknown option: %c\n", optopt);
				help();
				break;
		}
	}

	/* check input */
	if(argc-optind!=2){
		help();
	}

	c0appz_setrc(prog);
	c0appz_putrc();

	/* set input */
	if (sscanf(argv[optind+0], "%li", &idh) != 1) {
		ERR("invalid idhi - %s", argv[optind+0]);
		help();
	}
	if (sscanf(argv[optind+1], "%li", &idl) != 1) {
		ERR("invalid idlo - %s", argv[optind+1]);
		help();
	}

	/* init */
	c0appz_timein();
	rc = c0appz_init(idx);
	if (rc != 0) {
		fprintf(stderr,"error! c0appz_init() failed: %d\n", rc);
		return 222;
	}
	ppf("init");
	c0appz_timeout(0);

	/* check */
	c0appz_timein();
	if (!c0appz_ex(idh, idl, NULL)) {
		fprintf(stderr,"error!\n");
		fprintf(stderr,"object NOT found!!\n");
		rc = 1;
	}
	ppf("chck");
	c0appz_timeout(0);

	if (rc == 1) goto end;

	if (!yes) {
		int c=0;
		printf("delete object(y/n):");
		c = getchar();
		if (c != 'y') {
			fprintf(stderr,"object NOT deleted.\n");
			goto end;
		}
	}

	/* delete */
	c0appz_timein();
	if(c0appz_rm(idh,idl) != 0){
		fprintf(stderr,"error! delete object failed.\n");
		rc = 333;
		goto end;
	};
	ppf("%4s","rm");
	c0appz_timeout(0);
	fprintf(stderr,"success! object deleted!!\n");

end:

	/* c0appz_free */
	c0appz_timein();
	c0appz_free();
	ppf("free");
	c0appz_timeout(0);

	/* failure */
	if(rc>1){
		fprintf(stderr,"%s failed!\n", prog);
		return rc;
	}

	/* success */
	c0appz_dump_perf();
	fprintf(stderr,"%s success\n", prog);
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

/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * genl.c		"genl" utility frontend.
 *
 * Authors:	Jamal Hadi Salim
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h> /* until we put our own header */
#include "version.h"
#include "utils.h"
#include "genl_utils.h"

int show_stats;
int show_details;
int show_raw;

static void *BODY;
static struct genl_util *genl_list;


static int print_nofopt(struct nlmsghdr *n, void *arg)
{
	fprintf((FILE *) arg, "unknown genl type ..\n");
	return 0;
}

static int parse_nofopt(struct genl_util *f, int argc, char **argv)
{
	if (argc) {
		fprintf(stderr,
			"Unknown genl \"%s\", hence option \"%s\" is unparsable\n",
			f->name, *argv);
		return -1;
	}

	return 0;
}

static struct genl_util *get_genl_kind(const char *str)
{
	void *dlh;
	char buf[256];
	struct genl_util *f;

	for (f = genl_list; f; f = f->next)
		if (strcmp(f->name, str) == 0)
			return f;

	snprintf(buf, sizeof(buf), "%s.so", str);
	dlh = dlopen(buf, RTLD_LAZY);
	if (dlh == NULL) {
		dlh = BODY;
		if (dlh == NULL) {
			dlh = BODY = dlopen(NULL, RTLD_LAZY);
			if (dlh == NULL)
				goto noexist;
		}
	}

	snprintf(buf, sizeof(buf), "%s_genl_util", str);

	f = dlsym(dlh, buf);
	if (f == NULL)
		goto noexist;
reg:
	f->next = genl_list;
	genl_list = f;
	return f;

noexist:
	f = calloc(1, sizeof(*f));
	if (f) {
		strncpy(f->name, str, 15);
		f->parse_genlopt = parse_nofopt;
		f->print_genlopt = print_nofopt;
		goto reg;
	}
	return f;
}

static void usage(void) __attribute__((noreturn));

static void usage(void)
{
	fprintf(stderr,
		"Usage: genl [ OPTIONS ] OBJECT [help] }\n"
		"where  OBJECT := { ctrl etc }\n"
		"       OPTIONS := { -s[tatistics] | -d[etails] | -r[aw] | -V[ersion] | -h[elp] }\n");
	exit(-1);
}

int main(int argc, char **argv)
{
	while (argc > 1) {
		if (argv[1][0] != '-')
			break;
		if (matches(argv[1], "-stats") == 0 ||
		    matches(argv[1], "-statistics") == 0) {
			++show_stats;
		} else if (matches(argv[1], "-details") == 0) {
			++show_details;
		} else if (matches(argv[1], "-raw") == 0) {
			++show_raw;
		} else if (matches(argv[1], "-Version") == 0) {
			printf("genl utility, iproute2-%s\n", version);
			exit(0);
		} else if (matches(argv[1], "-help") == 0) {
			usage();
		} else {
			fprintf(stderr,
				"Option \"%s\" is unknown, try \"genl -help\".\n",
				argv[1]);
			exit(-1);
		}
		argc--;	argv++;
	}

	if (argc > 1) {
		struct genl_util *a;
		int ret;

		a = get_genl_kind(argv[1]);
		if (!a) {
			fprintf(stderr, "bad genl %s\n", argv[1]);
			exit(-1);
		}

		ret = a->parse_genlopt(a, argc-1, argv+1);
		return ret;
	}

	usage();
}

/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * brmonitor.c		"bridge monitor"
 *
 * Authors:	Stephen Hemminger <shemminger@vyatta.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/if_bridge.h>
#include <linux/neighbour.h>
#include <string.h>

#include "utils.h"
#include "br_common.h"


static void usage(void) __attribute__((noreturn));
static int prefix_banner;

static void usage(void)
{
	fprintf(stderr, "Usage: bridge monitor [file | link | fdb | mdb | vlan | vni | all]\n");
	exit(-1);
}

static int accept_msg(struct rtnl_ctrl_data *ctrl,
		      struct nlmsghdr *n, void *arg)
{
	FILE *fp = arg;

	switch (n->nlmsg_type) {
	case RTM_NEWLINK:
	case RTM_DELLINK:
		return print_linkinfo(n, arg);

	case RTM_NEWNEIGH:
	case RTM_DELNEIGH:
		return print_fdb(n, arg);

	case RTM_NEWMDB:
	case RTM_DELMDB:
		return print_mdb_mon(n, arg);

	case NLMSG_TSTAMP:
		print_nlmsg_timestamp(fp, n);
		return 0;

	case RTM_NEWVLAN:
	case RTM_DELVLAN:
		return print_vlan_rtm(n, arg, true, false);

	case RTM_NEWTUNNEL:
	case RTM_DELTUNNEL:
		return print_vnifilter_rtm(n, arg);

	default:
		return 0;
	}
}

void print_headers(FILE *fp, const char *label)
{
	if (timestamp)
		print_timestamp(fp);

	if (prefix_banner)
		fprintf(fp, "%s", label);
}

int do_monitor(int argc, char **argv)
{
	char *file = NULL;
	unsigned int groups = ~RTMGRP_TC;
	int llink = 0;
	int lneigh = 0;
	int lmdb = 0;
	int lvlan = 0;
	int lvni = 0;

	rtnl_close(&rth);

	while (argc > 0) {
		if (matches(*argv, "file") == 0) {
			NEXT_ARG();
			file = *argv;
		} else if (matches(*argv, "link") == 0) {
			llink = 1;
			groups = 0;
		} else if (matches(*argv, "fdb") == 0) {
			lneigh = 1;
			groups = 0;
		} else if (matches(*argv, "mdb") == 0) {
			lmdb = 1;
			groups = 0;
		} else if (matches(*argv, "vlan") == 0) {
			lvlan = 1;
			groups = 0;
		} else if (strcmp(*argv, "vni") == 0) {
			lvni = 1;
			groups = 0;
		} else if (strcmp(*argv, "all") == 0) {
			groups = ~RTMGRP_TC;
			lvlan = 1;
			lvni = 1;
			prefix_banner = 1;
		} else if (matches(*argv, "help") == 0) {
			usage();
		} else {
			fprintf(stderr, "Argument \"%s\" is unknown, try \"bridge monitor help\".\n", *argv);
			exit(-1);
		}
		argc--;	argv++;
	}

	if (llink)
		groups |= nl_mgrp(RTNLGRP_LINK);

	if (lneigh) {
		groups |= nl_mgrp(RTNLGRP_NEIGH);
	}

	if (lmdb) {
		groups |= nl_mgrp(RTNLGRP_MDB);
	}

	if (file) {
		FILE *fp;
		int err;

		fp = fopen(file, "r");
		if (fp == NULL) {
			perror("Cannot fopen");
			exit(-1);
		}
		err = rtnl_from_file(fp, accept_msg, stdout);
		fclose(fp);
		return err;
	}

	if (rtnl_open(&rth, groups) < 0)
		exit(1);

	if (lvlan && rtnl_add_nl_group(&rth, RTNLGRP_BRVLAN) < 0) {
		fprintf(stderr, "Failed to add bridge vlan group to list\n");
		exit(1);
	}

	if (lvni && rtnl_add_nl_group(&rth, RTNLGRP_TUNNEL) < 0) {
		fprintf(stderr, "Failed to add bridge vni group to list\n");
		exit(1);
	}

	ll_init_map(&rth);

	if (rtnl_listen(&rth, accept_msg, stdout) < 0)
		exit(2);

	return 0;
}

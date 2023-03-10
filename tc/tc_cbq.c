/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * tc_cbq.c		CBQ maintenance routines.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "utils.h"
#include "tc_core.h"
#include "tc_cbq.h"

unsigned int tc_cbq_calc_maxidle(unsigned int bndw, unsigned int rate, unsigned int avpkt,
			     int ewma_log, unsigned int maxburst)
{
	double maxidle;
	double g = 1.0 - 1.0/(1<<ewma_log);
	double xmt = (double)avpkt/bndw;

	maxidle = xmt*(1-g);
	if (bndw != rate && maxburst) {
		double vxmt = (double)avpkt/rate - xmt;

		vxmt *= (pow(g, -(double)maxburst) - 1);
		if (vxmt > maxidle)
			maxidle = vxmt;
	}
	return tc_core_time2tick(maxidle*(1<<ewma_log)*TIME_UNITS_PER_SEC);
}

unsigned int tc_cbq_calc_offtime(unsigned int bndw, unsigned int rate, unsigned int avpkt,
			     int ewma_log, unsigned int minburst)
{
	double g = 1.0 - 1.0/(1<<ewma_log);
	double offtime = (double)avpkt/rate - (double)avpkt/bndw;

	if (minburst == 0)
		return 0;
	if (minburst == 1)
		offtime *= pow(g, -(double)minburst) - 1;
	else
		offtime *= 1 + (pow(g, -(double)(minburst-1)) - 1)/(1-g);
	return tc_core_time2tick(offtime*TIME_UNITS_PER_SEC);
}

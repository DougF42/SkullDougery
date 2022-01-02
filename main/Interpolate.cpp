/*
 * Interpolate.cpp
 *
 *  Created on: Nov 19, 2021
 *      Author: doug
 *
 * Interpolation against a table
 *  This class is used to do linear interpolation against
 *  a table. The table consists of x/y pairs of data,
 *  and must contain at least two entries.  X must be
 *  increasing.
 *  In order to shorten the calculation, we store the x and y
 *  values AND the slope for each x/y pair.
 *
 *  If X is inside the range of the table, then the two
 *  table entries that surround a value of X will be used. In
 *  the case where X matches a table entry, we will use that
 *  entry and the entry previous to the target. One exception
 *  is if X matches the smallest x in the table, then we use
 *  the two lowest entries in the table.
 *
 *  If X is outside the range of X's, we use either the first two
 *  table entries (when X is smaller) or the last two table
 *  entries (when X is larger).
 *
 * ERRORS:
 */
#include <stdio.h>
#include "Interpolate.h"

Interpolate::Interpolate() {
	// Auto-generated constructor stub
	lastTabIdx=-1;
	slope[0]=0.0;
	limitFlag=false;
}

Interpolate::~Interpolate() {
	// Auto-generated destructor stub
}

/**
 * Add another entry to the table.
 *  (NOTE: A minimum of two enteries are REQUIRED!!!!
 *
 * NOTE: Xn MUST be incrementing!!!
 *       Results are weird (not well defined) slope is negative.
 */
void Interpolate::AddToTable(int Xn, unsigned int Yn) {
	if ((lastTabIdx>=0) && (Xn<xTable[lastTabIdx])) {
		// ERROR!!!!
		fprintf(stderr, "ERROR in Interpolate. X value is not incrementing!");
	}

	lastTabIdx++;
	xTable[lastTabIdx]=Xn;
	yTable[lastTabIdx]=Yn;

	if (lastTabIdx!=0)
		slope[lastTabIdx]=((double)(yTable[lastTabIdx]-yTable[lastTabIdx-1]) /
				           (double)(xTable[lastTabIdx]-xTable[lastTabIdx-1]));
}

/**
 * Linear Interpolation based on the table.
 *
 * If limitFlag is true, then we never return a y outside the range of yTable[0],
 *    yTable[lastTabIdx].
 * if limitFlag is false then:
 *    If x is less than the first entry, interpolate based on first two entries
 *    If x is greater than the last entry, interpolate base on last two entries
 *
 */
unsigned Interpolate::interp (int x)
{
	// First, find our range
	int highIdx = 0;
	int lowIdx = 0;
	for (highIdx = 0; highIdx < lastTabIdx; highIdx++ )
	{
		// Check HIGH limit
		if (xTable[highIdx] >= x)
		{
			if (limitFlag)
			{
				return (yTable[highIdx]);
			}
			else
			{
				break;
			}
		}
	}

	if (highIdx==0) {
		if ((limitFlag))
		{
			return (yTable[0]);
		}

		lowIdx = highIdx - 1;
	}

	int res = yTable[lowIdx] + (x - xTable[highIdx - 1]) * slope[highIdx];
	return (res);
}


/**
 * Print the current table to stdout
 */
void Interpolate::dumpTable() {
	int i;
	for (i=0; i<=lastTabIdx; i++) {
		fprintf(stdout,"X = %5d   Y= %5d   SLOPE=%7.5f\n", xTable[i], yTable[i], slope[i]);
	}
}


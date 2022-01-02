/*
 * Interpolate.h
 *
 * Interpolation against a table
 *  This comes in two parts -
 *     (1) Define the table
 *     (2) interpolate against the table.
 *
 *  Created on: Nov 19, 2021
 *      Author: doug
 */

#ifndef INTERPOLATE_H_
#define INTERPOLATE_H_
#include <cstdint>
#ifndef TABLEMAX
#define TABLEMAX 10
#endif
class Interpolate {
public:
	Interpolate();
	inline void setLimitFlag() {limitFlag=true; }
	void AddToTable(int idx, unsigned int value);
	int table[TABLEMAX];
	virtual ~Interpolate();
	unsigned interp(int idx);
	void dumpTable();

private:
	int xTable[TABLEMAX];
	uint32_t yTable[TABLEMAX];
	double slope[TABLEMAX];   // We pre-calculate the slope from n-1 to n to save cycles!
	int lastTabIdx;
	bool limitFlag;   // If true, then never give values outside the min/max Y
};

#endif /* INTERPOLATE_H_ */

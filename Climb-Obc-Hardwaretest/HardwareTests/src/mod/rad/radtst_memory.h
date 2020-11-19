/*
 * radtst_memory.h
 *
 *  Created on: 10.01.2020
 *      Author: Robert
 */

#ifndef MOD_RAD_RADTST_MEMORY_H_
#define MOD_RAD_RADTST_MEMORY_H_

#define RADTST_PRGFLASH2_TARGET_PAGESIZE		1024	// 1k pages
#define RADTST_PRGFLASH2_TARGET_PAGES			32		// not filled yet....

#define RADTST_EXPECTED_PATTERN_CNT				4
extern uint8_t expectedPagePatternsSeq[2 * RADTST_EXPECTED_PATTERN_CNT];
extern uint8_t prgFlash2Target[RADTST_PRGFLASH2_TARGET_PAGES][RADTST_PRGFLASH2_TARGET_PAGESIZE];


#endif /* MOD_RAD_RADTST_MEMORY_H_ */

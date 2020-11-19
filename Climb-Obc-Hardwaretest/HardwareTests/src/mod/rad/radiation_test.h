/*
 * radiation_test.h
 *
 *  Created on: 03.01.2020
 */

#ifndef MOD_RAD_RADIATION_TEST_H_
#define MOD_RAD_RADIATION_TEST_H_


typedef struct radtst_counter_s {				// only add uint32_t values (its printed as uint32_t[] !!!
	uint32_t rtcgprTestCnt;
	uint32_t signatureCheckCnt;
	uint32_t ram2PageReadCnt;
	uint32_t ram2PageWriteCnt;
	uint32_t mramPageReadCnt;
	uint32_t mramPageWriteCnt;
	uint32_t i2cmemPageReadCnt;
	uint32_t i2cmemPageWriteCnt;
	uint32_t flashSektorEraseCnt;
	uint32_t flashPageWriteCnt;
	uint32_t flashReadPageCnt;

	uint32_t expSignatureChanged;				// This should stay on RADTST_FLASHSIG_PARTS (4) - first time read after reset
	uint32_t expRam2BytesChanged;

	uint32_t signatureCheckBlocked;
	uint32_t signatureRebaseBlocked;
	uint32_t rtcgprTestErrors;
	uint32_t signatureErrorCnt;
	uint32_t ram2PageReadError;
	uint32_t ram2PageWriteError;
	uint32_t mramPageReadError;
	uint32_t mramPageWriteError;
	uint32_t i2cmemPageReadError;
	uint32_t i2cmemPageWriteError;
	uint32_t flashSektorEraseError;
	uint32_t flashSektorWriteError;
	uint32_t flashPageWriteError;
	uint32_t flashPageReadError;
} radtst_counter_t;

typedef struct radtst_readcheckenabled_s {
	unsigned int 	prgFlash 	:1;
	unsigned int  	rtcGpr		:1;
	unsigned int  	ram2		:1;
	unsigned int  	mram		:1;
	unsigned int  	i2cmem		:1;
	unsigned int  	flash12     :1;
	unsigned int  	:1;
	unsigned int  	:1;
} radtst_readcheckenabled_t;

typedef enum radtst_sources_e {
	RADTST_SRC_PRGFLASH2,				// The upper half of program flash (0x00040000 - 0x0007FFFF)
	RADTST_SRC_RTCGPR,					// 20 bytes (5Words) general purpose registers in RTC (battery buffered)
	RADTST_SRC_RAM2,					// The 'upper' (unused) RAM Bank (0x2007C000 - 0x20084000(?))
	RADTST_SRC_MRAM,
	RADTST_SRC_FRAM,
	RADTST_SRC_EE1,
	RADTST_SRC_EE2,
	RADTST_SRC_EE3,
	RADTST_SRC_FLASH1,


	RADTST_SRC_UNKNOWN = 128
} radtst_sources_t;

// Module Main API
void RadTstInit(void);
void RadTstMain(void);

void RadTstLogReadError2(radtst_sources_t source, uint8_t pageNr, uint8_t expByte, uint8_t *actPtr, uint16_t len);

extern uint8_t 	*expectedPagePatternsPtr; 	// points fillpattern bytes[0..3]
extern radtst_readcheckenabled_t	readEnabled;
extern radtst_counter_t	radtstCounter;

#endif /* MOD_RAD_RADIATION_TEST_H_ */

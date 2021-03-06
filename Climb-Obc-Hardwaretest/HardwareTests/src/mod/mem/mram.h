/*
 * mram.h
 *
 *  Created on: 27.12.2019
 */

#ifndef MOD_MEM_MRAM_H_
#define MOD_MEM_MRAM_H_

#define MRAM_MAX_READ_SIZE	1024
#define MRAM_MAX_WRITE_SIZE	1024

typedef enum mram_res_e
{
	MRAM_RES_SUCCESS = 0,
	MRAM_RES_BUSY,
	MRAM_RES_DATA_PTR_INVALID,
	MRAM_RES_RX_LEN_OVERFLOW,
	MRAM_RES_INVALID_ADR,
	MRAM_RES_JOB_ADD_ERROR
} mram_res_t;

void MramInit();						    // Module Init called once prior mainloop
void MramMain();							// Module routine participating each mainloop.

void ReadMramAsync(uint32_t adr,  uint8_t *rx_data,  uint32_t len, void (*finishedHandler)(mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len));
void WriteMramAsync(uint32_t adr, uint8_t *data, uint32_t len,  void (*finishedHandler)(mram_res_t result, uint32_t adr, uint8_t *data, uint32_t len));


#endif /* MOD_MEM_MRAM_H_ */

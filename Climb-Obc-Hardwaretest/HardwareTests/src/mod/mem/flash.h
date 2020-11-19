/*
 * flash.h
 *
 *  Created on: 20.12.2019
 *
 */

#ifndef MOD_MEM_FLASH_H_
#define MOD_MEM_FLASH_H_
/* Whole flash */
#define FLASH_SIZE 				134217728 	/* Bytes (2 * 2^26) 64 MByte max adr: 0x03FF FFFF‬ */
#define FLASH_PAGE_SIZE	 ((uint32_t) 512)	/* Bytes */
#define FLASH_SECTOR_SIZE	  	   262144	/* Bytes (2^18)  256kByte  */
#define FLASH_PAGE_NUMBER   	   262144
#define FLASH_SECTOR_NUMBER    		  512

/* Single DIE only */
#define FLASH_DIE_SIZE		   	 67108864	/* Bytes (2^25) 32 MByte  sdr 0x000 0000 .. 0x01FF FFFF -> dye 0, 0x020 0000 .. 0x3FF FFFF -> dye 1*/
#define FLASH_DIE_SECTOR_NUMBER  	  256
#define FLASH_DIE_PAGE_NUMBER  	   131072

#define FLASH_MAX_READ_SIZE			512		// bytes
#define FLASH_MAX_WRITE_SIZE		512		// bytes


typedef enum flash_res_e
{
	FLASH_RES_SUCCESS = 0,
	FLASH_RES_BUSY,
	FLASH_RES_DATA_PTR_INVALID,
	FLASH_RES_TX_OVERFLOW,
	FLASH_RES_INVALID_ADR,
	FLASH_RES_JOB_ADD_ERROR,
	FLASH_RES_WIPCHECK_ERROR,
	FLASH_RES_TX_ERROR,
	FLASH_RES_TX_WRITE_TOO_LONG,
	FLASH_RES_WRONG_FLASHNR,
	FLASH_RES_RX_LEN_OVERFLOW,
	FLASH_RES_TIMEOUT
} flash_res_t;

typedef enum flash_nr_e
{
	FLASH_NR1 = 1,
	FLASH_NR2 = 2
} flash_nr_t;


void FlashInit();						    // Module Init called once prior mainloop
void FlashMain();							// Module routine participating each mainloop.

void ReadFlashAsync(flash_nr_t flashNr, uint32_t adr,  uint8_t *rx_data,  uint32_t len, void (*finishedHandler)(flash_res_t result, uint8_t flashNr, uint32_t adr, uint8_t *data, uint32_t len));
void WriteFlashAsync(flash_nr_t flashNr, uint32_t adr, uint8_t *data, uint32_t len,  void (*finishedHandler)(flash_res_t result, uint8_t flashNr, uint32_t adr, uint32_t len));
void EraseFlashAsync(flash_nr_t flashNr, uint32_t adr, void (*finishedHandler)(flash_res_t result, uint8_t flashNr, uint32_t adr, uint32_t len));

#endif /* MOD_MEM_FLASH_H_ */

/*
 * flash.c
 *
 *  Created on: 20.12.2019
 *
 */
/* 2x S25FL512
 * 1 Sector = 256kByte
 * Sector erase time = 520ms
 * Page Programming Time = 340us
 * Page size = 512Byte
 * One time programmable memory 1024Byte
 * Program and erase suspend possible
 */
//	/* Achtung -> SSP Frequenz für read maximal 50MHz! */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <chip.h>

// #include "../../globals.h"		// For ClimbLedToggle(0);
#include "../../layer1/SSP/obc_ssp.h"

#include "flash.h"
#include "../cli/cli.h"
#include "../tim/timer.h"

typedef enum flash_status_e {
	FLASH_STAT_NOT_INITIALIZED,
	FLASH_STAT_IDLE,
	FLASH_STAT_RX_CHECKWIP,
	FLASH_STAT_RX_INPROGRESS,
	FLASH_STAT_TX_CHECKWIP,
	FLASH_STAT_TX_SETWRITEBIT,
	FLASH_STAT_TX_ERASE_TRANSFER_INPROGRESS,
	FLASH_STAT_WRITE_ERASE_INPROGRESS,
	FLASH_STAT_WRITE_ERASE_INPROGRESS_DELAY,
	FLASH_STAT_CLEAR_ERRORS,
	FLASH_STAT_ERASE_CHECKWIP,
	FLASH_STAT_ERASE_SETWRITEBIT,
	FLASH_STAT_ERROR							// TODO: what specific errors are there and what too do now ???? -> reinit SSP ???
} flash_status;

typedef struct flash_worker_s
{
	flash_status FlashStatus;
	uint8_t tx[5];
	uint8_t rx[1];
	bool (*ChipSelect)(bool select);
	uint8_t *job_status;
	uint8_t *data;
	uint32_t len;
	uint32_t adr;
	uint8_t  busNr;
	int     CheckTxCounter;
	int		DelayCounter;
	int     DelayValue;
	uint32_t   BusyTmoTicks;
	void (*RxCallback)(uint8_t rxtxResult, uint8_t flashNr, uint32_t adr, uint8_t *data, uint32_t len);
	void (*TxEraseCallback)(uint8_t rxtxResult, uint8_t flashNr, uint32_t adr, uint32_t len);
} flash_worker_t;

#define FLASH2_CS1_PIN 12
#define FLASH2_CS1_PORT 2
#define FLASH2_CS2_PIN 11
#define FLASH2_CS2_PORT 2

#define FLASH1_CS1_PIN 28
#define FLASH1_CS1_PORT 4
#define FLASH1_CS2_PIN 2
#define FLASH1_CS2_PORT 2

#define FLASH_RX_TIMEOUTTICKS_PERBYTE		1000
#define FLASH_TX_TIMEOUTTICKS				1000


// prototypes
bool flash_init(ssp_busnr_t busNr, bool (*ChipSelect)(bool select) );
void FlashMainFor(uint8_t flashNr);

void ReadFlashCmd(int argc, char *argv[]);
void ReadFlashFinished(flash_res_t rxtxResult, flash_nr_t flashNr, uint32_t adr, uint8_t *data, uint32_t len);

void WriteFlashCmd(int argc, char *argv[]);
void WriteFlashFinished(uint8_t rxtxResult, flash_nr_t flashNr, uint32_t adr, uint32_t len);

void EraseFlashCmd(int argc, char *argv[]);
void EraseFlashFinished(flash_res_t rxtxResult, flash_nr_t flashNr, uint32_t adr, uint32_t len);

void DumpSspJobCmd(int argc, char *argv[]);

// local variables
volatile bool flash1_busy;
volatile bool flash2_busy;
flash_worker_t flashWorker[2];
uint8_t FlashWriteData[FLASH_MAX_WRITE_SIZE+5];
uint8_t FlashReadData[FLASH_MAX_READ_SIZE];

//
// Be careful here! This Callbacks are called from IRQ !!!
// Do not do any complicated logic here!!!
// Transfer all things to be done to next mainloop....
bool FlashChipSelect_1_CS1( bool select) {
	Chip_GPIO_SetPinState(LPC_GPIO, FLASH1_CS1_PORT, FLASH1_CS1_PIN, !select);
	if (!select) {
		flash1_busy = false;
	}
	return true;
}

bool FlashChipSelect_1_CS2( bool select) {
	Chip_GPIO_SetPinState(LPC_GPIO, FLASH1_CS2_PORT, FLASH1_CS2_PIN, !select);
	if (!select) {
		flash1_busy = false;
	}
	return true;
}

bool FlashChipSelect_2_CS1( bool select) {
	Chip_GPIO_SetPinState(LPC_GPIO, FLASH2_CS1_PORT, FLASH2_CS1_PIN, !select);
	if (!select) {
		flash2_busy = false;
	}
	return true;
}

bool FlashChipSelect_2_CS2( bool select) {
	Chip_GPIO_SetPinState(LPC_GPIO, FLASH2_CS2_PORT, FLASH2_CS2_PIN, !select);
	if (!select) {
		flash2_busy = false;
	}
	return true;
}

void FlashInit() {
	flashWorker[0].FlashStatus = FLASH_STAT_NOT_INITIALIZED;
	flashWorker[1].FlashStatus = FLASH_STAT_NOT_INITIALIZED;

	ssp01_init();										// TODO: shouldn't each module init be called from main !?.....

	/* --- Chip selects IOs --- */
	Chip_IOCON_PinMuxSet(LPC_IOCON, FLASH2_CS1_PORT, FLASH2_CS1_PIN, IOCON_FUNC0 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, FLASH2_CS1_PORT, FLASH2_CS1_PIN);
	Chip_GPIO_SetPinDIROutput(LPC_GPIO, FLASH2_CS1_PORT, FLASH2_CS1_PIN);
	Chip_GPIO_SetPinState(LPC_GPIO, FLASH2_CS1_PORT, FLASH2_CS1_PIN, true);

	Chip_IOCON_PinMuxSet(LPC_IOCON, FLASH2_CS2_PORT, FLASH2_CS2_PIN, IOCON_FUNC0 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, FLASH2_CS2_PORT, FLASH2_CS2_PIN);
	Chip_GPIO_SetPinDIROutput(LPC_GPIO, FLASH2_CS2_PORT, FLASH2_CS2_PIN);
	Chip_GPIO_SetPinState(LPC_GPIO, FLASH2_CS2_PORT, FLASH2_CS2_PIN, true);

	Chip_IOCON_PinMuxSet(LPC_IOCON, FLASH1_CS1_PORT, FLASH1_CS1_PIN, IOCON_FUNC0 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, FLASH1_CS1_PORT, FLASH1_CS1_PIN);
	Chip_GPIO_SetPinDIROutput(LPC_GPIO, FLASH1_CS1_PORT, FLASH1_CS1_PIN);
	Chip_GPIO_SetPinState(LPC_GPIO, FLASH1_CS1_PORT, FLASH1_CS1_PIN, true);

	Chip_IOCON_PinMuxSet(LPC_IOCON, FLASH1_CS2_PORT, FLASH1_CS2_PIN, IOCON_FUNC0 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, FLASH1_CS2_PORT, FLASH1_CS2_PIN);
	Chip_GPIO_SetPinDIROutput(LPC_GPIO, FLASH1_CS2_PORT, FLASH1_CS2_PIN);
	Chip_GPIO_SetPinState(LPC_GPIO, FLASH1_CS2_PORT, FLASH1_CS2_PIN, true);

	// Each Flash has 2 Dyes with separate CS line
	if (flash_init(SSP_BUS1, FlashChipSelect_1_CS1 )) {
		if (flash_init(SSP_BUS1, FlashChipSelect_1_CS2 )) {
			// init ok
			flashWorker[0].FlashStatus = FLASH_STAT_IDLE;
		} else {
			printf("Initialization Error Flash1 CS2.\n");
		}
	} else {
		printf("Initialization Error Flash1 CS1.\n");
	}


	if (flash_init(SSP_BUS0, FlashChipSelect_2_CS1 )) {
		if (flash_init(SSP_BUS0, FlashChipSelect_2_CS2 )) {
			// init ok
			flashWorker[1].FlashStatus = FLASH_STAT_IDLE;
		} else {
			printf("Initialization Error Flash2 CS2.\n");
		}
	} else {
		printf("Initialization Error Flash2 CS1.\n");
	}

	RegisterCommand("fRead", ReadFlashCmd);
	RegisterCommand("fWrite", WriteFlashCmd);
	RegisterCommand("fErase", EraseFlashCmd);
	RegisterCommand("dumpSsp",DumpSspJobCmd);
}

void FlashMain(void) {
	FlashMainFor(FLASH_NR1);
	FlashMainFor(FLASH_NR2);
}

bool flash_init(ssp_busnr_t busNr, bool (*ChipSelect)(bool select) )
{
	/* Init flash n and read ID register
	 * Parameters: none
	 * Return value: 0 in case of success, != 0 in case of error
	 */
	uint8_t tx[1];
	uint8_t rx[1];
	uint8_t *job_status = NULL;
	volatile uint32_t helper;
	/* Read flash ID register */
	tx[0] = 0x9F; /* 0x9F */
	rx[0] = 0x00;

	if (ssp_add_job2(busNr , tx, 1, rx, 1, &job_status, ChipSelect))
	{
		/* Error while adding job */
		return false;
	}
	helper = 0;
	while ((*job_status != SSP_JOB_STATE_DONE) && (helper < 1000000))
	{
		/* Wait for job to finish */
		helper++;
	}

	if (rx[0] != 0x01)
	{
		/* Error - Flash could not be accessed */
		return false;
	}
	return true;
}

void DumpSspJobCmd(int argc, char *argv[]) {
	int bus = 0;
	if (argc > 0) {
		bus = atoi(argv[0]);
	}
	DumpSspJobs(bus);
}

void ReadFlashCmd(int argc, char *argv[]) {
	if (argc != 3) {
		printf("uasge: cmd <mem> <adr> <len> where mem i one of 1, 2\n" );
		return;
	}

	// CLI params to binary params
	uint8_t flashNr = atoi(argv[0]);
	uint32_t adr = atoi(argv[1]);
	uint32_t len = atoi(argv[2]);
	if (len > FLASH_MAX_READ_SIZE) {
		len = FLASH_MAX_READ_SIZE;
	}

	// Binary Command
	ReadFlashAsync(flashNr, adr, FlashReadData, len, ReadFlashFinished);
}

void ReadFlashFinished(flash_res_t result,  flash_nr_t flashNr, uint32_t adr, uint8_t *data, uint32_t len) {
	if (result == FLASH_RES_SUCCESS) {
		printf("Flash %d read at %04X:\n", flashNr, adr);
		for (int i=0; i<len; i++ ) {
			printf("%02X ", ((uint8_t*)data)[i]);
			if ((i+1)%8 == 0) {
				printf("   ");
			}
		}
		printf("\n");
	} else {
		printf("Error while reading from FlashNr %d: %d\n", flashNr, result);
	}
}

void WriteFlashCmd(int argc, char *argv[]) {
	if (argc != 4) {
		printf("uasge: cmd <flsh> <adr> <databyte> <len> where flsh is 1 or 2\n" );
		return;
	}

	// CLI params to binary params
	uint8_t flashNr = atoi(argv[0]);
	uint32_t adr = atoi(argv[1]);
	uint8_t byte = atoi(argv[2]);
	uint32_t len = atoi(argv[3]);
	if (len > FLASH_MAX_WRITE_SIZE) {
		len = FLASH_MAX_WRITE_SIZE;
	}

	for (int i=0;i<len;i++){
		FlashWriteData[i+5] = byte;		// keep first 5 bytes free for flash Write header.
	}

	// Binary Command
	WriteFlashAsync(flashNr, adr, FlashWriteData, len,  WriteFlashFinished);
}

void WriteFlashFinished(uint8_t rxtxResult, flash_nr_t flashNr, uint32_t adr, uint32_t len){
	if (rxtxResult == FLASH_RES_SUCCESS) {
		printf("%d bytes written to flash %d at %02X\n", len, flashNr, adr);
	} else {
		printf("Error while attempting to write to FlashNr %d: %d\n", flashNr, rxtxResult);
	}
}

void EraseFlashCmd(int argc, char *argv[]){
	if (argc != 2) {
			printf("uasge: cmd <flsh> <adr> where flsh is 1 or 2\n" );
			return;
		}

	// CLI params to binary params
	uint8_t flashNr = atoi(argv[0]);
	uint32_t adr = atoi(argv[1]);

	//ClimbLedToggle(0);
	// Binary Command
	EraseFlashAsync(flashNr, adr, EraseFlashFinished);
}

void EraseFlashFinished(flash_res_t rxtxResult, flash_nr_t flashNr, uint32_t adr, uint32_t len){
	//ClimbLedToggle(0);
	if (rxtxResult == FLASH_RES_SUCCESS) {
		//printf("flash %d: sector erased at %04X  TryCounter: %d \n", flashNr, adr, len);
		printf("flash %d: sector erased at %04X\n", flashNr, adr);
	} else {
		printf("Error while attempting to erase FlashNr %d: %d\n", flashNr, rxtxResult);
	}
}


void ReadFlashAsync (flash_nr_t flashNr, uint32_t adr, uint8_t *rx_data, uint32_t len, void (*finishedHandler)(flash_res_t rxtxResult, uint8_t flashNr, uint32_t adr, uint8_t *data, uint32_t len)) {
	volatile bool *busyFlag;
	uint8_t busNr;
	flash_worker_t *worker;

	if (flashNr == 1) {
		busyFlag = &flash1_busy;
		busNr = 1;
		worker = &flashWorker[0];
	} else if (flashNr == 2) {
		busyFlag = &flash2_busy;
		busNr = 0;
		worker = &flashWorker[1];
	} else {
		finishedHandler(FLASH_RES_WRONG_FLASHNR, flashNr, adr, 0 , len);
		return;
	}

	if (worker->FlashStatus != FLASH_STAT_IDLE) {
//	if (! *initializedFlag)
//	{
		// Flash was not initialized correctly		// TODO this also checks for all other busy states now  -> ret val ok????
		finishedHandler(FLASH_RES_BUSY, flashNr, worker->FlashStatus, 0, len);
		return;
	}

	if (rx_data == NULL)
	{
		finishedHandler(FLASH_RES_DATA_PTR_INVALID, flashNr, adr, 0, len);
		return;
	}

	if (len > FLASH_DIE_SIZE)
	{
		finishedHandler(FLASH_RES_RX_LEN_OVERFLOW, flashNr, adr, 0, len);
		return;
	}

	if (adr < FLASH_DIE_SIZE)
	{
		if (flashNr == 2) {
			//worker->flash_dev = SSPx_DEV_FLASH2_1;
			worker->ChipSelect = FlashChipSelect_2_CS1;
		} else {
			//worker->flash_dev = SSPx_DEV_FLASH1_1;
			worker->ChipSelect = FlashChipSelect_1_CS1;
		}
	}
	else if (adr < FLASH_SIZE)
	{
		if (flashNr == 2) {
			//worker->flash_dev = SSPx_DEV_FLASH2_2;
			worker->ChipSelect = FlashChipSelect_2_CS2;
		} else {
			//worker->flash_dev = SSPx_DEV_FLASH1_2;
			worker->ChipSelect = FlashChipSelect_1_CS2;
		}
		adr = adr - FLASH_DIE_SIZE;
	}
	else
	{
		/* Sector address overrun  */
		finishedHandler(FLASH_RES_INVALID_ADR, flashNr, adr, 0, len);
		return;
	}

	/*--- Check WIP-Bit (Wait for previous write to complete) --- */
	worker->tx[0] = 0x05; /* 0x05 */
	worker->rx[0] = 0x00;

	*busyFlag = true;
	if (ssp_add_job2(busNr,  worker->tx, 1, worker->rx, 1, NULL, worker->ChipSelect)) {
		/* Error while adding job */
		finishedHandler(FLASH_RES_JOB_ADD_ERROR, flashNr, adr, 0, len);
		return;
	}
	worker->data = rx_data;
	worker->len = len;
	worker->adr = adr;
	worker->busNr = busNr;
	worker->RxCallback = finishedHandler;
	worker->TxEraseCallback = NULL;

	// next step(s) is/are done in mainloop
	worker->BusyTmoTicks = FLASH_TX_TIMEOUTTICKS;
	worker->FlashStatus = FLASH_STAT_RX_CHECKWIP;
	return;
}

void WriteFlashAsync(flash_nr_t flashNr, uint32_t adr, uint8_t *data, uint32_t len,  void (*finishedHandler)(uint8_t rxtxResult, uint8_t flashNr, uint32_t adr, uint32_t len)) {
	volatile bool *busyFlag;
	uint8_t busNr;
	flash_worker_t *worker;

	if (flashNr == 1) {
		//initializedFlag = &flash1_initialized;
		busyFlag = &flash1_busy;
		busNr = 1;
		worker = &flashWorker[0];
	} else if (flashNr == 2) {
		//initializedFlag = &flash2_initialized;
		busyFlag = &flash2_busy;
		busNr = 0;
		worker = &flashWorker[1];
	} else {
		finishedHandler(FLASH_RES_WRONG_FLASHNR, flashNr, adr, len);
		return;
	}

	if (worker->FlashStatus != FLASH_STAT_IDLE) {
		// TODO this also checks for all other busy states now  -> make own values for 'unitialized', 'Error', .... !?
		finishedHandler(FLASH_RES_BUSY, flashNr, worker->FlashStatus, len);
		return;
	}

	if (data == NULL)
	{
		finishedHandler(FLASH_RES_DATA_PTR_INVALID, flashNr, adr, len);
		return;
	}

	if (len > FLASH_MAX_WRITE_SIZE)
	{
		finishedHandler(FLASH_RES_TX_OVERFLOW, flashNr, adr, len); // return FLASH_RET_TX_OVERFLOW;
		return;
	}

	if (adr < FLASH_DIE_SIZE)
	{
		if (flashNr == 2) {
			//worker->flash_dev = SSPx_DEV_FLASH2_1;
			worker->ChipSelect = FlashChipSelect_2_CS1;
		} else {
			//worker->flash_dev = SSPx_DEV_FLASH1_1;
			worker->ChipSelect = FlashChipSelect_1_CS1;
		}
	}
	else if (adr < FLASH_SIZE)
	{
		if (flashNr == 2) {
			//worker->flash_dev = SSPx_DEV_FLASH2_2;
			worker->ChipSelect = FlashChipSelect_2_CS2;
		} else {
			//worker->flash_dev = SSPx_DEV_FLASH1_2;
			worker->ChipSelect = FlashChipSelect_1_CS2;
		}
		adr = adr - FLASH_DIE_SIZE;
	}
	else
	{
		/* Sector address overrun  */
		finishedHandler(FLASH_RES_INVALID_ADR, flashNr, adr, len); //return FLASH_RET_INVALID_ADR;
		return;
	}

	/*--- Check WIP-Bit (Wait for previous write to complete) --- */
	worker->tx[0] = 0x05; /* 0x05 */
	worker->rx[0] = 0x00;

	*busyFlag = true;
	if (ssp_add_job2(busNr,  worker->tx, 1, worker->rx, 1, NULL, worker->ChipSelect)) {
		/* Error while adding job */
		finishedHandler(FLASH_RES_JOB_ADD_ERROR, flashNr, adr, len); //return FLASH_RET_JOB_ADD_ERROR;
	}
	worker->data = data;
	worker->len = len;
	worker->adr = adr;
	worker->busNr = busNr;
	worker->TxEraseCallback = finishedHandler;
	worker->RxCallback = NULL;
	worker->FlashStatus = FLASH_STAT_TX_CHECKWIP;
	worker->DelayValue = 1;			// Write in Progress check will be executed each mainloop for 25 tries.
}

void EraseFlashAsync(flash_nr_t flashNr, uint32_t adr, void (*finishedHandler)(flash_res_t rxtxResult, uint8_t flashNr, uint32_t adr, uint32_t len)){
	volatile bool *busyFlag;
	uint8_t busNr;
	flash_worker_t *worker;

	if (flashNr == 1) {
		//initializedFlag = &flash1_initialized;
		busyFlag = &flash1_busy;
		busNr = 1;
		worker = &flashWorker[0];
	} else if (flashNr == 2) {
		//initializedFlag = &flash2_initialized;
		busyFlag = &flash2_busy;
		busNr = 0;
		worker = &flashWorker[1];
	} else {
		finishedHandler(FLASH_RES_WRONG_FLASHNR, flashNr, adr, 0);
		return;
	}

	if (worker->FlashStatus != FLASH_STAT_IDLE) {
		// TODO this also checks for all other busy states now  -> make own values for 'unitialized', 'Error', .... !?
		finishedHandler(FLASH_RES_BUSY, flashNr, worker->FlashStatus, 0);
		return;
	}

	if (adr < FLASH_DIE_SIZE)
	{
		if (flashNr == 2) {
			//worker->flash_dev = SSPx_DEV_FLASH2_1;
			worker->ChipSelect = FlashChipSelect_2_CS1;
		} else {
			//worker->flash_dev = SSPx_DEV_FLASH1_1;
			worker->ChipSelect = FlashChipSelect_1_CS1;
		}
	}
	else if (adr < FLASH_SIZE)
	{
		if (flashNr == 2) {
			//worker->flash_dev = SSPx_DEV_FLASH2_2;
			worker->ChipSelect = FlashChipSelect_2_CS2;
		} else {
			//worker->flash_dev = SSPx_DEV_FLASH1_2;
			worker->ChipSelect = FlashChipSelect_1_CS2;
		}
		adr = adr - FLASH_DIE_SIZE;
	}
	else {
		/* Sector address overrun  */
		finishedHandler(FLASH_RES_INVALID_ADR, flashNr, adr, 0); //return FLASH_RET_INVALID_ADR;
		return;
	}

	/*--- Check WIP-Bit (Wait for previous write to complete) --- */
	worker->tx[0] = 0x05; /* 0x05 */
	worker->rx[0] = 0x00;

	*busyFlag = true;
	if (ssp_add_job2(busNr,  worker->tx, 1, worker->rx, 1, NULL, worker->ChipSelect)) {
		/* Error while adding job */
		finishedHandler(FLASH_RES_JOB_ADD_ERROR, flashNr, adr, 0); //return FLASH_RET_JOB_ADD_ERROR;
	}

	worker->adr = adr;
	worker->busNr = busNr;
	worker->len = 0;
	worker->DelayValue = 20000;		// Write in Progress check will be executed every 20000th mainloop for 25 tries.
	worker->TxEraseCallback = finishedHandler;
	worker->RxCallback = NULL;
	worker->FlashStatus = FLASH_STAT_ERASE_CHECKWIP;
}

void FlashMainFor(flash_nr_t flashNr) {
	volatile bool *busyFlag;
	flash_worker_t *worker;

	if (flashNr == 1) {
		busyFlag = &flash1_busy;
		worker = &flashWorker[0];
	} else if (flashNr == 2) {
		busyFlag = &flash2_busy;
		worker = &flashWorker[1];
	} else {
		return;
	}

	if (*busyFlag) {
		// TODO: make TMO check(s) here !!!
		if (worker->BusyTmoTicks > 0) {
			worker->BusyTmoTicks--;
			if (worker->BusyTmoTicks == 0){
				// Timeout !!! What to do next !?
				*busyFlag = false;							//TODO: is this wise here to fall back to idle !?=
				flash_res_t failedStatus = worker->FlashStatus;
				worker->FlashStatus = FLASH_STAT_ERROR;
				if (worker->RxCallback != NULL) {
					worker->RxCallback(FLASH_RES_TIMEOUT, flashNr, failedStatus, NULL, 0);
				}
				if (worker->TxEraseCallback != NULL) {
					worker->TxEraseCallback(FLASH_RES_TIMEOUT, flashNr, failedStatus, 0);
				}
				FlashInit();			// Maybe also not good -> also resets mram !?
			}
		}
	} else {
		worker->BusyTmoTicks = 0;			// Next time set it again, if needed.
		// pending job was finished
		switch (worker->FlashStatus) {
		case FLASH_STAT_RX_CHECKWIP: {
			// Write in Progress read job finished
			if (worker->rx[0] & 0x01) {
				// TODO Eigentlich sollte das hier nie passieren (Weil wir dieses Write busy beim Schreiben abwarten!),
				// aber wir könnten hier auch doch ein Zeit warten und das WIP neu lesen. ...
				worker->FlashStatus = FLASH_STAT_ERROR;
			}
			// next job is to ...
			/* Read Bytes */
			worker->tx[0] = 0x13; 	/* CMD fast read */
			worker->tx[1] = (worker->adr >> 24);
			worker->tx[2] = ((worker->adr & 0x00ff0000) >> 16);
			worker->tx[3] = ((worker->adr & 0x0000ff00) >> 8);
			worker->tx[4] = (worker->adr & 0x000000ff);

			*busyFlag = true;
			if (ssp_add_job2(worker->busNr, worker->tx, 5, worker->data, worker->len, &worker->job_status, worker->ChipSelect))
			{
				/* Error while adding job */
				worker->FlashStatus = FLASH_STAT_ERROR;
				worker->RxCallback(FLASH_RES_JOB_ADD_ERROR, flashNr,worker->adr, worker->data, worker->len);
				break;
			}
			worker->BusyTmoTicks = worker->len * FLASH_RX_TIMEOUTTICKS_PERBYTE;
			worker->FlashStatus = FLASH_STAT_RX_INPROGRESS;
			break;

		}
		case FLASH_STAT_RX_INPROGRESS: {
			// Read job is finished. Make Callback
			worker->FlashStatus = FLASH_STAT_IDLE;
			worker->RxCallback(FLASH_RES_SUCCESS, flashNr,worker->adr, worker->data, worker->len);
			break;
		}

		case FLASH_STAT_TX_CHECKWIP: {
			// Write in Progress read job finished
			if (worker->rx[0] & 0x01) {
				// WIP flag is still active !? -> error
				worker->FlashStatus = FLASH_STAT_ERROR;
				worker->TxEraseCallback(FLASH_RES_WIPCHECK_ERROR,flashNr, worker->adr, worker->len);
				break;
			}

			/*--- Write Enable (WREN) --- */
			/* Set WREN bit to initiate write process */
			worker->tx[0] = 0x06; /* 0x06 WREN */

			*busyFlag = true;
			if (ssp_add_job2(worker->busNr,  worker->tx, 1, NULL, 0, NULL,  worker->ChipSelect))
			{
				worker->FlashStatus = FLASH_STAT_ERROR;
				worker->TxEraseCallback(FLASH_RES_JOB_ADD_ERROR,flashNr, worker->adr, worker->len);
				break;
			}
			worker->BusyTmoTicks = FLASH_TX_TIMEOUTTICKS;
			worker->FlashStatus = FLASH_STAT_TX_SETWRITEBIT;
			break;
		}

		case FLASH_STAT_TX_SETWRITEBIT: {
			/* write bit job finished */
			/*--- Write - Page Program --- */
			worker->data[0] = 0x12; /* 0x12 page program 4 byte address */
			worker->data[1] = (worker->adr >> 24);
			worker->data[2] = ((worker->adr & 0x00ff0000) >> 16);
			worker->data[3] = ((worker->adr & 0x0000ff00) >> 8);
			worker->data[4] = (worker->adr & 0x000000ff);

			*busyFlag = true;
			if (ssp_add_job2(worker->busNr, worker->data, (5 + worker->len), NULL, 0, &worker->job_status,  worker->ChipSelect))
			{
				/* Error while adding job */
				worker->FlashStatus = FLASH_STAT_ERROR;
				worker->TxEraseCallback(FLASH_RES_JOB_ADD_ERROR,flashNr, worker->adr, worker->len);
				break;
			}
			worker->BusyTmoTicks = FLASH_TX_TIMEOUTTICKS * (5 + worker->len) ;
			worker->FlashStatus = FLASH_STAT_TX_ERASE_TRANSFER_INPROGRESS;
			break;
		}

		case FLASH_STAT_TX_ERASE_TRANSFER_INPROGRESS: {
			/* tx/erase data transfer job finished */
			/*--- Check WIP-Bit and Error bits --- */
			worker->tx[0] = 0x05; /* 0x05 */
			worker->CheckTxCounter = 25;				// We now wait 25 * 4 ms to get a cleared WIP bit and check for errors !?
			*busyFlag = true;
			if (ssp_add_job2(worker->busNr,  worker->tx, 1, worker->rx, 1, NULL,  worker->ChipSelect)) {
				/* Error while adding job */
				worker->FlashStatus = FLASH_STAT_ERROR;
				worker->TxEraseCallback(FLASH_RES_JOB_ADD_ERROR,flashNr, worker->adr, worker->len);
				break;
			}
			worker->BusyTmoTicks = FLASH_TX_TIMEOUTTICKS * 100;
			worker->FlashStatus = FLASH_STAT_WRITE_ERASE_INPROGRESS;
			break;
		}


		case FLASH_STAT_WRITE_ERASE_INPROGRESS: {
			/* WIP & Error bits read job finished */
			if (worker->rx[0] & 0x01) {
				// Write process still ongoing
				// Delay for ??some mainloops?? and repeating WIP job
				//worker->DelayCounter = 1;		// at this moment with i=1 we count up to 25 mainloops. We get a Write finished after 3 mainloops each
												// one delaying the next WIP read job (takes aprx. 34 us) for aprx. 12 us only. So idle state is reached after
												// aprx. 130/140 us  ( 3 * ( 12 + 34 )! Increasing this value here would slow down polling but increase the
												// timeout we wait here ( 25*delay ).

				// For erase we use:
				//worker->DelayCounter = 20000;	 // TODO: make real timing here. At this moment with 20.000 mainloops for one WIP polling delay (-> aprx. 90ms)
												 //       we get the sector erase finished after 6..7 tries. -> apx. 600 ms !!

				worker->DelayCounter = worker->DelayValue;		// This depends on write/erase.
				worker->FlashStatus = FLASH_STAT_WRITE_ERASE_INPROGRESS_DELAY;
			} else {
				// Write/erase process over, check for error bits
				if (worker->rx[0] & 0x60)		// EERR (bit5) and PERR (bit6) must be clear.
				{
					/* Error during write/erase process */
					/*--- Clear status register --- */
					worker->tx[0] = 0x30;
					*busyFlag = true;
					if (ssp_add_job2(worker->busNr, worker->tx, 1, NULL, 0, NULL,  worker->ChipSelect)) {
						/* Error while adding job */
						worker->FlashStatus = FLASH_STAT_ERROR;
						worker->TxEraseCallback(FLASH_RES_JOB_ADD_ERROR,flashNr, worker->adr, worker->len);
						break;
					}
					worker->BusyTmoTicks = FLASH_TX_TIMEOUTTICKS;
					worker->FlashStatus = FLASH_STAT_CLEAR_ERRORS;
					worker->TxEraseCallback(FLASH_RES_TX_ERROR,flashNr, worker->adr, worker->len);
				} else {
					// Everything worked out fine -> callback with Success
					worker->FlashStatus = FLASH_STAT_IDLE;
					worker->TxEraseCallback(FLASH_RES_SUCCESS,flashNr, worker->adr, worker->len);
					//worker->TxEraseCallback(FLASH_RES_SUCCESS,flashNr, worker->adr, worker->CheckTxCounter);
				}
			}
			break;
		}

		case FLASH_STAT_WRITE_ERASE_INPROGRESS_DELAY: {
			worker->DelayCounter--;
			if (worker->DelayCounter <= 0) {
				// its time to make a new WIP read job
				// ClimbLedToggle(0);
				/*--- Check WIP-Bit and Error bits --- */
				worker->tx[0] = 0x05;
				worker->CheckTxCounter--;
				if (worker->CheckTxCounter <= 0) {
					worker->FlashStatus = FLASH_STAT_ERROR;
					worker->TxEraseCallback(FLASH_RES_TX_WRITE_TOO_LONG,flashNr, worker->adr, worker->len);
				} else {
					*busyFlag = true;
					if (ssp_add_job2(worker->busNr, worker->tx, 1, worker->rx, 1, NULL,  worker->ChipSelect)) {
						/* Error while adding job */
						worker->FlashStatus = FLASH_STAT_ERROR;
						worker->TxEraseCallback(FLASH_RES_JOB_ADD_ERROR,flashNr, worker->adr, worker->len);
						break;
					}
					worker->BusyTmoTicks = FLASH_TX_TIMEOUTTICKS;
					worker->FlashStatus = FLASH_STAT_WRITE_ERASE_INPROGRESS;
				}
			}
			break;
		}

		case FLASH_STAT_CLEAR_ERRORS: {
			// Clear errors job done
			worker->FlashStatus = FLASH_STAT_IDLE;
			break;
		}

		case FLASH_STAT_ERASE_CHECKWIP: {
			// ssp job to read the WIP bit ist ready
			if (worker->rx[0] & 0x01) {
				// The WIP bit is not clear. As we should wait for it to be ok when writing, this is an unexpected error here.
				worker->FlashStatus = FLASH_STAT_ERROR;
				worker->TxEraseCallback(FLASH_RES_WIPCHECK_ERROR,flashNr, worker->adr, 0);
				break;
			}

			/*--- Write Enable (WREN) --- */
			/* Set WREN bit to initiate write process */
			worker->tx[0] = 0x06; /* 0x06 WREN */

			*busyFlag = true;
			if (ssp_add_job2(worker->busNr, worker->tx, 1, NULL, 0, NULL,  worker->ChipSelect))
			{
				worker->FlashStatus = FLASH_STAT_ERROR;
				worker->TxEraseCallback(FLASH_RES_JOB_ADD_ERROR,flashNr, worker->adr, 0);
				break;
			}
			worker->BusyTmoTicks = FLASH_TX_TIMEOUTTICKS;
			worker->FlashStatus = FLASH_STAT_ERASE_SETWRITEBIT;
			break;
		}

		case FLASH_STAT_ERASE_SETWRITEBIT: {
			/* write bit job finished */
			/*--- Write - Page Program --- */
			worker->tx[0] = 0xDC; /* 0xDC sector erase, 4 byte address */
			worker->tx[1] = (worker->adr >> 24);
			worker->tx[2] = ((worker->adr & 0x00ff0000) >> 16);
			worker->tx[3] = ((worker->adr & 0x0000ff00) >> 8);
			worker->tx[4] = (worker->adr & 0x000000ff);

			*busyFlag = true;
			if (ssp_add_job2(worker->busNr, worker->tx, 5, NULL, 0, &worker->job_status,  worker->ChipSelect))
			{
				/* Error while adding job */
				worker->FlashStatus = FLASH_STAT_ERROR;
				worker->TxEraseCallback(FLASH_RES_JOB_ADD_ERROR,flashNr, worker->adr, 0);
				break;
			}
			worker->BusyTmoTicks = FLASH_TX_TIMEOUTTICKS;
			worker->FlashStatus = FLASH_STAT_TX_ERASE_TRANSFER_INPROGRESS;		// From here on its same as write process (wait for WIP to be cleared)
			break;
		}

		case FLASH_STAT_IDLE:
		default:
			// nothing to do in main loop.
			break;
		} // end switch

	}
}

/*
 * eeprom.c
 *
 *  Created on: 22.11.2019
 *  Code parts are copied from PEG flight software
  */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "eeprom.h"
#include "../../globals.h"
#include "../../layer1/I2C/obc_i2c.h"
#include "../main.h"
#include "../cli/cli.h"
#include "../tim/timer.h"
#include "../crc/obc_checksums.h"


// module variables
eeprom_status_page_t statusPage1;
//eeprom_status_page_t statusPage2;
//eeprom_status_page_t statusPage3;

// We control one possible write job and one read job at a given time ....
I2C_Data writePageJob;
uint8_t writePageTx[5 + EEPROM_PAGE_SIZE];
bool writeInProgress = false;
//void (*writeFinishedHandler)(void) = 0;

I2C_Data readPageJob;
uint8_t readPageTx[2];
uint8_t readPageRx[EEPROM_PAGE_SIZE];
bool readInProgress = false;
void (*readFinishedHandler)(eeprom_page_t *page) = 0;
void (*writeFinishedHandler)(void) = 0;

// Prototypes
void WriteStatusCmd(int argc, char *argv[]);
void WritePageCmd(int argc, char *argv[]);
void ReadStatusCmd(int argc, char *argv[]);
void ReadPageCmd(int argc, char *argv[]);
void ReadPageFinished(eeprom_page_t *page);
void ReadStatusReceived(eeprom_page_t *page);

void EepromInit() {
	// Register module Commands
	RegisterCommand("eeWriteName", WriteStatusCmd);
	RegisterCommand("eeStatus", ReadStatusCmd);
	RegisterCommand("readPage", ReadPageCmd);
	RegisterCommand("writePage", WritePageCmd);

	// trigger the read status command to show Name of Hardware
	ReadPageAsync(I2C_ADR_EEPROM1, EEPROM_STATUS_PAGE, ReadStatusReceived);
}

void ReadPageCmd(int argc, char *argv[]) {
	if (argc != 2) {
		printf("uasge: cmd <mem> <pageNr> where mem i one of EE1, EE2, ...\n" );
		return;
	}

	// CLI params to binary params
	uint8_t chipAdr = GetI2CAddrForMemoryDeviceName(argv[0]);
	uint16_t pageNr = atoi(argv[1]);

	// Binary Command
	if (! ReadPageAsync(chipAdr, pageNr, ReadPageFinished)) {
		printf("Not possible to initialize the block read operation! (currently used?)\n");
	}
}

void ReadPageFinished(eeprom_page_t *page) {
	printf("Page read Id: %02X, WriteCycl: %d, CS: %08X \nRawData:  ", page->id, (page->cycles_high<<8 | page->cycles_low), page->cs);
	for (int i=0; i<EEPROM_PAGE_SIZE; i++ ) {
		printf("%02X ", ((uint8_t*)page)[i]);
		if ((i+1)%8 == 0) {
			printf("   ");
		}
	}
	printf("\n");
}

void ReadStatusCmd(int argc, char *argv[]) {
	if (! ReadPageAsync(I2C_ADR_EEPROM1, EEPROM_STATUS_PAGE, ReadStatusReceived)) {
		printf("Not possible to initialize the block read operation! (currently used?)\n");
	}
}

void ReadStatusReceived(eeprom_page_t *page) {
	memcpy(&statusPage1, page, EEPROM_PAGE_SIZE);
	printf("Name:  '%s'\n", statusPage1.obc_name);
	printf("HWRev: '%s'\n", statusPage1.obc_hardware_version);
	//printf("ResetCnt %d %d %d\n", statusPage1.reset_counter1, statusPage1.reset_counter2, statusPage1.reset_counter3 );
	//printf("Write cycles: %d\n",(statusPage1.cycles_high << 16) | statusPage1.cycles_low );
	printf("SWVer: '%s'\n", SW_VERSION);
}


bool ReadPageAsync(uint8_t chipAdress, uint16_t pageNr, void (*finishedHandler)(eeprom_page_t *page)) {
	if (readInProgress) {
		return false;
	}
	readInProgress = true;

	readPageJob.device = ONBOARD_I2C;
	readPageJob.tx_size = 2;
	readPageJob.tx_data = readPageTx;
	readPageJob.rx_size = EEPROM_PAGE_SIZE;
	readPageJob.rx_data = readPageRx;

	readPageJob.adress = chipAdress;
	readPageTx[0] = ((pageNr * EEPROM_PAGE_SIZE) >> 8); 	// Addr. high
	readPageTx[1] = ((pageNr * EEPROM_PAGE_SIZE) & 0xFF); 	// Addr. low

	readFinishedHandler = finishedHandler;
	i2c_add_job(&readPageJob);

	return true;
}


void WritePageCmd(int argc, char *argv[]) {
	uint8_t fillByte = 0xAA;
	if (argc < 2) {
		printf("uasge: cmd <mem> <pageNr> <pattenCode> where mem is one of EE1, EE2, ... and patternCode = A,B or <byte>.\n" );
		return;
	}
	// CLI params to binary params
	uint8_t chipAdr = GetI2CAddrForMemoryDeviceName(argv[0]);
	uint16_t pageNr = atoi(argv[1]);

	if (argc >= 3) {
		 if (strcmp(argv[2],"A") == 0) {
			 fillByte = 0xAA;
		 } else if (strcmp(argv[2],"B") == 0) {
			 fillByte = 0x55;
		 } else {
			 fillByte = atoi(argv[2]);
		 }
	}

	char data[EEPROM_PAGE_SIZE];
	for (int i=0; i < EEPROM_PAGE_SIZE; i++ ){
		data[i] = fillByte;
	}

	// Binary Command
	if (! WritePageAsync(chipAdr, pageNr, data, NULL)) {
		printf("Not possible to initialize the page write operation! (currently used?)\n");
	}

}

bool WritePageAsync(uint8_t chipAdress, uint16_t pageNr, char *data, void (*finishedHandler)(void)) {
	if (writeInProgress) {
		return false;
	}
	writeInProgress = true;

	writePageJob.device = ONBOARD_I2C;
	writePageJob.adress = chipAdress;
	writePageTx[0] = ((pageNr * EEPROM_PAGE_SIZE) >> 8); 		// Address high
	writePageTx[1] = ((pageNr * EEPROM_PAGE_SIZE) & 0xFF); 	// Address low
	memcpy(&(writePageTx[2]),data, EEPROM_PAGE_SIZE);			// Data
	writePageJob.tx_size = EEPROM_PAGE_SIZE + 2;
	writePageJob.tx_data = writePageTx;
	writePageJob.rx_data = NULL;
	writePageJob.rx_size = 0;
	writeFinishedHandler = finishedHandler;
	//i2c_add_job(&writePageJob);

	return ( i2c_add_job(&writePageJob) == 0 );
}


void WriteStatusCmd(int argc, char *argv[]) {
	eeprom_status_page_t page;
	page.id = EEPROM_STATUS_PAGE_ID;
	page.reset_counter1 = statusPage1.reset_counter1;
	page.reset_counter2 = statusPage1.reset_counter2;
	page.reset_counter3 = statusPage1.reset_counter3;
	uint32_t cycles = (statusPage1.cycles_high << 16) | statusPage1.cycles_low;
	cycles++;
	page.cycles_low = cycles & 0x0000FFFF;
	page.cycles_high = (uint8_t)(cycles >> 16);
	strncpy(page.obc_name,"<nset>",7);
	strncpy(page.obc_hardware_version,"-.-",4);
	if (argc >= 1) {
		strncpy( page.obc_name, argv[0], 7);
		if (argc >= 2) {
			strncpy(page.obc_hardware_version, argv[1], 4);
		}
	}
	/* Calculate checksum */
	page.cs = crc32((uint8_t *)(&page), 28);

	WritePageAsync(I2C_ADR_EEPROM1, EEPROM_STATUS_PAGE, (char *)&page, NULL);
//		writePageJob.device = ONBOARD_I2C;
//		writePageJob.adress = I2C_ADR_EEPROM1;
//		writePageTx[0] = ((EEPROM_STATUS_PAGE * 32) >> 8); 		// Address high
//		writePageTx[1] = ((EEPROM_STATUS_PAGE * 32) & 0xFF); 	// Address low
//		memcpy(&(writePageTx[2]),&page, 32);			// Data
//		writePageJob.tx_size = 34;
//		writePageJob.tx_data = writePageTx;
//		writePageJob.rx_data = NULL;
//		writePageJob.rx_size = 0;
//		i2c_add_job(&writePageJob);
//	}
}

void EepromMain() {
	if (writeInProgress) {
		if (writePageJob.job_done == 1) {
			// Result is finshed -> call handler routine
//			if (writePageJob.error != I2C_ERROR_NO_ERROR) {
//				printf("Error %d while writting to EEPROM", writePageJob.error);
//			} else {
//				if (writeFinishedHandler != 0) {
//					writeFinishedHandler();
//				} else {
//					uint16_t pagenr = (writePageTx[0] << 8 | writePageTx[1]) / 32;
//					printf("Page %d (ID:%x) written.", pagenr, writePageTx[2]);
//				}
//			}
			// Our Main-tick is 250ms, so it is very unlikely that we have not had the 5ms Page Write time for our EEPROMS when we are here.
			// to be 100%sure
			// either wait another tick here -> wastes 250ms
			//
			// or lets waste this 5ms here now (and block the main loop for this time).
			TimBlockMs(5);
			writeInProgress = false;

			if (writePageJob.error != I2C_ERROR_NO_ERROR) {
					printf("Error %d while writting to EEPROM", writePageJob.error);
			} else {
				if (writeFinishedHandler != 0) {
					writeFinishedHandler();
				} else {
					uint16_t pagenr = (writePageTx[0] << 8 | writePageTx[1]) / 32;
						printf("Page %d (ID:%x) written.", pagenr, writePageTx[2]);
				}
			}
		}
	}

	if (readInProgress) {
		if (readPageJob.job_done == 1) {
			// Result is finshed -> call handler routine
			readInProgress = false;
			if (readFinishedHandler != 0 && readPageJob.error == I2C_ERROR_NO_ERROR) {
				readFinishedHandler((eeprom_page_t *)readPageRx);
			}
			if (readPageJob.error != I2C_ERROR_NO_ERROR) {
				printf("I2C Error '%d'. No received handler called!", readPageJob.error);
			}
		}
	}
}




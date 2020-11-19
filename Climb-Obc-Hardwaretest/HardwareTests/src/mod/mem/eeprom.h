/*
 * eeprom.h
 *
 *  Created on: 22.11.2019
 */

#ifndef MOD_MEM_EEPROM_H_
#define MOD_MEM_EEPROM_H_

#include <stdint.h>
#include <stdbool.h>

#define EEPROM_SIZE 			8192 	/* Bytes */
#define EEPROM_PAGE_SIZE 		32 		/* Bytes */
#define EEPROM_PAGE_NUMBER 		256 	/* Pages (EEPROM_SIZE/EEPROM_PAGE_SIZE) */

/* --- Add your page numbers and information here -------------------------------------- */
/* Unique page numbers */
#define EEPROM_STATUS_PAGE 		0
//#define EEPROM_CONFIG_PAGE1 	1
//#define EEPROM_CONFIG_PAGE2 	2
//
///* Logger */
//#define EEPROM_LOGSTATUS_PAGE  	3
//#define EEPROM_LOGSCRIPT_PAGE1 	4
//#define EEPROM_LOGSCRIPT_PAGE2 	5
//#define EEPROM_LOGBADCNT_PAGE   6
//#define EEPROM_LOGREADPS_PAGE   7
//#define EEPROM_LOGWODRNG_PAGE1  9
//#define EEPROM_LOGWODRNG_PAGE2  10
//#define EEPROM_LOGWODRNG_PAGE3  11
//
///* state machine */
//#define EEPROM_MISSION_PAGE   	8
//
///* science unit */
//#define EEPROM_SCIENCE_PAGE   	12

/* IDs */
#define EEPROM_RESERVED_PAGE_ID 	0x00		// Was war hier die Idee dahinter ?
#define EEPROM_STATUS_PAGE_ID  		0x5A

//#define EEPROM_CONFIG_PAGE1_ID 		0x7B
//#define EEPROM_CONFIG_PAGE2_ID 		0x9B
///* Logger */
//#define EEPROM_LOGSTATUS_PAGE_ID  	0xF0
//#define EEPROM_LOGSCRIPT_PAGE1_ID 	0xF3
//#define EEPROM_LOGSCRIPT_PAGE2_ID 	0x33
//#define EEPROM_LOGBADCNT_PAGE_ID  	0x44
//#define EEPROM_LOGREADPS_PAGE_ID  	0x55
//#define EEPROM_LOGWODRNG_PAGE1_ID   0xA1
//#define EEPROM_LOGWODRNG_PAGE2_ID   0xA5
//#define EEPROM_LOGWODRNG_PAGE3_ID   0xA9
///* state machine */
//#define EEPROM_MISSION_PAGE_ID	  	0x66
//#define EEPROM_SCIENCE_PAGE_ID	  	0x77
/* -------------------------------------------------------------------------------------- */

/* General structure of one EEPROM page */
typedef struct eeprom_page_s
{
	uint8_t id;						// Specifies the type of block stored here
	uint8_t cycles_high;			// counts write cycles o fthis block
	uint16_t cycles_low;
	uint8_t data[24];
	uint32_t cs;
} eeprom_page_t;

/* */
//typedef struct eeprom_page_array_s
//{
//	eeprom_page_t page[4];
//} eeprom_page_array_t;

typedef struct eeprom_status_page_s
{
	uint8_t id;
	uint8_t cycles_high;
	uint16_t cycles_low;
	uint32_t reset_counter1;
	uint32_t reset_counter2;
	uint32_t reset_counter3;
	char obc_hardware_version[4];
	char obc_name[7];
	uint8_t testdata;	// Do not move this entry inside the struct
	uint32_t cs;
} eeprom_status_page_t;

//typedef enum eeproms_e
//{
//	EEP1 = 1, EEP2, EEP3, FRAM
//} eeproms_t;
//

void EepromInit();						    // Module Init called once prior mainloop
void EepromMain();							// Module routine participating each mainloop.


bool ReadPageAsync(uint8_t chipAdress, uint16_t pageNr, void (*finishedHandler)(eeprom_page_t *page));
bool WritePageAsync(uint8_t chipAdress, uint16_t pageNr, char *data,  void (*finishedHandler)(void));

//RetVal eeprom_increment_reset_counter(void);
//RetVal eeprom_write_page(uint8_t page, eeprom_page_t * data);
//RetVal eeprom_read_page(uint8_t page, eeprom_page_t * rx_data);
//RetVal eeprom_program_version_info(char * name, char * hw);
//RetVal eeprom_config_store(void);
//RetVal eeprom_config_read(void);






#endif /* MOD_MEM_EEPROM_H_ */

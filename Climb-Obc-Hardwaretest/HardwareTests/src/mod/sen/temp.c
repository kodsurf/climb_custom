/*
 * temp.c
 *
 *  Created on: 11.01.2020
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "temp.h"
#include "../../globals.h"
#include "../../layer1/I2C/obc_i2c.h"
#include "../cli/cli.h"

bool tmpReadInProgress;
I2C_Data tmpReadTempJob;
uint8_t tmpTxCommand[1];
uint8_t tmpRxBuffer[2];

void ReadTemperatureCmd(int argc, char *argv[]);

void TmpInit(){
	tmpReadInProgress = false;
	RegisterCommand("readTemp", ReadTemperatureCmd);
}

void TmpMain(){
	if ((tmpReadInProgress) && (tmpReadTempJob.job_done == 1)) {
		tmpReadInProgress = false;
		int16_t tr = ((int16_t)tmpRxBuffer[0])<<4 | ((int16_t)tmpRxBuffer[0])>>4;
		float t = ((float)tr) * 0.0625F;
		printf("TMP100: %02X %02X %.1f C\n", tmpRxBuffer[0], tmpRxBuffer[1], t );
	}
}

void ReadTemperatureCmd(int argc, char *argv[]) {
	 tmp_read_temperature();
}

void tmp_read_temperature() {
	if (!tmpReadInProgress) {
		tmpReadInProgress = true;

		tmpTxCommand[0] = 0x00;		// just read temp word at register 00
		tmpReadTempJob.device = ONBOARD_I2C;
		tmpReadTempJob.tx_size = 1;
		tmpReadTempJob.tx_data = tmpTxCommand;
		tmpReadTempJob.rx_size = 2;
		tmpReadTempJob.rx_data = tmpRxBuffer;
//		tmpReadTempJob.adress  = I2C_ADR_TEMP;
		i2c_add_job(&tmpReadTempJob);
	}
}

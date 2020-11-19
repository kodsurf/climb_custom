/*
 * main.h
 *
 *  Created on: 02.11.2019
 *      Author: Robert
 */

#ifndef MOD_MAIN_H_
#define MOD_MAIN_H_

typedef struct IEC60335_FlashSign_struct
{
	uint32_t word0;
	uint32_t word1;
	uint32_t word2;
	uint32_t word3;
} FlashSign_t; //, *pFlashSign_t;


// Module API Prototypes
void MainInit();
void MainMain();

// Module API functions
void CalculateFlashSignatureAsync(uint32_t startAddr, uint32_t length, void(*SignatureCalculated)(FlashSign_t signature));
bool IEC60335_IsEqualSignature(FlashSign_t *sign1, FlashSign_t *sign2);

#endif /* MOD_MAIN_H_ */

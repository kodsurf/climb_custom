#include <stdio.h>
#include <time.h>
#include <chip.h>

/* Other includes */
#include "obc_rtc.h"
#include "../crc/obc_checksums.h"
#include "../cli/cli.h"
#include "timer.h"

typedef enum rtc_status_e {
	RTC_STAT_SYNCHRONIZED	= 0x11,
	RTC_STAT_RUNNING		= 0x22,
	RTC_STAT_RESETDEFAULT	= 0x33,
	RTC_STAT_UNKNOWN		= 0xDD,
	RTC_STAT_XTAL_ERROR		= 0xEE,
} rtc_status_t;

//// Usage of 19 bytes General Purpose Register
//typedef enum rtc_gpridx_e {
//	RTC_GPRIDX_STATUS = 0,
//
//	RTC_GPRIDX_CRC8 = 19
//} rtc_gpridx_t;


// Prototypes
//uint8_t RtcReadGpr(rtc_gpridx_t idx);
//void RtcWriteGpr(rtc_gpridx_t idx, uint8_t byte);
void show_gpregs(void);

//
// From RTOS
//#define configMAX_LIBRARY_INTERRUPT_PRIORITY    ( 5 )
#define RTC_INTERRUPT_PRIORITY  6 //(configMAX_LIBRARY_INTERRUPT_PRIORITY + 1)  /* RTC - highest priority after watchdog! */

// old prototypes
//void rtc_get_val(RTC_TIME_T *tim);
//uint32_t rtc_get_date(void);
//uint32_t rtc_get_time(void);
uint64_t rtc_get_datetime(void);
//uint64_t rtc_get_extended_time(void);
void rtc_calculate_epoch_time(void);
uint32_t rtc_get_epoch_time(void);
//void rtc_set_time(RTC_TIME_Type * etime);
void rtc_correct_by_offset(int32_t offset_in_seconds);

//RetVal rtc_check_if_reset(void);
//uint8_t rtc_checksum_calc(uint8_t *data);
//RetVal rtc_sync(RTC_TIME_T *tim);

//RetVal rtc_backup_reg_read(uint8_t id, uint8_t * val);
//RetVal rtc_backup_reg_set(uint8_t id, uint8_t val);
//RetVal rtc_backup_reg_reset(uint8_t go);
//
//typedef enum errors_e
//{
//	EC_NO_ERROR = 0,
//	EC_POWER_SW_ERROR,
//	EC_BROWN_OUT_DETECTED,
//	EC_STACK_OVERFLOW,
//	EC_MANUAL_RESET,
//	EC_HARD_FAULT,
//	EC_SCHEDULER_STOPPED,
//	EC_SIGNATURE_ERROR = 255
//} error_code;

//
//typedef struct obc_status_s
//{
//	unsigned int last_reset_source1 :1; /* Bit 0 *//* (source1 source2) POR: 0b00, EXTR: 0b01, WDTR: 0b10; BODR: 0b11 */
//	unsigned int last_reset_source2 :1; /* Bit 1 */
//	unsigned int obc_powersave :1; /* Bit 2 */
//	unsigned int rtc_synchronized :1; /* Bit 3 */
//	unsigned int rtc_initialized :1; /* Bit 4 */
//	unsigned int :1; /* Bit 5 */
//	unsigned int :1; /* Bit 6 */
//	unsigned int :1; /* Bit 7 */
//	unsigned int :1; /* Bit 8 */
//	unsigned int :1; /* Bit 9 */
//	unsigned int :1; /* Bit 10 */
//	unsigned int :1; /* Bit 11 */
//	unsigned int :1; /* Bit 12 */
//	unsigned int :1; /* Bit 13 */
//	unsigned int :1; /* Bit 14 */
//	unsigned int :1; /* Bit 15 */
//	unsigned int rtc_oszillator_error :1; /* Bit 16 */
//	unsigned int :1; /* Bit 17 */
//	unsigned int :1; /* Bit 18 */
//	unsigned int :1; /* Bit 19 */
//	unsigned int :1; /* Bit 20 */
//	unsigned int :1; /* Bit 21 */
//	unsigned int :1; /* Bit 22 */
//	unsigned int :1; /* Bit 23 */
//	unsigned int :1; /* Bit 24 */
//	unsigned int :1; /* Bit 25 */
//	unsigned int :1; /* Bit 26 */
//	unsigned int :1; /* Bit 27 */
//	unsigned int :1; /* Bit 28 */
//	unsigned int :1; /* Bit 29 */
//	unsigned int :1; /* Bit 30 */
//	unsigned int :1; /* Bit 31 */
//
//	unsigned int error_code_before_reset ;
//
//}
//volatile obc_status_t;
//
//obc_status_t obc_status;

//void taskDISABLE_INTERRUPTS() {
//	// in peg here is some ASM inline code called from RTOS
//	__disable_irq();
//}
//
//void taskENABLE_INTERRUPTS() {
//	// in peg here is some ASM inline code called from RTOS
//	__enable_irq();
//}

#define EXTENDED_DEBUG_MESSAGES true

volatile uint32_t rtc_epoch_time;
volatile uint8_t rtc_currentDayOfMonth;
volatile bool  rtc_dayChanged;
volatile uint32_t rtc_dayChangedAtSeconds;

uint32_t rtc_calibration_base;
uint32_t rtc_calibration_day;

void RtcResetCalibrationMessurements() {
	// Reset the Calibration messurements
	rtc_currentDayOfMonth = 0;
	rtc_dayChanged = false;
	rtc_dayChangedAtSeconds = 0;
	rtc_calibration_base = 0;
	rtc_calibration_day = 0;
}


void RtcGetTimeCmd(int argc, char *argv[]) {
	printf("RTC DateTime: %lld (Status: %02X)\n",rtc_get_datetime(),RtcReadGpr(RTC_GPRIDX_STATUS));
	if (argc > 0) {
		show_gpregs();
	}
}

void RtcSetTimeCmd(int argc, char *argv[]) {
	// setTime <hours> <minutes> <seconds>[ sync]
	uint8_t sec = 0;
	uint8_t min = 0;
	uint8_t hrs = 0;
	bool synchronized = false;

	if (argc > 0) {
		hrs = atoi(argv[0]);
	}
	if (argc > 1) {
		min = atoi(argv[1]);
	}
	if (argc > 2) {
		sec = atoi(argv[2]);
	}
	if (argc > 3) {
		synchronized = true;
	}

	// binary cmd
	RtcSetTime(hrs, min, sec, synchronized);

	printf("RTC DateTime: %lld (Status: %02X)\n",rtc_get_datetime(),RtcReadGpr(RTC_GPRIDX_STATUS));
}

void RtcSetTime(uint8_t hours, uint8_t minutes, uint8_t seconds, bool synchronized) {
	if (hours>23 || minutes > 59 || seconds > 59) {
		return;		// TODO: log 'out of range' event !?
	}

	uint32_t ccr_val = LPC_RTC->CCR;

	/* Temporarily disable */
	if (ccr_val & RTC_CCR_CLKEN) {
		LPC_RTC->CCR = ccr_val & (~RTC_CCR_CLKEN) & RTC_CCR_BITMASK;
	}

	Chip_RTC_SetTime(LPC_RTC, RTC_TIMETYPE_HOUR, hours);
	Chip_RTC_SetTime(LPC_RTC, RTC_TIMETYPE_MINUTE, minutes);
	Chip_RTC_SetTime(LPC_RTC, RTC_TIMETYPE_SECOND, seconds);

	/* Restore to old setting */
	LPC_RTC->CCR = ccr_val;

	if (RtcReadGpr(RTC_GPRIDX_STATUS) != RTC_STAT_XTAL_ERROR) {
		if (synchronized) {
			RtcWriteGpr(RTC_GPRIDX_STATUS, RTC_STAT_SYNCHRONIZED);
		} else {
			RtcWriteGpr(RTC_GPRIDX_STATUS, RTC_STAT_RUNNING);
		}
	}

	RtcResetCalibrationMessurements();

}

void RtcSetDateCmd(int argc, char *argv[]) {
	uint16_t year = 0;
	uint8_t month = 0;
	uint8_t day = 0;

	if (argc > 0) {
		year = atoi(argv[0]);
	}
	if (argc > 1) {
		month = atoi(argv[1]);
	}
	if (argc > 2) {
		day = atoi(argv[2]);
	}

	// binary cmd
	RtcSetDate(year, month, day);

	printf("RTC DateTime: %lld (Status: %02X)\n",rtc_get_datetime(),RtcReadGpr(RTC_GPRIDX_STATUS));

}

void RtcSetDate(uint16_t year, uint8_t month, uint8_t dayOfMonth) {
	if (year>4095 || month > 12 || month < 1 || dayOfMonth > 31 || dayOfMonth < 1 ) {
		return;		// TODO: log 'out of range' event !?
	}

	uint32_t ccr_val = LPC_RTC->CCR;

	/* Temporarily disable */
	if (ccr_val & RTC_CCR_CLKEN) {
		LPC_RTC->CCR = ccr_val & (~RTC_CCR_CLKEN) & RTC_CCR_BITMASK;
	}

	Chip_RTC_SetTime(LPC_RTC, RTC_TIMETYPE_DAYOFMONTH, dayOfMonth );
	Chip_RTC_SetTime(LPC_RTC, RTC_TIMETYPE_MONTH, month);
	Chip_RTC_SetTime(LPC_RTC, RTC_TIMETYPE_YEAR, year);

	/* Restore to old setting */
	LPC_RTC->CCR = ccr_val;

	RtcResetCalibrationMessurements();
}

void show_gpregs(void) {
	char *ptr = (char*) &(LPC_RTC->GPREG);
	printf("RTC GPR: ");
	for (int i=0;i<20;i++) {
		printf("%02X ", ptr[i]);
	}
	printf("\n");
}

void RtcReadAllGprs(uint8_t *data) {
	char *ptr = (char*) &(LPC_RTC->GPREG);
	for (int i=0;i<20;i++) {
		data[i] = ptr[i];
	}
}


void RtcInitializeGpr() {
	for (int i=0;i<19;i++) {
		RtcWriteGpr(i, 'a' + i);		// Some Recognizable pattern ;-).
	}
}

bool RtcIsGprChecksumOk(void) {
	uint8_t *gprbase;
	// The rtc does not change its GPR on itself, does it?
	// So no need to copy and/or disable the interrupts here !?
	gprbase = (uint8_t *) &(LPC_RTC->GPREG);
	uint8_t crc = CRC8(gprbase, 19) + 1;
	return (gprbase[19] == crc);
}

// We use the 5 GPR registers as a byte store with 19 bytes + 1byte CRC8
void RtcWriteGpr(rtc_gpridx_t idx, uint8_t byte) {
	if ((idx < 19) && (idx >= 0)) {
		// GPREG only can be written as uint32/4byte at once!
		// A single uint8_t ptr does not work here :-( -> it writes 4 (same) bytes at once !?
		uint32_t *gprbase = (uint32_t *)(&(LPC_RTC->GPREG));
		uint32_t *ptr = gprbase;
		uint8_t channel = idx /4;
		uint8_t tmpBytes[4];

		ptr += channel;
		*((uint32_t *)tmpBytes) = *ptr;
		tmpBytes[idx % 4] = byte;
		*ptr = *((uint32_t *)tmpBytes);

		// last byte of word 4 is checksum
		ptr = gprbase + 4;
		*((uint32_t *)tmpBytes) = *ptr;
		tmpBytes[3] = CRC8((uint8_t*)&(LPC_RTC->GPREG), 19) + 1;
		*ptr = *((uint32_t *)tmpBytes);
	}
}

uint8_t RtcReadGpr(rtc_gpridx_t idx) {
	// Other than writing, for reading a uint8 ptr is good enough to get all bytes separately.
	uint8_t *gprbase = (uint8_t *)(&(LPC_RTC->GPREG));
	if (idx>19) {
		//TODO: log an 'out of range' event here !!!
		idx = 19;
	} else if (idx < 0) {
		//TODO: log an 'out of range' event here !!!
		idx = 0;
	}
	return gprbase[idx];
}

void RtcInit(void) {
	rtc_status_t status = RTC_STAT_UNKNOWN;

	RTC_TIME_T 	tim;

	Chip_RTC_Init(LPC_RTC);

	//show_gpregs();	// print all GPREGS for debugging

	/* Init Timer 1 before RTC enable */			// TODO module inits and sequence to be determined ....
	//timer0_init();

	/* Check RTC reset */
	if (!RtcIsGprChecksumOk() || (RtcReadGpr(RTC_GPRIDX_STATUS) == 0x61)) {
		printf("RTC GPR Checksum Error (or status is init byte 0x61) -> Reinit GPRs\n", 0);
		RtcInitializeGpr();

		/* RTC module has been reset, time and data invalid */
		/* Set to default values */
		tim.time[RTC_TIMETYPE_SECOND] = 0;
		tim.time[RTC_TIMETYPE_MINUTE] = 0;
		tim.time[RTC_TIMETYPE_HOUR] = 0;
		tim.time[RTC_TIMETYPE_DAYOFMONTH] = 1;
		tim.time[RTC_TIMETYPE_DAYOFWEEK] = 1;
		tim.time[RTC_TIMETYPE_DAYOFYEAR] = 1;
		tim.time[RTC_TIMETYPE_MONTH] = 1;
		tim.time[RTC_TIMETYPE_YEAR] = 1001;
		Chip_RTC_SetFullTime(LPC_RTC, &tim);

		status = RTC_STAT_RESETDEFAULT;
		//rtc_backup_reg_reset(1);
		//rtc_backup_reg_set(1, 0x00);	// RTC is definitely not in sync
		//obc_status.rtc_synchronized = 0;
	}
	else
	{
		// read status from GPR
		status = RtcReadGpr(RTC_GPRIDX_STATUS);
		printf("RTC GPR Checksum ok. Status: %02X DateTime: %lld\n", status, rtc_get_datetime());

		/* RTC was running - check if time is correct */
//		uint8_t rval = 0x00;
//		rtc_backup_reg_read(1, &rval);
//		if (rval == RTC_SYNCHRONIZED)
//		{
//			// RTC was running and a previous synchronization was successful -> time is valid
//			obc_status.rtc_synchronized = 1;
//#if EXTENDED_DEBUG_MESSAGES
//
//			printf("OBC: RTC is synchronized\n", 0);
//#endif
//
//		}
//		else
//		{
//			// RTC was running, but time was out of sync already
//			obc_status.rtc_synchronized = 0;
//#if EXTENDED_DEBUG_MESSAGES
//
//			printf("OBC: RTC is NOT synchronized\n", 0);
//#endif
//
//		}

		/* Restore last error code from RTC register */
//		obc_status.error_code_before_reset = error_code_get();
	}

	// Check if RTC XTAL is running.

	Chip_RTC_Enable(LPC_RTC, ENABLE);		// We have to enable the RTC
	TimBlockMs((uint8_t)400);				// This 400ms are needed to get a stable RTC OSC after Power on (tested with LPCX board).
	LPC_RTC->RTC_AUX &= RTC_AUX_RTC_OSCF;	// Now clear the OSC Error bit (its set to 1 on each RTC power on)
	TimBlockMs(5);							// and lets wait another short time.

	// Now there shouldn't be a (new) error bit set here.
	if (LPC_RTC->RTC_AUX & RTC_AUX_RTC_OSCF)
	{
		// If so (tested with defect OBC board!) we really have no RTC OSC running!
		LPC_RTC->RTC_AUX &= RTC_AUX_RTC_OSCF;	// Clear the error  bit.
		printf("RTC: Oscillator error!\n");
		status = RTC_STAT_XTAL_ERROR;
	}

	rtc_calculate_epoch_time();
	RtcWriteGpr(RTC_GPRIDX_STATUS, status);

	Chip_RTC_CntIncrIntConfig(LPC_RTC, RTC_AMR_CIIR_IMSEC,  ENABLE);

	NVIC_SetPriority(RTC_IRQn, RTC_INTERRUPT_PRIORITY);
	NVIC_EnableIRQ(RTC_IRQn); /* Enable interrupt */

	RegisterCommand("getTime", RtcGetTimeCmd);
	RegisterCommand("setTime", RtcSetTimeCmd);
	RegisterCommand("setDate", RtcSetDateCmd);

	//show_gpregs();	// print all GPREGS for debugging
	return;
}

void RTC_IRQHandler(void)
{
	//LPC_TIM0->TC = 0; // Synchronize ms-timer to RTC seconds TODO....
	Chip_RTC_ClearIntPending(LPC_RTC, RTC_INT_COUNTER_INCREASE);
	Chip_RTC_ClearIntPending(LPC_RTC, RTC_INT_ALARM);

	if (rtc_currentDayOfMonth == 0) {
		rtc_currentDayOfMonth = LPC_RTC->TIME[RTC_TIMETYPE_DAYOFMONTH];
	}
	if (rtc_currentDayOfMonth !=  LPC_RTC->TIME[RTC_TIMETYPE_DAYOFMONTH]) {
		// Day changed
		rtc_currentDayOfMonth = LPC_RTC->TIME[RTC_TIMETYPE_DAYOFMONTH];
		rtc_dayChangedAtSeconds = secondsAfterReset;
		rtc_dayChanged = true;
	}

	rtc_epoch_time++; /* increment QB50 s epoch variable and calculate UTC time */	// TODO ...

	rtc_status_t status = RtcReadGpr(RTC_GPRIDX_STATUS);
	if (status == RTC_STAT_XTAL_ERROR) {
		// There was an error while init (no RTC Clock running). Now it seems to be ok (otherwise there would not be an IRQ)
		// clear the error bit now.
		if (LPC_RTC->RTC_AUX & RTC_AUX_RTC_OSCF)
		{
			LPC_RTC->RTC_AUX &= RTC_AUX_RTC_OSCF;	// Clear the error by writing to this bit now.
		}
		// TODO: is this assumption here correct. We had an unknown time from init until now but it seems to run from here on.
		//       For sure it is not synchronized any more.....
		RtcWriteGpr(RTC_GPRIDX_STATUS, RTC_STAT_UNKNOWN);
	}
	/* Do powersave modes or other things here */
	/*if (obc_status.obc_powersave)
	{
		// Reset watchdog regularly if OBC is in powersave
		WDT_Feed();
	}*/
}

/*void rtc_set_time(RTC_TIME_Type * etime)
{
	RTC_TIME_Type tim;

	if (obc_status.rtc_initialized == 0)
	{
		return;
	}

	if (etime == NULL)
	{
		tim.SEC = 0;
		tim.MIN = 36;
		tim.HOUR = 16;
		tim.DOM = 1;
		tim.DOW = 1;
		tim.DOY = 29;
		tim.MONTH = 1;
		tim.YEAR = 2016;

		RTC_SetFullTime(LPC_RTC, &tim);
	}
	else
	{
		RTC_SetFullTime(LPC_RTC, etime);
	}

	rtc_backup_reg_reset(1); // optional
}*/


struct tm * gmtime_wrapper(int32_t * corrected_time)
{
	return gmtime((uint32_t *)corrected_time);
}

void rtc_correct_by_offset(int32_t offset_in_seconds)
{
	struct tm now;
	RTC_TIME_T rtc_tim;
	volatile uint32_t corrected_time;

//	TODO
//	if (obc_status.rtc_initialized == 0)
//	{
//		return;
//	}

	/* Current time */
	Chip_RTC_GetFullTime(LPC_RTC, &rtc_tim);

	now.tm_hour = rtc_tim.time[RTC_TIMETYPE_HOUR];
	now.tm_min = rtc_tim.time[RTC_TIMETYPE_MINUTE];
	now.tm_sec = rtc_tim.time[RTC_TIMETYPE_SECOND];
	now.tm_mday = rtc_tim.time[RTC_TIMETYPE_DAYOFMONTH]; 	/* Day of month (1 - 31) */
	now.tm_mon = rtc_tim.time[RTC_TIMETYPE_MONTH]- 1;	 	/* Months since January (0 -11)*/
	now.tm_year = rtc_tim.time[RTC_TIMETYPE_MONTH] - 1900;   /* Years since 1900 */

	corrected_time = ((uint32_t) mktime(&now)) + offset_in_seconds;

	if (corrected_time > 2147483648U)
	{
		// Corrected time is out of range
		return;
	}

	volatile struct tm *t = gmtime_wrapper((int32_t *) &corrected_time);

	rtc_tim.time[RTC_TIMETYPE_HOUR] 		= t->tm_hour;
	rtc_tim.time[RTC_TIMETYPE_MINUTE] 		= t->tm_min;
	rtc_tim.time[RTC_TIMETYPE_SECOND] 		= t->tm_sec;
	rtc_tim.time[RTC_TIMETYPE_DAYOFMONTH] 	= t->tm_mday;
	rtc_tim.time[RTC_TIMETYPE_MONTH]   		= t->tm_mon + 1; 		// RTC months from 1 - 12
	rtc_tim.time[RTC_TIMETYPE_YEAR]    		= t->tm_year + 1900; 	// RTC years with 4 digits (YYYY)

	Chip_RTC_SetFullTime(LPC_RTC, &rtc_tim);
	rtc_calculate_epoch_time();
	return;
}




void rtc_calculate_epoch_time(void)
{
	struct tm start, now;
	RTC_TIME_T rtc_tim;
	rtc_epoch_time = 0;

	/* 01.01.2000, 00:00:00 */
	start.tm_hour = 0;
	start.tm_min = 0;
	start.tm_sec = 0;
	start.tm_mday = 1; /* Day of month (1 - 31) */
	start.tm_mon = 0; /* Months since January (0 -11)*/
	start.tm_year = 100; /* Years since 1900 */

	/* Current time */

	Chip_RTC_GetFullTime(LPC_RTC, &rtc_tim);

	now.tm_hour = rtc_tim.time[RTC_TIMETYPE_HOUR];
	now.tm_min = rtc_tim.time[RTC_TIMETYPE_MINUTE];
	now.tm_sec = rtc_tim.time[RTC_TIMETYPE_SECOND];
	now.tm_mday = rtc_tim.time[RTC_TIMETYPE_DAYOFMONTH]; /* Day of month (1 - 31) */
	now.tm_mon = rtc_tim.time[RTC_TIMETYPE_MONTH] - 1; /* Months since January (0 -11)*/
	now.tm_year = rtc_tim.time[RTC_TIMETYPE_YEAR] - 1900; /* Years since 1900 */

	rtc_epoch_time = (uint32_t) difftime(mktime(&now), mktime(&start));
	return;
}

//void rtc_get_val(RTC_TIME_T *tim)
//{
//	Chip_RTC_GetFullTime(LPC_RTC, tim);
//}


/* Returns the current RTC time according to UTC.
 * Parameters: 	none
 * Return value: date as uint32 with a decimal number formated as HHMMSS.
 */
uint32_t rtc_get_time(void)
{
	RTC_TIME_T tim;
	Chip_RTC_GetFullTime(LPC_RTC, &tim);

	// TODO: tim0 is not used for ms counting yet.....
	return (tim.time[RTC_TIMETYPE_SECOND] + tim.time[RTC_TIMETYPE_MINUTE] * 100 + tim.time[RTC_TIMETYPE_HOUR] * 10000);
	//return (0 + tim.time[RTC_TIMETYPE_SECOND] * 1000 + tim.time[RTC_TIMETYPE_MINUTE] * 100000 + tim.time[RTC_TIMETYPE_HOUR] * 10000000);
	//return ((LPC_TIM0->TC) + tim.SEC * 1000 + tim.MIN * 100000 + tim.HOUR * 10000000);
}

uint32_t rtc_get_date(void)
{
	/* Returns the current RTC date according to UTC.
	 * Parameters: 	none
	 * Return value: date as uint32 with a decimal number formated as YYYYmmdd.
	 */
	RTC_TIME_T tim;
	Chip_RTC_GetFullTime(LPC_RTC, &tim);

	return (tim.time[RTC_TIMETYPE_DAYOFMONTH] + tim.time[RTC_TIMETYPE_MONTH] * 100 + (tim.time[RTC_TIMETYPE_YEAR]) * 10000);
}

/* Returns the current RTC date and time according to UTC.
 * Parameters: 	none
 * Return value: date as uint64 with a decimal number formated as YYYYmmddHHMMSS.
 */
uint64_t rtc_get_datetime(void) {
	return ((uint64_t)rtc_get_date()) * 1000000 + (uint64_t)rtc_get_time();
}


uint32_t rtc_get_epoch_time(void)
{
	return rtc_epoch_time;
}


//RetVal rtc_backup_reg_set(uint8_t id, uint8_t val)
//{
//	/* Writes one byte to the RTC's buffered registers. The register number is specified with the parameter id.
//	 * Parameters: 	uint8_t id - Register number to write
//	 * 				uint8_t val - Value written to the register
//	 * Return value: SUCCESS/ERROR
//	 */
//
//	uint8_t regs[20] =
//	{ };
//
//	if (id >= 19)
//	{
//		/* Register does not exist or is protected. */
//		return FAILED;
//	}
//
//	/* Prevent all other tasks and interrupts from altering the register contents */
//	taskDISABLE_INTERRUPTS();
//
//	/* Read all registers */
//	*((uint32_t *) &regs[0]) = RTC_ReadGPREG(LPC_RTC, 0);
//	*((uint32_t *) &regs[4]) = RTC_ReadGPREG(LPC_RTC, 1);
//	*((uint32_t *) &regs[8]) = RTC_ReadGPREG(LPC_RTC, 2);
//	*((uint32_t *) &regs[12]) = RTC_ReadGPREG(LPC_RTC, 3);
//	*((uint32_t *) &regs[16]) = RTC_ReadGPREG(LPC_RTC, 4);
//
//	/* Set given value */
//	regs[id] = val;
//
//	/* Calculate and store new checksum */
//	regs[19] = rtc_checksum_calc(&regs[0]);
//
//	/* Store values */
//	RTC_WriteGPREG(LPC_RTC, 0, *((uint32_t *) (&regs[0])));
//	RTC_WriteGPREG(LPC_RTC, 1, *((uint32_t *) (&regs[4])));
//	RTC_WriteGPREG(LPC_RTC, 2, *((uint32_t *) (&regs[8])));
//	RTC_WriteGPREG(LPC_RTC, 3, *((uint32_t *) (&regs[12])));
//	RTC_WriteGPREG(LPC_RTC, 4, *((uint32_t *) (&regs[16])));
//
//	taskENABLE_INTERRUPTS();
//
//	return DONE;
//}

//RetVal rtc_backup_reg_reset(uint8_t go)
//{
//	/* Resets the backup registers and calculates the checksum
//	 *
//	 */
//
//	if (go == 0)
//	{
//		return FAILED;
//	}
//
//	uint8_t regs[20] =
//	{ };
//
//	/* Prevent all other tasks and interrupts from altering the register contents */
//	taskDISABLE_INTERRUPTS();
//
//	/* Calculate and store new checksum */
//	regs[19] = rtc_checksum_calc(&regs[0]);
//
//	/* Store values */
//	RTC_WriteGPREG(LPC_RTC, 0, *((uint32_t *) (&regs[0])));
//	RTC_WriteGPREG(LPC_RTC, 1, *((uint32_t *) (&regs[4])));
//	RTC_WriteGPREG(LPC_RTC, 2, *((uint32_t *) (&regs[8])));
//	RTC_WriteGPREG(LPC_RTC, 3, *((uint32_t *) (&regs[12])));
//	RTC_WriteGPREG(LPC_RTC, 4, *((uint32_t *) (&regs[16])));
//
//	taskENABLE_INTERRUPTS();
//
//	return DONE;
//}

//RetVal rtc_backup_reg_read(uint8_t id, uint8_t * val)
//{
//	/* Reads one byte from the RTC's buffered registers. The register number is specified with the parameter id.
//	 * Parameters: 	uint8_t id - Register number to read from
//	 * 				uint8_t * - pointer where value shall be stored
//	 * Return value: 0 in case of success, else (wrong checksum) != 0
//	 * ID
//	 */
//
//	/* Read all registers */
//
//	uint8_t cs;
//	uint8_t regs[20] =
//	{ };
//
//	if (id > 19)
//	{
//		/* Register does not exist */
//		return FAILED;
//	}
//
//	taskDISABLE_INTERRUPTS();
//
//	*((uint32_t *) &regs[0]) = RTC_ReadGPREG(LPC_RTC, 0);
//	*((uint32_t *) &regs[4]) = RTC_ReadGPREG(LPC_RTC, 1);
//	*((uint32_t *) &regs[8]) = RTC_ReadGPREG(LPC_RTC, 2);
//	*((uint32_t *) &regs[12]) = RTC_ReadGPREG(LPC_RTC, 3);
//	*((uint32_t *) &regs[16]) = RTC_ReadGPREG(LPC_RTC, 4);
//
//	/* Calculate checksum */
//	cs = rtc_checksum_calc(&regs[0]);
//
//	taskENABLE_INTERRUPTS();
//
//	/* Compare checksums */
//	if (cs != regs[19])
//	{
//		/* Checksum is not correct - values may be corrupted */
//		return FAILED;
//	}
//
//	*val = regs[id];
//
//	return DONE;
//}


//RetVal rtc_check_if_reset(void)
//{
//	/**
//	 * Check if the RTC module was reseted and therefore time and register values are invalid.
//	 */
//
//	uint8_t cs;
//	uint8_t regs[20] =
//	{ };
//
//	taskDISABLE_INTERRUPTS();
//
//	*((uint32_t *) &regs[0]) = RTC_ReadGPREG(LPC_RTC, 0);
//	*((uint32_t *) &regs[4]) = RTC_ReadGPREG(LPC_RTC, 1);
//	*((uint32_t *) &regs[8]) = RTC_ReadGPREG(LPC_RTC, 2);
//	*((uint32_t *) &regs[12]) = RTC_ReadGPREG(LPC_RTC, 3);
//	*((uint32_t *) &regs[16]) = RTC_ReadGPREG(LPC_RTC, 4);
//
//	/* Calculate checksum */
//	cs = rtc_checksum_calc(&regs[0]);
//
//	taskENABLE_INTERRUPTS();
//
//	/* Compare checksums */
//	if (cs != regs[19])
//	{
//#if EXTENDED_DEBUG_MESSAGES
//		printf("OBC: RTC: Was reset.\n");
//#endif
//		/* Checksum is not correct - values may be corrupted */
//
//		return FAILED;
//	}
//
//	return DONE;
//}

//uint8_t rtc_checksum_calc(uint8_t *data)
//{
//	/* Add 1 to ensure 0 for all data entries gives a cs != 0 */
//	return (CRC8(data, 19) + 1);
//}

void RtcMain(void) {
	if (rtc_dayChanged) {
		rtc_dayChanged = false;
		if (rtc_calibration_base == 0) {
			rtc_calibration_base = rtc_dayChangedAtSeconds;
			rtc_calibration_day = 1;
			printf("RTC Calibration initialized with %ld on day 1\n", rtc_dayChangedAtSeconds);
		} else {
			uint32_t secondsTicked =  rtc_dayChangedAtSeconds - rtc_calibration_base;
			uint32_t secondsPerDay = secondsTicked/rtc_calibration_day;
			printf("RTC Calibration end of day %d. Seconds ticked: %ld average per day: %ld\n", rtc_calibration_day, secondsTicked, secondsPerDay );
			rtc_calibration_day++;
		}
	}
}

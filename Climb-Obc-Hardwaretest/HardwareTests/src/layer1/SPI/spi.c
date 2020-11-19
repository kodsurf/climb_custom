/**
 * @file stc_spi.c
 *
 * @date Created on: 07.10.2012
 * @author Andreas Sinn <andreas.sinn@spaceteam.at>
 */

// ===== INCLUDES =====
#include <stdio.h>

#include "spi.h"

#define SPI_SCK_PORT	0
#define SPI_SCK_PIN		15

#define SPI_MOSI_PORT	0
#define SPI_MOSI_PIN	18

#define SPI_MISO_PORT	0
#define SPI_MISO_PIN	17

#define SPI_INTERRUPT_PRIORITY 5		//TODO check with all other prios??
#define SPI_JOBQUEUE_SIZE	  16

typedef struct spi_job_s
{
    uint8_t *txbuffer;
    uint8_t bytes_to_send;
    uint8_t bytes_sent;
    uint8_t *array_to_store;
    uint8_t bytes_to_read;
    uint8_t bytes_read;
    void(*chipSelectHandler)(bool select);
}
spi_job_t;

typedef struct spi_jobs_s
{
    spi_job_t job[SPI_JOBQUEUE_SIZE];
    uint8_t current_job;
    uint8_t last_job_added;
    uint8_t jobs_pending;
}
spi_jobs_t;



// ===== global variables =====
//uint8_t SPI_TX_BUF[SPI_BUFFER_SIZE];
//nt8_t SPI_RX_BUF[SPI_BUFFER_SIZE];
// SPI_CFG_Type spiInitialization;

spi_jobs_t spi_jobs;

//uint16_t pressure_calib[8];

bool spi_initialized;
bool spi_error_occured;
bool spi_cmd_not_sent;
bool spi_job_buffer_overflow;


uint8_t spi_getJobsPending() {
	return spi_jobs.jobs_pending;
}

void spi_init(void)
{

	/* --- SPI pins --- */
	Chip_IOCON_PinMuxSet(LPC_IOCON, SPI_SCK_PORT, SPI_SCK_PIN, IOCON_FUNC3 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, SPI_SCK_PORT, SPI_SCK_PIN);

	Chip_IOCON_PinMuxSet(LPC_IOCON, SPI_MOSI_PORT, SPI_MOSI_PIN, IOCON_FUNC3 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, SPI_MOSI_PORT, SPI_MOSI_PIN);

	Chip_IOCON_PinMuxSet(LPC_IOCON, SPI_MISO_PORT, SPI_MISO_PIN, IOCON_FUNC3 | IOCON_MODE_INACT);
	Chip_IOCON_DisableOD(LPC_IOCON, SPI_MISO_PORT, SPI_MISO_PIN);


    // WARNING: Exchange needed for SSP !!!
    // SPI Configuration for CPHA=1, CPOL=1, f=5MHz (Maximum of ADXL345), MSB First and 8 data bits
//    SPI_ConfigStructInit(&spiInitialization);
//
//    // Initialize SPI peripheral with parameter given in structure above
//    spiInitialization.CPHA = SPI_CPHA_SECOND;						// SPI_CPHA_SECOND
//    spiInitialization.CPOL = SPI_CPOL_LO;							// SPI_CPOL_LO
//    spiInitialization.ClockRate = 500000;							// ClockRate, max. 5MHz
//    spiInitialization.DataOrder = SPI_DATA_MSB_FIRST;				// SPI_DATA_MSB_FIRST
//    spiInitialization.Databit = SPI_DATABIT_8;						// SPI_DATABIT_8
//    spiInitialization.Mode = SPI_MASTER_MODE;						// SPI_MASTER_MODE

    Chip_SPI_Init(LPC_SPI); 		// All default values as above, Bitrate is set to 4000000 with this!
    Chip_SPI_Int_Enable(LPC_SPI);
    NVIC_SetPriority(SPI_IRQn, SPI_INTERRUPT_PRIORITY);
    NVIC_EnableIRQ(SPI_IRQn);

    spi_initialized = 1;
    return;
}

void SPI_IRQHandler(void)
{
	LPC_SPI->INT = SPI_INT_SPIF;			// Reset interrupt flag

    if (spi_jobs.jobs_pending == 0)
    {
        return;     // No pending jobs
    }

    if (LPC_SPI->SR & SPI_ERROR_MASK)
    {     // Check SPI status register for errors
        spi_error_occured = 1;
        return;
    }

    uint8_t temp = (uint8_t) LPC_SPI->DR;     // Read SPI buffer

    if (spi_jobs.job[spi_jobs.current_job].bytes_to_send > spi_jobs.job[spi_jobs.current_job].bytes_sent) {
    	// TX ongoing
    	uint8_t txIdx = spi_jobs.job[spi_jobs.current_job].bytes_sent;
    	uint8_t data = spi_jobs.job[spi_jobs.current_job].txbuffer[txIdx];
    	Chip_SPI_SendFrame(LPC_SPI, data);
    	spi_jobs.job[spi_jobs.current_job].bytes_sent++;

    } else if (spi_jobs.job[spi_jobs.current_job].bytes_read == 0 && spi_jobs.job[spi_jobs.current_job].bytes_to_read != 0)
    {     // spi_jobs.job[spi_jobs.current_job].bytes_to_read)
//        if (spi_jobs.job[spi_jobs.current_job].cmd_sent == 0)
//        {     // Send command to sensor/periphery
//            spi_cmd_not_sent = 1;
//        }
        LPC_SPI->DR = 0xFF;     // Send dummy data while readout
        spi_jobs.job[spi_jobs.current_job].bytes_read++;
    }
    else if (spi_jobs.job[spi_jobs.current_job].bytes_read == spi_jobs.job[spi_jobs.current_job].bytes_to_read)
    {
    	// RX finished
    	if (spi_jobs.job[spi_jobs.current_job].bytes_read  > 0) {

    		spi_jobs.job[spi_jobs.current_job].array_to_store[spi_jobs.job[spi_jobs.current_job].bytes_read - 1] = temp;
    	}

        // Chip unselect sensor
        spi_jobs.job[spi_jobs.current_job].chipSelectHandler(false);

        spi_jobs.current_job++;
        spi_jobs.jobs_pending--;

        if (spi_jobs.current_job == SPI_MAX_JOBS)
        {
            spi_jobs.current_job = 0;
        }



        if (spi_jobs.jobs_pending > 0)
        {	// Check if jobs pending
            // Chip select sensor
        	spi_jobs.job[spi_jobs.current_job].chipSelectHandler(true);
        	//Chip_GPIO_SetPinState(LPC_GPIO, FLOGA_CS_PORT, FLOGA_CS_PIN, false);

            LPC_SPI->DR = spi_jobs.job[spi_jobs.current_job].txbuffer[0];
            spi_jobs.job[spi_jobs.current_job].bytes_sent = 1;
        }
    }
    else
    {
        spi_jobs.job[spi_jobs.current_job].array_to_store[spi_jobs.job[spi_jobs.current_job].bytes_read - 1] = temp;
        LPC_SPI->DR = 0xFF;     // Send dummy data for readout
        spi_jobs.job[spi_jobs.current_job].bytes_read++;
    }

    return;
}

bool spi_add_job( void(*chipSelectCallback)(bool select),
		           uint8_t* txpTr, uint8_t bytes_to_write,
				   uint8_t *array_to_store, uint8_t bytes_to_read )
{
    if (spi_jobs.jobs_pending >= SPI_MAX_JOBS)
    {	// Maximum amount of jobs stored, job can't be added!
        spi_job_buffer_overflow = 1;
        spi_jobs.jobs_pending = 0;	// Delete jobs
        return false;
    }

    NVIC_DisableIRQ(SPI_IRQn);

    uint8_t position = (spi_jobs.current_job + spi_jobs.jobs_pending) % SPI_MAX_JOBS;

    spi_jobs.job[position].txbuffer = txpTr;
    spi_jobs.job[position].bytes_to_send = bytes_to_write;
    spi_jobs.job[position].array_to_store = array_to_store;
    spi_jobs.job[position].bytes_to_read = bytes_to_read;
    spi_jobs.job[position].chipSelectHandler = chipSelectCallback;
    spi_jobs.job[position].bytes_read = 0;
    spi_jobs.job[position].bytes_sent = 0;

    if (spi_jobs.jobs_pending == 0)
    {	// Check if jobs pending
    	 // Chip select sensor
    	spi_jobs.job[position].chipSelectHandler(true);
    	//Chip_GPIO_SetPinState(LPC_GPIO, FLOGA_CS_PORT, FLOGA_CS_PIN, false);
        LPC_SPI->DR = spi_jobs.job[position].txbuffer[0];
        spi_jobs.job[position].bytes_sent = 1;
    }

    spi_jobs.jobs_pending++;

    NVIC_EnableIRQ(SPI_IRQn); // Beta

    return true;
}



//
//bool hig_read_values(void)
//{
//    return spi_add_job(ADXL375, 0xF2, sizeof(data.block_1000hz.body.high_acc), (uint8_t*) &data.block_1000hz.body.high_acc);	// for reasons unknow 0xF2 instead of 0xB2
//}
//
//bool gyro_read_values(void)
//{
//    return spi_add_job(L3G4200D, 0xE8, sizeof(data.block_1000hz.body.gyro), (uint8_t*)&data.block_1000hz.body.gyro);
//}
//
//bool gyro_read_temperature(void)
//{
//    return spi_add_job(L3G4200D, 0xA6, 2, (uint8_t*) &data.block_1hz.body.gyro_temperature);	//TODO: why reading 2 bytes, as gyro_temperature is only 1byte in size
//}
//
//bool pressure_read_values(void)
//{
//    static uint8_t current_measurement = 0;     // Converted value stored in Sensor's RAM, 0... pressure, 1...temperature
//    static uint16_t counter = 0;
//    static uint8_t dummy_buffer[6];
//    counter++;
//
//    if (counter % 25 == 0)
//    {
//        spi_add_job(MS5611, 0x00, 3, (uint8_t*) &data.block_100hz.body.pressure);     // Read pressure //TODO: why only 3 bytes, as pressure is 4 bytes in size
//        spi_add_job(MS5611, 0x58, 0, (uint8_t*) &dummy_buffer[0]);     // 0x58 Start conversation of temperature  with OSR=4096 (highest Resolution)
//        current_measurement = 1;
//    }
//    else
//    {
//        if (current_measurement)
//        {     // Dummy buffer erstellen
//            spi_add_job(MS5611, 0x00, 3, (uint8_t*) &data.block_1hz.body.pressure_temperature);     // Read temperature //TODO: why only 3 bytes, as pressure_temperature is 4 bytes in size
//        }
//        else
//        {
//            spi_add_job(MS5611, 0x00, 3, (uint8_t*) &data.block_100hz.body.pressure);     // Read pressure  //TODO: why only 3 bytes, as pressure is 4 bytes in size
//        }
//
//        spi_add_job(MS5611, 0x48, 0, &dummy_buffer[0]);     // 0x48 Start conversation of pressure with OSR=4096 (highest Resolution)
//        current_measurement = 0;
//    }
//    return 0;
//}
//
//bool pressure_init(void)
//{
//    // Digital pressure sensor MS5611
//    // 24 bit pressure, 24 bit temperature
//
//    SPI_DATA_SETUP_Type xferConfig;
//    uint8_t i, j = 0;
//
//    // --- Reset sensor ---
//    SPI_TX_BUF[0] = 0x1E;     // Reset command
//
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.length = 1;
//    xferConfig.rx_data = NULL;
//
//    pressure_select();
//
//    SPI_Write(LPC_SPI, &xferConfig);
//    delay_ms(5);
//
//    pressure_unselect();
//
//    delay_ms(50);     // Wait for reset complete
//
//    for (j = 0; j <= 7; j++)
//    {     // Read data for calibration+CRC
//        for (i = 0; i < 4; i++)
//        {     // Clear buffers
//            SPI_TX_BUF[i] = 0;
//            SPI_RX_BUF[i] = 0;
//        }
//
//        SPI_TX_BUF[0] = 0xA0 + 2 * j;     // Read PROM data
//        xferConfig.tx_data = &SPI_TX_BUF[0];
//        xferConfig.rx_data = &SPI_RX_BUF[0];
//
//        pressure_select();
//        xferConfig.length = 1;
//        SPI_Write(LPC_SPI, &xferConfig);
//        xferConfig.length = 3;
//
//        SPI_Read(LPC_SPI, &xferConfig);
//        pressure_unselect();
//
//        pressure_calib[j] = (SPI_RX_BUF[0] << 8) | SPI_RX_BUF[1];     // Store data
//
//    }
//
//    // Check factory setting
//    //if (pressure_calib[0] != 80)
//    //return 1;
//
//
//    if ( crc4(&pressure_calib[0]) != (pressure_calib[7] & 0x000F) )
//        return 1; // failed
//
//    SPI_TX_BUF[0] = 0x48;     // 0x48 Start conversation of pressure with OSR=4096 (highest Resolution)
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.length = 1;
//
//    pressure_select();
//    SPI_Write(LPC_SPI, &xferConfig);
//    pressure_unselect();
//
//    status.main.body.sensor_pressure_initialized = 1;
//    return 0;
//}
//
//void pressure_read_values_polling(void)
//{
//    SPI_DATA_SETUP_Type xferConfig;
//    static bool current_measurement = 0;     // Converted value stored in Sensor's RAM, 0... pressure, 1...temp
//
//    uint32_t i = 0;
//    for (i = 0; i < 8; i++)
//    {
//        SPI_TX_BUF[i] = 0;
//        SPI_RX_BUF[i] = 0;
//    }
//
//    // --- Get 24 bit ADC value from last conversion ---
//    SPI_TX_BUF[0] = 0x00;     // ADC Read
//
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.rx_data = &SPI_RX_BUF[0];
//    xferConfig.length = 1;
//
//    pressure_select();
//    SPI_Write(LPC_SPI, &xferConfig);
//    xferConfig.length = 3;
//    SPI_Read(LPC_SPI, &xferConfig);
//    pressure_unselect();
//
//    //Start next conversation
//    if (current_measurement)
//    {
//        data.block_1hz.body.pressure_temperature = (SPI_RX_BUF[0] << 16) | (SPI_RX_BUF[1] << 8) | SPI_RX_BUF[2];
//        SPI_TX_BUF[0] = 0x48;     // 0x48 Start conversation of pressure with OSR=4096 (highest Resolution)
//        current_measurement = 0;
//    }
//    else
//    {
//        data.block_100hz.body.pressure = (SPI_RX_BUF[0] << 16) | (SPI_RX_BUF[1] << 8) | SPI_RX_BUF[2];
//        SPI_TX_BUF[0] = 0x58;     // 0x58 Start conversation of temperature  with OSR=4096 (highest Resolution)
//        current_measurement = 1;
//    }
//
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.length = 1;
//
//    pressure_select();
//    SPI_Write(LPC_SPI, &xferConfig);
//    pressure_unselect();
//
//    return;
//}
//
//bool hig_init(void)
//{
//    // initialize ADXL375 on SPI
//    // 13 bit resolution, up to 3200Hz measuring rate, +/-200g -> 0.04883 g/digit
//
//    SPI_DATA_SETUP_Type xferConfig;
//
//    uint32_t i = 0;
//    for (i = 0; i < 8; i++)
//    {
//        SPI_TX_BUF[i] = 0;
//        SPI_RX_BUF[i] = 0;
//    }
//
//    // --- read device ID ---
//    SPI_TX_BUF[0] = 0x80;		// device ID register (0x00 & MSB = 1 for reading)
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.rx_data = &SPI_RX_BUF[0];
//    SPI_RX_BUF[0] = 0;
//
//    xferConfig.length = 1;
//
//    hig_select();
//    SPI_Write(LPC_SPI, &xferConfig);
//    SPI_Read(LPC_SPI, &xferConfig);
//    hig_unselect();
//
//    if (SPI_RX_BUF[0] != 0xE5)	// return if device ID is incorrect
//        return 1;
//
//    // --- set FIFO-mode ---
//    SPI_TX_BUF[0] = 0x38;		// FIFO control register
//    SPI_TX_BUF[1] = 0x0F;		// no FIFO is used	// 0x8F stream-FIFO, 32 samples till interrupt
//
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.rx_data = NULL;
//    xferConfig.length = 2;
//
//    hig_select();
//    SPI_Write(LPC_SPI, &xferConfig);
//    hig_unselect();
//
//    // --- set measurement rate ---
//    SPI_TX_BUF[0] = 0x2C;		// data rate and power mode control register
//    SPI_TX_BUF[1] = 0x0D;		// 0x0D=800Hz	// 0x0F=3200Hz
//
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.rx_data = NULL;
//    xferConfig.length = 2;
//
//    hig_select();
//    SPI_Write(LPC_SPI, &xferConfig);
//    hig_unselect();
//
//    // --- set data format ---
//    SPI_TX_BUF[0] = 0x31;		// data format control register
//    SPI_TX_BUF[1] = 0x0B;		// no self-test, 4-wire-SPI, right justified mode
//
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.rx_data = NULL;
//    xferConfig.length = 2;
//
//    hig_select();
//    SPI_Write(LPC_SPI, &xferConfig);
//    hig_unselect();
//
//    // --- set interrupts ---
//    SPI_TX_BUF[0] = 0x2E;		// interrupt enable control register
//    SPI_TX_BUF[1] = 0x00;		// disable all interrupts
//
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.rx_data = NULL;
//    xferConfig.length = 2;
//
//    hig_select();
//    SPI_Write(LPC_SPI, &xferConfig);
//    hig_unselect();
//
//    // --- enable measurement ---
//    SPI_TX_BUF[0] = 0x2D;		// power saving features control register
//    SPI_TX_BUF[1] = 0x08;		// measurement mode, no auto-sleep or sleep	// 0x00 standby mode
//
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.rx_data = NULL;
//    xferConfig.length = 2;
//
//    hig_select();
//    SPI_Write(LPC_SPI, &xferConfig);
//    hig_unselect();
//
//    status.main.body.sensor_hig_initialized = 1;
//    return 0;
//}
//
//void hig_deinit(void)
//{
//    SPI_DATA_SETUP_Type xferConfig;
//
//    SPI_TX_BUF[0] = 0x2D;		// power saving features control register
//    SPI_TX_BUF[1] = 0x00;		// standby mode
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.rx_data = NULL;
//    xferConfig.length = 2;
//
//    hig_select();
//    SPI_ReadWrite(LPC_SPI, &xferConfig, SPI_TRANSFER_POLLING);
//    hig_unselect();
//
//    status.main.body.sensor_hig_initialized = 0;
//}
//
//void hig_read_values_polling(void)
//{
//    SPI_DATA_SETUP_Type xferConfig;
//
//    int i;
//    for (i = 0; i < 8; i++)
//    {
//        SPI_TX_BUF[i] = 0;
//        SPI_RX_BUF[i] = 0;
//    }
//
//    SPI_TX_BUF[0] = 0xB2;		// DATAX0 register (0x32 & MSB = 1 for reading)
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.rx_data = &SPI_RX_BUF[0];
//    xferConfig.length = 1;
//
//    hig_select();
//    SPI_Write(LPC_SPI, &xferConfig);
//    xferConfig.length = 6;		// read DATAX0 and the 5 subsequent registers (till DATAZ1)
//    SPI_Read(LPC_SPI, &xferConfig);
//    hig_unselect();
//
//    data.block_1000hz.body.high_acc.x = (int16_t) ((SPI_RX_BUF[1] << 8) | SPI_RX_BUF[0]);     // Value in g = register value * 0.04883
//    data.block_1000hz.body.high_acc.z = (int16_t) ((SPI_RX_BUF[3] << 8) | SPI_RX_BUF[2]);		// the default coordinate system of the sensor
//    data.block_1000hz.body.high_acc.y = (int16_t) ((SPI_RX_BUF[5] << 8) | SPI_RX_BUF[4]);		// is left handed -> change y and z -> right handed
//
//    return;
//}
//
//bool gyro_init(void)
//{
//    SPI_DATA_SETUP_Type xferConfig;
//
//    // --- Read device ID ---
//    SPI_TX_BUF[0] = 0x8F;     // Who am I - read device ID from 0x0F
//
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.rx_data = &SPI_RX_BUF[0];
//    xferConfig.length = 1;
//
//    gyro_select();
//    SPI_Write(LPC_SPI, &xferConfig);
//    SPI_Read(LPC_SPI, &xferConfig);
//    gyro_unselect();
//
//    if (SPI_RX_BUF[0] != 0xD3)
//    {     // Check who am I - register
//        return 1;     // Read ID is wrong -> Sensor or interface probably not functional
//    }
//
//    // --- Control register 1 ---
//    SPI_TX_BUF[0] = 0x20;     // 0x20 Control register 1
//    SPI_TX_BUF[1] = 0xFF;     // 0xFF All axis on at 800Hz, BW 110Hz speed
//
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.length = 2;
//
//    gyro_select();
//    SPI_Write(LPC_SPI, &xferConfig);
//    gyro_unselect();
//
//    // --- Control register 2 ---
//    SPI_TX_BUF[0] = 0x21;     // 0x21 Control register 2
//    SPI_TX_BUF[1] = 0x28;     // 0x28 High pass filter 0.2Hz @ 800Hz data rate
//
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.length = 2;
//
//    gyro_select();
//    SPI_Write(LPC_SPI, &xferConfig);
//    gyro_unselect();
//
//    // ---  Control register 4 ---
//    SPI_TX_BUF[0] = 0x23;     // 0x23 Control register 4
//    SPI_TX_BUF[1] = 0xB0;     // 0x30 2000dps, selftest disabled, block data update
//
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.length = 2;
//
//    gyro_select();
//    SPI_Write(LPC_SPI, &xferConfig);
//    gyro_unselect();
//
//    // --- Control register 5 ---
//    SPI_TX_BUF[0] = 0x24;     // 0x24 Control register 5
//    SPI_TX_BUF[1] = 0x02;     // 0x12 High pass enabled, Data is HP and LP filtered
//    //SPI_TX_BUF[1] = 0x12;     // 0x12 High pass enabled, Data is HP and LP filtered
//
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.length = 2;
//
//    gyro_select();
//    SPI_Write(LPC_SPI, &xferConfig);
//    gyro_unselect();
//
//    status.main.body.sensor_gyro_initialized = 1;
//    return 0;     // Kein Fehler aufgetreten
//}
//
//void gyro_deinit(void)
//{
    //SPI_DATA_SETUP_Type xferConfig;
//
//    SPI_TX_BUF[0] = 0x20;     // 0x20 Adress of the config register
//    SPI_TX_BUF[1] = 0x00;     // 0x00
//
//    gyro_select();
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.rx_data = &SPI_RX_BUF[0];
//    xferConfig.length = 2;
//    SPI_ReadWrite(LPC_SPI, &xferConfig, SPI_TRANSFER_POLLING);
//    gyro_unselect();
//
//    status.main.body.sensor_gyro_initialized = 0;
//    return;
//}
//
//void gyro_read_values_polling(void)
//{
//    // Readout Gyroscope
//    // L3G4200D, 70mdps/digit @ 800Hz
//    SPI_DATA_SETUP_Type xferConfig;
//
//    int i;
//    for (i = 0; i < 6; i++)
//    {
//        SPI_TX_BUF[i] = 0x00;
//        SPI_RX_BUF[i] = 0x00;
//    }
//
//    SPI_TX_BUF[0] = 0xE8;     // Read data from 0x28 to 0x2D, Read Bit(7) set, multiple read bit (6) set
//    // 1110 1000, READ, M/S, AD(5:0): 28h
//
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.rx_data = &SPI_RX_BUF[0];
//    xferConfig.length = 1;
//
//    gyro_select();
//    SPI_Write(LPC_SPI, &xferConfig);
//    xferConfig.length = 6;
//    SPI_Read(LPC_SPI, &xferConfig);
//    gyro_unselect();
//
//    //data.gyro_x = SPI_RX_BUF[0] | (SPI_RX_BUF[1] << 8);
//    //data.gyro_y = SPI_RX_BUF[2] | (SPI_RX_BUF[3] << 8);
//    //data.gyro_z = SPI_RX_BUF[4] | (SPI_RX_BUF[5] << 8);
//
//}
//
//void gyro_read_temperature_polling(void)
//{
//    SPI_DATA_SETUP_Type xferConfig;
//
//    SPI_TX_BUF[0] = 0xA6;     //	0x26 temperature register, single read
//
//    xferConfig.tx_data = &SPI_TX_BUF[0];
//    xferConfig.rx_data = &SPI_RX_BUF[0];
//    xferConfig.length = 1;
//
//    gyro_select();
//    SPI_Write(LPC_SPI, &xferConfig);
//    SPI_Read(LPC_SPI, &xferConfig);
//    gyro_unselect();
//
//    //data.gyro_temp = SPI_RX_BUF[0];		// Temperatur
//    //data.gyro_status = SPI_RX_BUF[1]; 	// Status - disabled
//    return;
//}
//
////********************************************************
////! @brief calculate the CRC code for details look into CRC CODE NOTES, AN520 C-code example for MS56xx
////!
////! @return crc code
////********************************************************
//unsigned char crc4(uint16_t n_prom[])
//{
//    int cnt;		// simple counter
//    unsigned int n_rem;		// crc reminder
//    unsigned int crc_read;		// original value of the crc
//    unsigned char  n_bit;
//    n_rem = 0x00;
//    crc_read=n_prom[7];		//save read CRC
//    n_prom[7]=(0xFF00 & (n_prom[7]));		//CRC byte is replaced by 0
//    for (cnt = 0; cnt < 16; cnt++)		// operation is performed on bytes
//
//    { // choose LSB or MSB
//        if (cnt%2==1)
//            n_rem ^= (unsigned short) ((n_prom[cnt>>1]) & 0x00FF);
//        else
//            n_rem ^= (unsigned short) (n_prom[cnt>>1]>>8);
//        for (n_bit = 8; n_bit > 0; n_bit--)
//        {
//            if (n_rem & (0x8000))
//            {
//                n_rem = (n_rem << 1) ^ 0x3000;
//            }
//            else
//            {
//                n_rem = (n_rem << 1);
//            }
//        }
//    }
//    n_rem= (0x000F & (n_rem >> 12));// // final 4-bit reminder is CRC code
//    n_prom[7]=crc_read; // restore the crc_read to its original place
//    return (n_rem ^ 0x00);
//}
//
//void gyro_select(void)
//{
//    GPIO_ClearValue(GYRO_CS_PORT, (1 << GYRO_CS_PIN));
//}
//void gyro_unselect(void)
//{
//    GPIO_SetValue(GYRO_CS_PORT, (1 << GYRO_CS_PIN));
//}
//void hig_select(void)
//{
//    GPIO_ClearValue(HIGH_G_CS_PORT, 1 << HIGH_G_CS_PIN);
//}
//void hig_unselect(void)
//{
//    GPIO_SetValue(HIGH_G_CS_PORT, 1 << HIGH_G_CS_PIN);
//}
//void pressure_select(void)
//{
//    GPIO_ClearValue(PRESSURE_CS_PORT, 1 << PRESSURE_CS_PIN);
//}
//void pressure_unselect(void)
//{
//    GPIO_SetValue(PRESSURE_CS_PORT, 1 << PRESSURE_CS_PIN);
//}

void DumpSpiJobs(void) {
	printf("spijobs cur: %d, pen: %d, add: %d\n", spi_jobs.current_job, spi_jobs.jobs_pending, spi_jobs.last_job_added );
	for (int i= 0; i<SPI_JOBQUEUE_SIZE; i++) {
		printf("%s[%d]: ", ((i==spi_jobs.current_job)?"c->":((i==spi_jobs.last_job_added)?"+->":"   ")),i);
		printf("tx: %d/%d rx: %d/%d\n",  spi_jobs.job[i].bytes_sent, spi_jobs.job[i].bytes_to_send, spi_jobs.job[i].bytes_read, spi_jobs.job[i].bytes_to_read );
	}

}


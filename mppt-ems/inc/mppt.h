                                                                                                                                                                                                                    /** mppt.c
 * Top level header file for MPPT EMS (STI assembly number 781-124-033 rev. 0)
 *
 * (c) 2018 Solar Technology Inc.
 * 7620 Cetronia Road
 * Allentown PA, 18106
 * 610-391-8600
 *
 * This code is for the exclusive use of Solar Technology Inc.
 * and cannot be used in its present or any other modified form
 * without prior written authorization.
 *
 * HOST PROCESSOR: STM32F410RBT6
 * Developed using STM32CubeF4 HAL and API version 1.18.0
 *
 *
 *
 * REVISION HISTORY
 *
 * 1.0: 12/27/2017	Created By Nicholas C. Ipri (NCI) nipri@solartechnology.com
 *
 */
#ifndef MPPT_H_
#define MPPT_H_

// voltage related constants
// The ones that are commented out are not used in rev 4.3
#define TENTH_VOLT			0x0f
#define QUARTER_VOLT		0x27	// 39 counts = 0.25 volts
#define HALF_VOLT      		0x4d	// 77 counts = 0.5 V
#define ONE_VOLT			0x9b	// 155 counts
#define TWO_VOLT       		0x135	// 309 counts = 2.0 Volts
#define ONE_AND_HALF_VOLT 	0xe8	// 232 counts = 1.5 Volts
#define LOW_BATTERY2   		0x695	// 1685 counts = 10.9 Volts
//#define LOW_BATTERY1   		0x6a5	// 1701 counts = 11.0 Volts
#define V_MIN_LOAD_ON       0x73f	// 1855 counts = 12 Volts
#define V_MAX_LOAD_ON       0x93d	// 2365 counts = 15.3 Volts
#define V_MIN_LOAD_OFF 		0x676	// 1654 counts = 10.7 Volts
#define V_MAX_LOAD_OFF 		0x95c	// 2396 counts = 15.5 Volts
#define MAX_START_VOLT 		0x7da	// 2010 counts = 13.0 Volts
#define BAT_DROP_DEAD_VOLT	0x4d5	// 1237 counts = 8 Volts: the minimum battery voltage below which, we will not charge and warn the user.

// this is the range of voltages that we want across the solar array and are a function of duty cycle and battery voltage
//#define MAX_PV_VOLT			0xc14	// 3092 counts corresponding to 20.0 Volts
#define MAX_PV_VOLT			0xcae	// 3246 counts corresponding to 21.0 Volts
#define MIN_PV_VOLT			0x90f	// 2319 counts corresponding to 15.0 Volts

//#define THRESHOLD_CURRENT	0x1F	// 31 counts corresponding t0 250 mA
#define THRESHOLD_CURRENT	0x31	// 49 counts corresponding t0 394 mA
#define MAX_CHARGE_CURRENT	0xEBA	// 3722 counts = 30 amps


// Adsorption Threshold voltage values at temperature (flooded)
#define ATV_25        		0x8b2	// 2226 counts = 14.4v
#define ATV_40      		0x870	// 2160 counts = 13.97v
#define ATV_NEG30			0x9b1	// 2497 counts = 16.05v
#define ATV_80				0x7b9	// 1977 counts = 12.75v

// Float Threshold voltage values at temperature (flooded)
#define FTV_25        		0x827	// 2087 counts = 13.5v
#define FTV_40      		0x7E3	// 2019 counts = 13.06v
#define FTV_NEG30			0x8fb	// 2299 counts = 14.87v
#define FTV_80				0x753	// 1875 counts = 12.13v


//the two rates given below are in counts, and are 10 times larger than actual.
//#define RATE1       		54		// TV rate between 0C and 40C (-35 mV/degree)
#define RATE1				46		// TV rate between -30C and 40C (-30 mV/degree)
#define RATE2       		15		// TV rate above 40C (-10 mV/degree)

#define MSRATE				45		// -29 mV/degC... almost identical to RATE2 so we'll just use RATE2 for now



#define TEMP_0      		0xd3d	// 3389 count corrensponding to 0 C
#define TEMP_40	    		0xf2d	// 3885 count corrensponding to 40 C
#define TEMP_NEG30			0xbc9	// 3017 counts = -30 C

/* This is the MOSFET temperatures at which the fan is switched on or off. Change as necessary */
#define FAN_ON_TEMP			0xfa9 	// 4009 counts for 50 C / 122 deg F
#define FAN_OFF_TEMP		0xf14	// 3860 counts for 38 C / 100.4 deg F

#define MAX_TEMP

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);
void HD44780_Init(void);
//void HD44780_WriteData(uint8_t row, uint8_t col, char *data);
void HD44780_WriteData(uint8_t, uint8_t, char *, uint8_t);
void HD44780_WriteCommand(uint8_t);
void HD44780_GotoXY(uint8_t, uint8_t);

//static double vBat, iBat, vSolar, iSolar, loadVoltage, ambientTemp, mosfetTemp, loadCurrent;

char *rx;
uint8_t myChar;
uint8_t inBuff[16];
uint8_t rxBuff[16];

uint8_t rxByteCount;
uint8_t inByteCount;
uint32_t usCounter;


//double vBat, iBat, vSolar, iSolar, loadVoltage, loadCurrent, ambientTemp, mosfetTemp;

#endif /* MPPT_H_ */

/** mppt.c
 * Top level source file for MPPT EMS (STI assembly number 781-124-033 rev. 0)
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
 * Baseline code was generated using STM32CubeMX version 4.23.0
 *
 *
 * REVISION HISTORY
 *
 * 1.0: 12/27/2017	Created By Nicholas C. Ipri (NCI) nipri@solartechnology.com
 */

/** Console Output for Debugging
 * Used in getADCReadings()
 * Uncomment #define DEBUG2 to get a string of formatted, human readable ADC data to a terminal program.
 * DEFAULT: Leave commented to send the real data packet to the controller.
 */
//#define DEBUG2

/** MPPT Algorithm Selection
*Uncomment #define INC_COND to utilize the MPPT Incremental Conductance (IC) algorithm.
*DEFAULT: Leave commented to implement the Perturb and Observe (P&O) method.
*
*NOTE: THE IC ALGORITHM IS UNVERIFIED FOR PRODUCTION USE IN VERSION 1.0!
*/
//#define INC_COND

// Used to control timer and fan functionality
#define ON		1	// Start the timer
#define OFF		0	// Stop the timer
#define UPDATE	2	// Update the duty cycle

// Used to clear, or not clear, the LCD
#define YES		1	// Clear
#define NO		0	// Don't clear

// Battery warning indicators
#define NORMALBATTV	0
#define HIBATTV 	1
#define LOBATTV		2
#define DEADBATT	3

// Time in seconds for load to be held off in the event of RF interference that messes up the driver for the load MOSFET
#define CYCLE_LOAD_TIMEOUT	5


#define ADSORPTION_TIME_FLOODED	3600 		// 3600 seconds = 60 minutes
#define ADSORPTION_LOCKOUT_TIME 28800		// 28800 seconds = 8 hours

#define MIN_DUTY_CYCLE		192		//75% of the TIM1 period
#define MAX_DUTY_CYCLE  	235 	//92% of the TIM1 period
#define PCT80_DUTY_CYCLE 	205

// Time, in seconds, to wait between reading solar array charge current when it's below THRESHOLD_CURRENT
// The switching converter is turned off while in timeout, conserving power.
#define LOW_CHARGE_CURRENT_TIMEOUT	10

#define min(a,b) 	((a) < (b)) ? (a) : (b);


#include "stm32f4xx_hal.h"
#include "HD44780.h"
#include "mppt.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>


ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim5;
TIM_HandleTypeDef htim9;
TIM_HandleTypeDef htim11;
UART_HandleTypeDef huart1;


uint8_t strBuffer[256];
uint8_t sendBuffer[128], escBuffer[128];

/** adcBuffer Description
 * 0: Battery Bank Voltage
 * 1: Solar Array Voltage
 * 2: Battery Bank Current
 * 3: Solar Array Current
 * 4: Battery Load Voltage
 * 5: Ambient Temperature
 * 6: MOSFET Temperature
 * 7: Battery Load Current
 */
uint16_t adcBuffer[9];

uint32_t vBattery;
uint32_t vSolarArray, last_vSolarArray;
uint32_t iBattery;
uint32_t iSolarArray;
uint32_t vLoad;
uint32_t tempAmbient;
uint32_t tempMOSFETS;
uint32_t iLoad;

uint16_t flashData;
uint16_t adsorptionTime, adsorptionCompleteTime;
uint16_t tim9Count, adcCount;
uint16_t canPulse;
uint16_t powerCycleTimeout, timerCount;
uint16_t battOffsetV, solarOffsetV, battOffsetI, solarOffsetI, loadOffsetI;
uint16_t mpptBypassCount = 0;
uint16_t duty;

uint8_t powerCycleOffTime, offTimeCount;
uint8_t cycleLoadTime = 0;
uint8_t maxDutyCycleCount = 0;

uint8_t lowChargeCurrentTimeout;
uint8_t sendMessageCount = 0;
uint8_t getADC = 0;
uint8_t adcConvComplete = 0;
uint8_t lcdUpdate = 0;
uint8_t lcdUpdateFlag = 0;
uint8_t warning = 0;
uint8_t pulseInterval = 120;		// 120 second (2 minute) intervals between pulsing the battery bank
uint8_t aveCount;

bool adsorptionFlag;
bool adsorptionComplete;
bool floatFlag;
bool canCharge;
bool isCharging;
bool lowChargeCurrentFlag;
bool overTempFlag;
bool batteryFaultFlag;
bool mpptBypassFlag;
bool cycleLoadPower;
bool enablePowerCycle;
bool fanStatus;

int POB_Direction = 1;

double dV, dI;
double currentPower, lastPower, lastVsolar, lastIsolar, lastIbattery;
double vBat, iBat, vSolar, iSolar, loadVoltage, ambientTemp, mosfetTemp, loadCurrent;
double vBatAve, iBatAve, vSolarAve, iSolarAve, loadVoltageAve, loadCurrentAve;
double vBatOut, iBatOut, vSolarOut, iSolarOut, loadVoltageOut, loadCurrentOut;

// adcUnit = Vref / 2^12
//Vref = 3.3v and 2^12 = 4096 for 12 bits of resolution
const double adcUnit = 0.000806;
const double voltageDividerOutput = 0.0623;		// Voltage Divider ratio gives 0.0623 volts out / volt in

// LCD Strings
char logo[] = "SOLAR TECH";
char version[] = "MPPT EMS VER 1.0";
char battery[] = "BATTERY BANK";
char solarArray[] = "SOLAR ARRAY";
char load[] = "BATTERY LOAD";
char battHi[] = "HIGH VOLTAGE!";
char battLo[] = "LOW VOLTAGE!";
char battDead[] = "BELOW 8 VOLTS!";
char charger1[] = "PLEASE CONNECT";
char charger2[] = "EXTERNAL CHARGER";

// Version string sent to the controller in sendMessage()
char ver[] = "A1.0";


void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM5_Init(void);
static void MX_TIM9_Init(void);
static void MX_TIM11_Init(void);

static void MX_USART1_UART_Init(void);
static void MX_DMA_Init(void);

void changePWM_TIM5(uint16_t, uint8_t);
void changePWM_TIM1(uint16_t, uint8_t);

double calcVoltage(uint16_t, uint8_t);
double calcCurrent(uint16_t );
double calcTemperature(uint16_t);

bool switchFan(uint8_t);
void switchSolarArray(uint8_t);
void switchLoad(uint8_t);
void switchCharger(uint8_t);
void switchChargeLED(uint8_t);
void toggleChargeLED(void);
void switchDiagLED(uint8_t);
void switchCapacitors(uint8_t);

void lcdBatteryInfo(void);
void lcdSolarInfo(void);
void lcdLoadInfo();
void getADCreadings(uint8_t);

double AdsorptionVoltage(double);
double FloatVoltage(double);

void updateLCD(uint8_t);
void sendMessage(void);
void pulse(void);
void delay_us(uint32_t);
void writeFlash(uint16_t);

void calcMPPT(void);
void calcMPPT_TI(void);

void calcMPPT_IC(void);

void mpptBypass(uint8_t);
void handleData(void);

extern void crc16_init(void);
extern uint16_t crc16(uint8_t[], uint8_t);


/** System Clock Configuration
*/
void SystemClock_Config(void)
{

	RCC_OscInitTypeDef RCC_OscInitStruct;
	RCC_ClkInitTypeDef RCC_ClkInitStruct;

	__HAL_RCC_PWR_CLK_ENABLE();

	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);


	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = 16;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 8;
	RCC_OscInitStruct.PLL.PLLN = 100;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 4;
	RCC_OscInitStruct.PLL.PLLR = 2;

	HAL_RCC_OscConfig(&RCC_OscInitStruct);


	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7);

	HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

	HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

	/* SysTick_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* ADC1 init function */
static void MX_ADC1_Init(void)
{

	ADC_ChannelConfTypeDef sConfig;

    /**Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion) 
    */
	hadc1.Instance = ADC1;
	hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4; // was _DIV4
	hadc1.Init.Resolution = ADC_RESOLUTION_12B;
	hadc1.Init.ScanConvMode = ENABLE;
	hadc1.Init.ContinuousConvMode = DISABLE; //DISABLE;
	hadc1.Init.DiscontinuousConvMode = DISABLE;
	hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
	hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
	hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc1.Init.NbrOfConversion = 8;
	hadc1.Init.DMAContinuousRequests = ENABLE;
	hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;

	HAL_ADC_Init(&hadc1);

	/**Configure the selected ADC regular channels and their corresponding rank in the sequencer and also their sample time. */

	// Battery Bank Voltage
	sConfig.Channel = ADC_CHANNEL_0;
	sConfig.Rank = 1;
	sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;

	HAL_ADC_ConfigChannel(&hadc1, &sConfig);

	//Solar Array Voltage
	sConfig.Channel = ADC_CHANNEL_1;
	sConfig.Rank = 2;
	sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;

	HAL_ADC_ConfigChannel(&hadc1, &sConfig);

//	Battery Bank Current
	sConfig.Channel = ADC_CHANNEL_2;
	sConfig.Rank = 3;
	sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;

	HAL_ADC_ConfigChannel(&hadc1, &sConfig);

//	Solar Array Current
	sConfig.Channel = ADC_CHANNEL_3;
	sConfig.Rank = 4;
	sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;

	HAL_ADC_ConfigChannel(&hadc1, &sConfig);

// 	Load Voltage
	sConfig.Channel = ADC_CHANNEL_4;
	sConfig.Rank = 5;
	sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;

	HAL_ADC_ConfigChannel(&hadc1, &sConfig);

//	Ambient Temperature
	sConfig.Channel = ADC_CHANNEL_6;
	sConfig.Rank = 6;
	sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES; //ADC_SAMPLETIME_28CYCLES;

	HAL_ADC_ConfigChannel(&hadc1, &sConfig);

//	MOSFET Temperature
	sConfig.Channel = ADC_CHANNEL_7;
	sConfig.Rank = 7;
	sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;

	HAL_ADC_ConfigChannel(&hadc1, &sConfig);

//	Load Current
	sConfig.Channel = ADC_CHANNEL_8;
	sConfig.Rank = 8;
	sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;

	HAL_ADC_ConfigChannel(&hadc1, &sConfig);

}

/**
 *Generates 4 PWM channels that switch the DC-DC MOSFETs.
 *These timer settings are chosen to give a 196 KHz switching frequency and
 *complementary switching of the high and low side MOSFETS
 *that is 180 deg phase shifted to give interleaved switching between the 2 sides (phases) of the bridge.
 *The DeadTime parameter was optimized using an oscilloscope with
 *2 channels connected to the gates of the HI and LOW side MOSFETS on one side of the bridge.
 *The DeadTime parameter below gives about 50 nS of dead time between switching the HI and LOW
 *side MOSFETS. The proper setting of this parameter is very important to prevent shoot-through, a condition where both
 *hi and lo side MOSFETS are on at the same time which would short the battery to ground... a very nasty situation indeed
 *
 *DO NOT CHANGE THESE SETTINGS without a thorough understanding of how the switching converter operates, else you risk
 *damage to the switching MOSFETS, processor, 3.3v supply and other components on the PCB.
*/

static void MX_TIM1_Init(void)

{

	  TIM_ClockConfigTypeDef sClockSourceConfig;
	  TIM_MasterConfigTypeDef sMasterConfig;
	  TIM_SlaveConfigTypeDef sSlaveConfig;
	  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig;

	  TIM_OC_InitTypeDef sConfigOC;
	  TIM_OC_InitTypeDef sConfigOC2;

	  htim1.Instance = TIM1;
	  htim1.Init.Prescaler = 0;
	  htim1.Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED2;
	  htim1.Init.Period = 256;
	  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	  htim1.Init.RepetitionCounter = 0;

	  HAL_TIM_Base_Init(&htim1);

	  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;

	  HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig);

	  HAL_TIM_PWM_Init(&htim1);

	  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

	  HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig);

	  sSlaveConfig.SlaveMode = TIM_SLAVEMODE_DISABLE;
	  sSlaveConfig.InputTrigger = TIM_TS_ITR0;

	  HAL_TIM_SlaveConfigSynchronization(&htim1, &sSlaveConfig);

	  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_ENABLE;
	  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
	  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
	  sBreakDeadTimeConfig.DeadTime =  22; //8;
	  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
	  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
	  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;

	  HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig);

	  sConfigOC.OCMode = TIM_OCMODE_PWM1;
	  sConfigOC.Pulse = PCT80_DUTY_CYCLE;
	  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
	  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
	  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;

	  sConfigOC2.OCMode = TIM_OCMODE_PWM2;
	  sConfigOC2.Pulse = 256 - PCT80_DUTY_CYCLE;
	  sConfigOC2.OCPolarity = TIM_OCPOLARITY_HIGH;
	  sConfigOC2.OCNPolarity = TIM_OCNPOLARITY_HIGH;
	  sConfigOC2.OCFastMode = TIM_OCFAST_DISABLE;
	  sConfigOC2.OCIdleState = TIM_OCIDLESTATE_RESET;
	  sConfigOC2.OCNIdleState = TIM_OCNIDLESTATE_RESET;

	  HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1);

	  HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC2, TIM_CHANNEL_2);

	 HAL_TIM_MspPostInit(&htim1);
}

// Controls PWM to the LCD backlight
static void MX_TIM5_Init(void)
{

	TIM_ClockConfigTypeDef sClockSourceConfig;
	TIM_MasterConfigTypeDef sMasterConfig;

	TIM_OC_InitTypeDef sConfigOC_LCD;

  	htim5.Instance = TIM5;
  	htim5.Init.Prescaler = 0;
  	htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  	htim5.Init.Period = 32000;
  	htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

  	HAL_TIM_Base_Init(&htim5);

  	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;

  	HAL_TIM_ConfigClockSource(&htim5, &sClockSourceConfig);


  	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

  	HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig);

	sConfigOC_LCD.OCMode = TIM_OCMODE_PWM1;
	sConfigOC_LCD.Pulse = 15000;
	sConfigOC_LCD.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC_LCD.OCFastMode = TIM_OCFAST_DISABLE;
	sConfigOC_LCD.OCIdleState = TIM_OCIDLESTATE_RESET;

	HAL_TIM_OC_Init(&htim5);

	HAL_TIM_OC_ConfigChannel(&htim5, &sConfigOC_LCD, TIM_CHANNEL_1);

	HAL_TIM_MspPostInit(&htim5);
}

//Used as a 1 mS timer.
static void MX_TIM9_Init(void) {

	  TIM_ClockConfigTypeDef sClockSourceConfig;
	  TIM_MasterConfigTypeDef sMasterConfig;

	  htim9.Instance = TIM9;
	  htim9.Init.Prescaler = 50000; // 50 MHz / 50000 = 1 kHz
	  htim9.Init.CounterMode = TIM_COUNTERMODE_UP;
	  htim9.Init.Period = 1; // period of 1 gives 1 mS with the prescaler @ 50000
	  htim9.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;

	  HAL_TIM_Base_Init(&htim9);

	  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;

	  HAL_TIM_ConfigClockSource(&htim9, &sClockSourceConfig);

	  HAL_TIM_OC_Init(&htim9);

	  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

	  HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig);

	  HAL_TIM_Base_Start_IT(&htim9);
}

//Used as free running counter for delay_us() .
static void MX_TIM11_Init(void) {

	  TIM_ClockConfigTypeDef sClockSourceConfig;
	  TIM_MasterConfigTypeDef sMasterConfig;

	  htim11.Instance = TIM11;
	  htim11.Init.Prescaler = 50; // 50 MHz / 50 = 1 MHz clock... 1 uS count interval
	  htim11.Init.CounterMode = TIM_COUNTERMODE_UP;
	  htim11.Init.Period = 0xffff; // Full 16 bit counter
	  htim11.Init.ClockDivision = TIM_CLOCKDIVISION_DIV2;

	  HAL_TIM_Base_Init(&htim11);

	  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_ITR0; //TIM_CLOCKSOURCE_INTERNAL;

	  HAL_TIM_ConfigClockSource(&htim11, &sClockSourceConfig);

	  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

	  HAL_TIMEx_MasterConfigSynchronization(&htim11, &sMasterConfig);;
	  HAL_TIM_Base_Start(&htim11);
}

/* USART1 init function */
static void MX_USART1_UART_Init(void)
{

	huart1.Instance = USART1;
	huart1.Init.BaudRate = 9600;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;

	HAL_UART_Init(&huart1);

	__HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
	HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_0);
	HAL_NVIC_SetPriority(USART1_IRQn,0,0);
	HAL_NVIC_EnableIRQ(USART1_IRQn);
	HAL_NVIC_ClearPendingIRQ(USART1_IRQn);
}


/**
  * Enable DMA controller clock and interrupt
  */
static void MX_DMA_Init(void)
{
  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}


/* GPIOs for digital functions are configured here */
/* GPIOs for alternate functions are configured in stm32f4xx_hal_msp.c */
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

 //PORT C GPIOs
 //Pin 11 pings the external WDT, Pin 10 is the Charge LED,  Pin 9 controls SA_SWITCH,  Pins 6 - 0 control the LCD
  GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_10 | GPIO_PIN_9 | GPIO_PIN_6 |GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_3 | GPIO_PIN_2 | GPIO_PIN_1 |GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  // PORT B GPIOs: INPUTS
  //Pin 7: Power Good Input, Pin 8: OverVoltage Fault (OV_FAULT) input
  GPIO_InitStruct.Pin = GPIO_PIN_7 | GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  //PORT B GPIOs: OUTPUTS
  //Pin 1 controls BB_V+_Switch, Pin 2 for Fan Control, Pin 15 for the on-board Diagnostic LED
   GPIO_InitStruct.Pin = GPIO_PIN_15 | GPIO_PIN_11 | GPIO_PIN_10 |  GPIO_PIN_5 | GPIO_PIN_2 | GPIO_PIN_1;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_PULLDOWN;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {

	//TIM9 is the 1 mS timer
	if (htim->Instance==TIM9)
	{
		// Read the ADC and execute the MPPT algorithm in the isCharging loop every 250 mS
//		if (adcCount == 1000)
		if (adcCount == 100)
		{
			adcCount = 0;
			getADC = 1;
		}

		if (tim9Count == 1000)
		{
			tim9Count = 0;
			lcdUpdate++;
			canPulse++;

			if (canPulse > pulseInterval)
				canPulse = 0;

			if (aveCount <= 5)
			{
				if (aveCount == 0)
				{
					vBatAve = 0;
					iBatAve = 0;
					vSolarAve = 0;
					iSolarAve = 0;
					loadVoltageAve = 0;
					loadCurrentAve = 0;
				}

				vBatAve += vBat;
				iBatAve += iBat;
				vSolarAve += vSolar;
				iSolarAve  += iSolar;
				loadVoltageAve += loadVoltage;
				loadCurrentAve += loadCurrent;

				aveCount++;

				if (aveCount == 5)
				{
					aveCount = 0;

					vBatOut = vBatAve / 5;
					iBatOut = iBatAve / 5;
					vSolarOut = vSolarAve / 5;
					iSolarOut = iSolarAve / 5;
					loadVoltageOut = loadVoltageAve / 5;
					loadCurrentOut = loadCurrentAve / 5;
				}
			}

			// used in updateLCD to display different messages at these intervals. Add more intervals to add more messages
			if ((lcdUpdate == 1) || (lcdUpdate == 5) || (lcdUpdate == 9) || (lcdUpdate == 13) )
				lcdUpdateFlag = 1;

			if (isCharging == 1) // Skip displaying the logo and version while charging
			{
				if (lcdUpdate > 16)
				{
					if ( (warning == HIBATTV) || (warning == LOBATTV) )
						lcdUpdate = 0;
					else
						lcdUpdate = 4;
				}
			}
			else
			{
				if (lcdUpdate > 16) // Display everything
					lcdUpdate = 0;
			}

			// Time interval to wait between checking for minimum charge current. This flag get set in the canCharge loop
			if (lowChargeCurrentFlag)
			{
				lowChargeCurrentTimeout++;

				if (lowChargeCurrentTimeout == LOW_CHARGE_CURRENT_TIMEOUT)
				{
					lowChargeCurrentTimeout = 0;
					lowChargeCurrentFlag = false;
				}
			}
			else
			{
				lowChargeCurrentTimeout = 0;
			}

			// Flash the charge LED when charging is active
			if (isCharging == 0)
				toggleChargeLED();
			else
				switchChargeLED(ON);


			// Adsorption voltage (Va) timer.
			if (adsorptionFlag)
			{
				adsorptionTime++;

				if (adsorptionTime >= ADSORPTION_TIME_FLOODED)
				{
					adsorptionFlag = false;
					adsorptionComplete = true;
					adsorptionTime = 0;
				}
			}

			// Va lockout timer. After charging to Va and holding for ADSORPTION_TIME_FLOODED, wait for ADSORPTION_LOCKOUT_TIME
			// before allowing to charge up to Va again
			if (adsorptionComplete)
			{
				adsorptionCompleteTime++;

				if (adsorptionCompleteTime >= ADSORPTION_LOCKOUT_TIME)
				{
					adsorptionCompleteTime = 0;
					adsorptionComplete = false;
				}
			}


			// Power cycle watchdog timer: set by the host controller over the UART to power cycle the load after a programmed time
			// (powerCycleTimeout) and holds the load off for powercycleOffTime before restoring power to the load.
			if (enablePowerCycle)
			{
				timerCount++;

				if (timerCount >= powerCycleTimeout)
				{
					enablePowerCycle = false;
					offTimeCount++;
					switchLoad(OFF);
				}
			}

			if (offTimeCount > 0)
			{
				offTimeCount++;

				if (offTimeCount >= powerCycleOffTime)
				{
					offTimeCount = 0;
					switchLoad(ON);
				}
			}

			if (cycleLoadPower)
			{
				cycleLoadTime++;

				if (cycleLoadTime >= CYCLE_LOAD_TIMEOUT)
				{
					cycleLoadTime = 0;
					cycleLoadPower = false;
					switchLoad(ON);
				}

			}
		}

		tim9Count++;
		adcCount++;

		HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_11); // Ping the WDT
	}
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc1) {

	vBattery += adcBuffer[0];
	vSolarArray += adcBuffer[1];
	iBattery += adcBuffer[2];
	iSolarArray += adcBuffer[3];
	vLoad += adcBuffer[4];
	tempAmbient += adcBuffer[5];
	tempMOSFETS += adcBuffer[6];
	iLoad += adcBuffer[7];

	adcConvComplete = 1;

}

// Controls the Duty cycle of the switching MOSFETs
void changePWM_TIM1(uint16_t pulse, uint8_t onOffUpdate)
{

	  if (onOffUpdate == ON)
	  {
		  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, PCT80_DUTY_CYCLE);
		  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 256 - PCT80_DUTY_CYCLE);

		  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
		  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);

		  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
		  HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
	  }

	  else if (onOffUpdate == OFF)
	  {
		  HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
		  HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);

		  HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
		  HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_2);

//		  HAL_TIM_GenerateEvent(&htim1, TIM_EVENTSOURCE_BREAK);


	  }

	  else if (onOffUpdate == UPDATE)
	  {
		  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pulse);
		  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 256 - pulse);
	  }
	  else
	  {
		  // Do Nothing
	  }
}

// Controls the brightness of the LCD backlight
void changePWM_TIM5(uint16_t pulse, uint8_t onOffUpdate)
{
	 if (onOffUpdate == ON)
	 {
		 __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_1, pulse);
		 HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_1);
	 }

	 else if (onOffUpdate == OFF)
	 {
		 HAL_TIM_PWM_Stop(&htim5, TIM_CHANNEL_1);
	 }

	 else if (onOffUpdate == UPDATE)
	 {
		 __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_1, pulse);
	 }

	 else
	 {
		 // Do Nothing
	 }
}

void getADCreadings (uint8_t howMany)
{

	uint8_t i;

	vBattery = 0;
	vSolarArray = 0;
	iBattery = 0;
	iSolarArray = 0;
	vLoad = 0;
	tempAmbient = 0;
	tempMOSFETS = 0;
	iLoad = 0;

	//	ADC channel values are initialized above and new values are accumulated in HAL_ADC_ConvCpltCallback() after each loop
	//	(i.e.each completed conversion)
	for (i=howMany; i>0; i--)
	{
		adcConvComplete = 0;

		HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adcBuffer, 8);
		while(!adcConvComplete);
	}

// Averaging the readings
	vBattery /= howMany;
	vSolarArray /= howMany;
	iBattery /= howMany;
	iSolarArray /= howMany;
	vLoad /= howMany;
	tempAmbient /= howMany;
	tempMOSFETS /= howMany;
	iLoad /= howMany;

// Check for any problems with battery voltage
	if (vBattery >= V_MAX_LOAD_OFF )
	{
		warning = HIBATTV;
		switchLoad(OFF);
	}

	if ((warning == HIBATTV) && (vBattery <= V_MAX_LOAD_ON) )
	{
		warning = NORMALBATTV;
		switchLoad(ON);
	}

	if (vBattery <= V_MIN_LOAD_OFF)
	{
		warning = LOBATTV;
		switchLoad(OFF);
	}

	if ((warning == LOBATTV) && (vBattery >= V_MIN_LOAD_ON))
	{
		warning = NORMALBATTV;
		switchLoad(ON);
	}

	if (vBattery < BAT_DROP_DEAD_VOLT)
	{
		warning = DEADBATT;
		switchLoad(OFF);
	}

	if ( (warning == DEADBATT) && (vBattery >= BAT_DROP_DEAD_VOLT) )
	{
		warning = NORMALBATTV;
		switchLoad(ON);
	}

// Calculate the values and send them to the host controller
	vBat = calcVoltage(vBattery, 2);
	vSolar = calcVoltage(vSolarArray, 2);
	iBat = calcCurrent(iBattery);
	iSolar = calcCurrent(iSolarArray);
	loadVoltage = calcVoltage(vLoad, 1);
	ambientTemp = calcTemperature(tempAmbient);
	mosfetTemp = calcTemperature(tempMOSFETS);
	loadCurrent = calcCurrent(iLoad);

//check MOSFET temperature and switch fan on or off as needed
	if ( (!fanStatus) && (mosfetTemp >= FAN_ON_TEMP) )
		fanStatus = switchFan(ON);
	else if ( (fanStatus) && (mosfetTemp <= FAN_OFF_TEMP))
		fanStatus = switchFan(OFF);
	else
		//do nothing

	sendMessageCount++;

#ifdef DEBUG2

	if (sendMessageCount >= 15)		// Output a line every second (debugging)
	{
		sendMessageCount = 0;
		// This data is sent to a terminal like puTTY
		 sprintf(strBuffer, "MPPT ADC Values: %2.2f, %2.2f, %2.2f, %2.2f, %2.2f, %2.2f, %2.2f, %2.2f, %d, %2.2f, %2.2f, %d, %d\r\n", vBat, iBat, vSolar, iSolar, loadVoltage, loadCurrent, ambientTemp, mosfetTemp, canCharge, FloatVoltage(ambientTemp), AdsorptionVoltage(ambientTemp), floatFlag, adsorptionFlag);
		 HAL_UART_Transmit(&huart1, strBuffer, sizeof(strBuffer), 0xffff);
	}

#else
		if (sendMessageCount >= 150)	// Output a line every 15 seconds
		{
			sendMessageCount = 0;
			sendMessage();
		}

#endif

}

double AdsorptionVoltage(double ambTemp)
{
	if (ambTemp < TEMP_NEG30)
		return (ATV_NEG30);

	else if ( (ambTemp > TEMP_NEG30) && (ambTemp < TEMP_40) )
		return (ATV_NEG30 - ( (ambTemp - TEMP_NEG30) * (double)RATE1) );

	else
		return (ATV_40);
}

double FloatVoltage(double ambTemp)
{
	if (ambTemp <= TEMP_NEG30)
		return (FTV_NEG30);

	else if ( (ambTemp > TEMP_NEG30) && (ambTemp < TEMP_40) )
		return (FTV_NEG30 - ( (ambTemp - TEMP_NEG30) * (double)RATE1) );

	else
		return (FTV_40);
}

double calcVoltage(uint16_t ADvalue, uint8_t gain)
{
	return (ADvalue * adcUnit) / gain / voltageDividerOutput;
}

double calcCurrent(uint16_t ADvalue)
{
	const uint8_t gain = 50;						// Gain of the INA213AIDCK current sense amplifier
	const double Rsense	=	0.002;					// Value of current sense resistors used in design.

	return (ADvalue * adcUnit) / gain / Rsense;
}

double calcTemperature(uint16_t ADvalue)
{
	const double LM335voltage = 0.010;				// The LM335 outputs 10 mV / degree Kelvin
	const double kelvin = 273.15;					// 0 C = 273.15 K

	return ((ADvalue * adcUnit) / LM335voltage) - kelvin;
}

void mpptBypass(uint8_t onOff)
{

	if (onOff == ON)
	{
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET);

		mpptBypassCount++;

		if ( (mpptBypassCount >= 600) || ((mpptBypassCount >= 100) && (iSolar < lastIbattery) ) )
		{
			mpptBypassCount = 0;
			maxDutyCycleCount = 0;
			mpptBypassFlag = true;
			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
		}
	}

	else
	{
		mpptBypassCount = 0;
		maxDutyCycleCount = 0;
		mpptBypassFlag = false;
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
	}
}

void calcMPPT_IC(void)
{
	double conductance;

	dV = vSolar - lastVsolar;
	dI = iSolar - lastIsolar;

	if (dV != 0)
	{
		conductance  = (double)dI / (double)dV;
	}

	if (dV != 0)
	{

//		if ((iSolar + (conductance * vSolar)) == 0)
		if (conductance == -((double)iSolar/(double)vSolar))
		{

		}

//		else if ((iSolar + (conductance * vSolar)) > 0)
		else if (conductance > -((double)iSolar/(double)vSolar))
		{
			duty++;

			if (duty >= MAX_DUTY_CYCLE)
			{
				duty = MAX_DUTY_CYCLE;
				maxDutyCycleCount++;
			}
		}

		else
		{
			duty--;

			if (duty <= MIN_DUTY_CYCLE)
				duty = MIN_DUTY_CYCLE;
		}

	}

	else if (dI != 0)
	{
		if (dI > 0)
		{
			duty--;

			if (duty <= MIN_DUTY_CYCLE)
				duty = MIN_DUTY_CYCLE;
		}

		else
		{
			duty++;

			if (duty >= MAX_DUTY_CYCLE)
			{
				duty = MAX_DUTY_CYCLE;
				maxDutyCycleCount++;
			}
		}

	}

	lastVsolar = vSolar;
	lastIsolar = iSolar;
	changePWM_TIM1(duty, UPDATE);
}

void calcMPPT_TI(void)
{
	currentPower = vSolar * iSolar;

	if (currentPower < lastPower)
	{
		POB_Direction *= -1;
	}

	if (POB_Direction == 1)
	{
		duty--;

		if (duty <= MIN_DUTY_CYCLE)
			duty = MIN_DUTY_CYCLE;
	}
	else
	{
		duty++;

		if (duty >= MAX_DUTY_CYCLE)
		{
			duty = MAX_DUTY_CYCLE;
			maxDutyCycleCount++;
		}
//		else
//		{
//			maxDutyCycleCount = 0;
//		}
	}

	lastPower = currentPower;
	changePWM_TIM1(duty, UPDATE);
}

void calcMPPT(void)
{

	currentPower = vSolar * iSolar;

//	if (currentPower == lastPower) {
		//do nothing
//	}

//	else
//	{
		if (currentPower > lastPower)
		{
//			if (vSolarArray > last_vSolarArray)
			if (vSolar > lastVsolar)
			{
				duty--;

				if (duty <= MIN_DUTY_CYCLE)
					duty = MIN_DUTY_CYCLE;
			}
			else
			{
				duty++;

				if (duty >= MAX_DUTY_CYCLE)
				{
					duty = MAX_DUTY_CYCLE;
					maxDutyCycleCount++;
				}
			}
		}

		else
		{

//			if (vSolarArray > last_vSolarArray)
			if (vSolar > lastVsolar)
			{
				duty++;

				if (duty >= MAX_DUTY_CYCLE)
				{
					duty = MAX_DUTY_CYCLE;
					maxDutyCycleCount++;
				}
			}
			else
			{
				duty--;

				if (duty <= MIN_DUTY_CYCLE)
					duty = MIN_DUTY_CYCLE;
			}
		}
//	}

	lastPower = currentPower;
//	last_vSolarArray = vSolarArray;
	lastVsolar = vSolar;
	changePWM_TIM1(duty, UPDATE);
}

bool switchFan(uint8_t onOff)
{
	bool isFanOn;

	if (onOff == ON)
	{
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);
		isFanOn = true;
	}
	else
	{
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);
		isFanOn = false;
	}

	return isFanOn;
}

void switchSolarArray(uint8_t onOff)
{
	if (onOff == ON)
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);
}

void switchLoad(uint8_t onOff)
{
	if (onOff == ON)
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
}

void switchCharger(uint8_t onOff)
{
	if (onOff == ON)
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
}

void switchChargeLED(uint8_t onOff)
{
	if (onOff == ON)
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_RESET);
}

void toggleChargeLED(void)
{
	HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_10);
}

void switchDiagLED(uint8_t onOff)
{
	if (onOff == ON)
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
}

void switchCapacitors(uint8_t onOff)
{
	if (onOff == ON)
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
}


void lcdBatteryInfo(void)
{
	char tmp_buffer[16];

	HD44780_WriteData(0, 0, battery, YES);
//	sprintf(tmp_buffer, "%2.2f V %2.2f A", vBat, iBat);
	sprintf(tmp_buffer, "%2.2f V %2.2f A", vBatOut, iBatOut);
	HD44780_WriteData(1, 0, tmp_buffer, NO);
}

void lcdSolarInfo(void)
{
	char tmp_buffer[16];

	HD44780_WriteData(0, 0, solarArray, YES);
//	sprintf(tmp_buffer, "%2.2f V %2.2f A", vSolar, iSolar);
	sprintf(tmp_buffer, "%2.2f V %2.2f A", vSolarOut, iSolarOut);
	HD44780_WriteData(1, 0, tmp_buffer, NO);
}

void lcdLoadInfo(void)
{
	char tmp_buffer[16];

	HD44780_WriteData(0, 0, load, YES);
//	sprintf(tmp_buffer, "%2.2f V %2.2f A", loadVoltage, loadCurrent);
	sprintf(tmp_buffer, "%2.2f V %2.2f A", loadVoltageOut, loadCurrentOut);
	HD44780_WriteData(1, 0, tmp_buffer, NO);
}


void updateLCD(uint8_t warning)
{

	lcdUpdateFlag = 0;

	switch (lcdUpdate)
	{
		case 1:

			if (warning == HIBATTV)
			{
				HD44780_WriteData(0, 0, battery, YES);
				HD44780_WriteData(1, 0, battHi, NO);
			}
			else if (warning == LOBATTV)
			{
				HD44780_WriteData(0, 0, battery, YES);
				HD44780_WriteData(1, 0, battLo, NO);
			}
			else if (warning == DEADBATT)
			{
				HD44780_WriteData(0, 0, battery, YES);
				HD44780_WriteData(1, 0, battDead, NO);
			}

			else

				if (isCharging == 1)
				{
					lcdBatteryInfo();
				}
				else
				{
					HD44780_WriteData(0, 0, logo, YES);
					HD44780_WriteData(1, 0, version, NO);
				}

			break;

		case 5:

			if (warning == DEADBATT)
			{
				HD44780_WriteData(0, 0, charger1, YES);
				HD44780_WriteData(1, 0, charger2, NO);
			}
			else
			{
				lcdSolarInfo();
			}

			break;

		case 9:
			lcdBatteryInfo();
			break;

		case 13:
			lcdLoadInfo();
			break;

		default:
			break;
	}
}

void pulse(void)
{

	uint8_t i;

	switchCapacitors(OFF);

	for (i=0; i<=9; i++)
	{
		HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_11);
		delay_us(100);
	}

	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
	switchCapacitors(ON);
}

void delay_us(uint32_t usDelay)
{
	uint32_t initTime;

	initTime = __HAL_TIM_GET_COUNTER(&htim11);
	while ( (__HAL_TIM_GET_COUNTER(&htim11) - initTime < usDelay));
}

void writeFlash(uint16_t data)
{
	HAL_FLASH_Unlock();

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGSERR );
      FLASH_Erase_Sector(FLASH_SECTOR_4, VOLTAGE_RANGE_3);

      HAL_FLASH_Program(TYPEPROGRAM_HALFWORD, 0x08010000, 0x0004);

      HAL_FLASH_Program(TYPEPROGRAM_HALFWORD, 0x08010002, 0x0004);
      HAL_FLASH_Program(TYPEPROGRAM_HALFWORD, 0x08010004, 0x0004);

      HAL_FLASH_Lock();
}

// this function sends data to the controller
void sendMessage(void)
{

	uint16_t crc = 0xffff;
	uint8_t msgLength = 0;
	uint8_t i, j;
	uint16_t data;

	memset((void *)sendBuffer, 0, sizeof(sendBuffer));
	memset((void *)escBuffer, 0, sizeof(escBuffer));

	sendBuffer[0] = 0x9a;
	msgLength++;

	strncpy((char *)&sendBuffer[1], ver, 4);
	msgLength += 4;

	sendBuffer[msgLength] = 0x9e;
	msgLength++;

	// Battery Voltage
//	data = vBat * 1000;	// Convert to mV
	data = vBatOut * 1000;	// Convert to mV
	sendBuffer[msgLength] = (uint8_t)data & 0x00ff;
	msgLength++;
	sendBuffer[msgLength] = (uint8_t) (data>>8);
	msgLength++;

	// Battery current
//	data = iBat * 1000;
	data = iBatOut * 1000;
	sendBuffer[msgLength] = (uint8_t)data & 0x00ff;
	msgLength++;
	sendBuffer[msgLength] = (uint8_t) (data>>8);
	msgLength++;

	// Solar Array Voltage
//	data = vSolar * 1000;
	data = vSolarOut * 1000;
	sendBuffer[msgLength] = (uint8_t)data & 0x00ff;
	msgLength++;
	sendBuffer[msgLength] = (uint8_t) (data>>8);
	msgLength++;

	// Solar Array Current
//	data = iSolar * 1000;
	data = iSolarOut * 1000;
	sendBuffer[msgLength] = (uint8_t)data & 0x00ff;
	msgLength++;
	sendBuffer[msgLength] = (uint8_t) (data>>8);
	msgLength++;

	// Load Voltage
//	data = loadVoltage * 1000;
	data = loadVoltageOut * 1000;
	sendBuffer[msgLength] = (uint8_t)data & 0x00ff;
	msgLength++;
	sendBuffer[msgLength] = (uint8_t) (data>>8);
	msgLength++;

	// Load Current
//	data = loadCurrent * 1000;
	data = loadCurrentOut * 1000;
	sendBuffer[msgLength] = (uint8_t)data & 0x00ff;
	msgLength++;
	sendBuffer[msgLength] = (uint8_t) (data>>8);
	msgLength++;

	// Vbattery flag
	data = batteryFaultFlag;
	sendBuffer[msgLength] = (uint8_t)data & 0x00ff;
	msgLength++;

	// Overtemp flag
	data = overTempFlag;
	sendBuffer[msgLength] = (uint8_t)data & 0x00ff;
	msgLength++;

	crc = crc16(sendBuffer, msgLength);
	sendBuffer[msgLength] = (uint8_t)crc & 0x00ff;
	msgLength++;
	sendBuffer[msgLength] = (uint8_t) (crc>>8);
	msgLength++;

	for (i = 0, j = 0; i < msgLength; i++, j++)
	{

		switch (sendBuffer[i])
		{

			case 0x9a:

				if (i != 0)
				{
					escBuffer[j++] = 0x9b;
					escBuffer[j] = 0x01;
				}

				else
					escBuffer[j] = sendBuffer[i];

				break;

			case 0x9b:

				escBuffer[j++] = 0x9b;
				escBuffer[j] = 0x02;

				break;

			default:
				escBuffer[j] = sendBuffer[i];
				break;
		}
	}

	HAL_UART_Transmit(&huart1, escBuffer, j, 0xffff);
}

void handleData()
{

	uint8_t commandByte;

	commandByte = inBuff[1];

	//to be altered as we add more commands
	if (commandByte != 0x00) {
		return;
	}

	powerCycleTimeout = (inBuff[2] << 8) | inBuff[3];
	powerCycleOffTime = inBuff[4];

	if ((powerCycleTimeout >= 1) && (powerCycleTimeout < 0xffff)) {
		enablePowerCycle = true;
		timerCount = 0;
		offTimeCount = 0;
	}

	else {
		enablePowerCycle = false;
	}
}


int main(void)
{

//	uint16_t start_volt;

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* Configure the system clock */
	SystemClock_Config();

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_ADC1_Init();
	MX_TIM1_Init();
	MX_TIM5_Init();
	MX_TIM9_Init();
	MX_TIM11_Init();
	MX_USART1_UART_Init();

	crc16_init();
	HD44780_Init();
	HD44780_WriteData(0, 0, "INITIALIZING!", YES);

	lcdUpdate = 0;
	canCharge = false;
	isCharging = false;
	mpptBypass(OFF);

	fanStatus = switchFan(OFF);
	switchCharger(OFF);
	switchSolarArray(OFF); // Enable this only when ready to charge, disable all other times.
	switchLoad(ON);
	switchChargeLED(ON);
	switchCapacitors(ON);
	switchDiagLED(OFF);

	// Ensure that Desulphate MOSFETS are OFF
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);

	changePWM_TIM5(15000, ON);
	changePWM_TIM1(PCT80_DUTY_CYCLE, OFF);

//	getADCreadings(32);
//	lcdBatteryInfo();

//writeFlash(0xaa55);
	battOffsetV =  *(__IO uint16_t *)0x08010000;
	solarOffsetV = *(__IO uint16_t *)0x08010002;
	battOffsetI = *(__IO uint16_t *)0x08010006;
	solarOffsetI = *(__IO uint16_t *)0x08010004;
	loadOffsetI = *(__IO uint16_t *)0x08010008;

	lowChargeCurrentFlag = false;
	lowChargeCurrentTimeout = 0;

	overTempFlag = false;
	batteryFaultFlag = false;
	adsorptionComplete = false;
	mpptBypassFlag = false;
	cycleLoadPower = false;

	adsorptionTime = 0;
	adsorptionCompleteTime = 0;
	aveCount = 0;

	while (1)
	{

		// Get ADC readings
		if (getADC == 1)
		{
			getADC = 0;
			getADCreadings(32);
		}

		// Update the LCD
		if (lcdUpdateFlag == 1)
		{
			lcdUpdateFlag = 0;
			updateLCD(warning);
		}

		if ((!enablePowerCycle) && (warning == NORMALBATTV) && (loadVoltage > 0.1) && (loadVoltage < 5.0))
		{
			cycleLoadPower = true;
			switchLoad(OFF);
		}

		if (vBattery >= BAT_DROP_DEAD_VOLT) // We charge only if the battery isn't too dead
		{

			// We have enough solar energy to charge the batteries
			if (vSolarArray >= (vBattery + TWO_VOLT))
			{
				switchSolarArray(ON);
				switchCharger(ON);

				if (canPulse == pulseInterval)
				{
					pulse();
					canPulse = 0;
				}

				// Batteries need bulk charging?
				if ( (vBat < FloatVoltage(ambientTemp) - (double)0.25) )
				{
					canCharge = true;
					adsorptionFlag = false;
					adsorptionTime = 0;
					floatFlag = false;
				}

				// did we charge to the adsorption voltage (Va) but didn't hold it for ADSORPTION_TIME_FLOODED?
				else if (adsorptionFlag && floatFlag && !adsorptionComplete)
				{
					if (vBat <= AdsorptionVoltage(ambientTemp) - (double)0.5)
					{
						canCharge = true;
					}
					else
					{
						canCharge = false;
//						switchCharger(OFF);
//						switchSolarArray(OFF);
						changePWM_TIM1(PCT80_DUTY_CYCLE, OFF);
					}
				}

				// Did we charge to Va and hold it for ADSORPTION_TIME_FLOODED?
				else if ( (!adsorptionFlag && floatFlag && adsorptionComplete) || (!adsorptionFlag && !floatFlag && adsorptionComplete) )
				{
					if (vBat <= FloatVoltage(ambientTemp) - (double)(0.25))
					{
						canCharge = true;
					}
					else
					{
						canCharge = false;
//						switchCharger(OFF);
//						switchSolarArray(OFF);
						changePWM_TIM1(PCT80_DUTY_CYCLE, OFF);
					}
				}

				else
				{
					canCharge = false;
//					switchCharger(OFF);
//					switchSolarArray(OFF);
					changePWM_TIM1(PCT80_DUTY_CYCLE, OFF);
				}

				while(canCharge)
				{

					if (canPulse == pulseInterval)
					{
						pulse();
						canPulse = 0;
					}

					if (getADC == 1)
					{
						getADC = 0;
						getADCreadings(32);
					}

					if (lcdUpdateFlag == 1)
					{
						lcdUpdateFlag = 0;
						updateLCD(warning);
					}

					if ((!enablePowerCycle) && (warning == NORMALBATTV) && (loadVoltage > 0.1) && (loadVoltage < 5.0))
					{
						cycleLoadPower = true;
						switchLoad(OFF);
					}

					// May not need this here...
					if (vBat < (FloatVoltage(ambientTemp) - (double)0.25) )
					{
						adsorptionFlag = false;
						floatFlag = false;
						adsorptionTime = 0;
					}

					// Get out of this loop if we can't charge, no longer need to charge
					// or, for whatever reason, we drop below our "drop dead" threshold voltage
					if ( (vSolarArray <= (vBattery + TWO_VOLT) ) || (vBat >= AdsorptionVoltage(ambientTemp) ) || (vBattery < BAT_DROP_DEAD_VOLT) )
					{
						canCharge = false;
						isCharging = false;
//						switchCharger(OFF);
//						switchSolarArray(OFF);
						changePWM_TIM1(PCT80_DUTY_CYCLE, OFF);
					}

					else
					{

						// Check if we have minimum charge current from the PV panels
						if (!lowChargeCurrentFlag)
						{

							changePWM_TIM1(PCT80_DUTY_CYCLE, ON);
//							switchCharger(ON);

							HAL_Delay(1000);
							getADCreadings(32);

							// Start charging if we have enough current
							if (iSolarArray >= THRESHOLD_CURRENT)
							{
//								switchSolarArray(ON);
								HAL_Delay(10);
								isCharging = true;
//								currentPower = vSolar * iSolar;
//								last_vSolarArray = vSolarArray;
								lastVsolar = vSolar;
								duty = PCT80_DUTY_CYCLE;
								lastPower = currentPower;
								lastVsolar = vSolar;
								lastIsolar = iSolar;
							}
							// If we don't have minimum charge current, set a flag and wait for LOW_CHARGE_CURRENT_TIMEOUT before trying again
							else
							{
								isCharging = false;
//								switchCharger(OFF);
//								switchSolarArray(OFF);
								changePWM_TIM1(PCT80_DUTY_CYCLE, OFF);
								lowChargeCurrentFlag = true;
							}
						}

						while (isCharging)
						{

							if (getADC == 1)
							{
								getADC = 0;
								getADCreadings(32);


								if (maxDutyCycleCount < 100)
								{
									if (mpptBypassFlag == true)
									{
//										switchSolarArray(ON);
//										switchCharger(ON);
										changePWM_TIM1(PCT80_DUTY_CYCLE, ON);
										mpptBypassFlag = false;
										mpptBypass(OFF);
									}

#ifdef INC_COND
//									calcMPPT_IC();
#else
									calcMPPT();
//									calcMPPT_TI();
#endif

								}
								else if ( (maxDutyCycleCount >= 100) && (mpptBypassCount < 600) )
								{

									if (mpptBypassCount == 0)
										lastIbattery = iBat;

//									switchCharger(OFF);
//									switchSolarArray(OFF);
									changePWM_TIM1(PCT80_DUTY_CYCLE, OFF);
									mpptBypass(ON);
								}

							}

							if (lcdUpdateFlag == 1)
							{
								lcdUpdateFlag = 0;
								updateLCD(warning);
							}

							if ((!enablePowerCycle) && (warning == NORMALBATTV) && (loadVoltage > 0.1) && (loadVoltage < 5.0))
							{
								cycleLoadPower = true;
								switchLoad(OFF);
							}

							// We no longer have enough current to charge.
							if (iSolarArray < THRESHOLD_CURRENT)
							{
//								switchCharger(OFF);
//								switchSolarArray(OFF);
								isCharging = false;
								mpptBypass(OFF);
							}

							if (vSolarArray >= MAX_PV_VOLT)
							{
								duty = PCT80_DUTY_CYCLE;
							}

							if (vBat < FloatVoltage(ambientTemp))
							{
								adsorptionFlag = false;
								floatFlag = false;
								adsorptionTime = 0;
							}

							if ( (warning == HIBATTV) || (warning == DEADBATT) )
							{
								isCharging = false;
								canCharge = false;
//								switchCharger(OFF);
//								switchSolarArray(OFF);
								changePWM_TIM1(PCT80_DUTY_CYCLE, OFF);
								mpptBypass(OFF);
							}

							if (vBat >= FloatVoltage(ambientTemp) )
								floatFlag = true;

							if ( !adsorptionFlag && !adsorptionComplete && floatFlag && (vBat >= AdsorptionVoltage(ambientTemp) ) )
							{
								adsorptionFlag = true;
								floatFlag = true;
								adsorptionTime = 0;
							}

							if (adsorptionFlag && floatFlag && !adsorptionComplete)
							{

								if (vBat >= AdsorptionVoltage(ambientTemp) )
								{
									isCharging = false;
									canCharge = false;
//									switchCharger(OFF);
//									switchSolarArray(OFF);
									changePWM_TIM1(PCT80_DUTY_CYCLE, OFF);
									mpptBypass(OFF);
								}
							}
							else if (!adsorptionFlag && floatFlag && adsorptionComplete)
							{

								if (vBat >= FloatVoltage(ambientTemp) + (double)0.25 )
								{
									isCharging = false;
									canCharge = false;
//									switchCharger(OFF);
//									switchSolarArray(OFF);
									changePWM_TIM1(PCT80_DUTY_CYCLE, OFF);
									mpptBypass(OFF);
								}
							}
							else
							{
								// Do Nothing
							}
						} // end while (isCharging)
					} //end else
				} //end while (canCharge)
			} // end if (vSolarArray > (vBattery + TWO_VOLT))

			// Solar array voltage is high enough to de-sulfate the batteries but not high enough to charge them
			else if ( (vSolarArray > vBattery) && (vSolarArray < (vBattery + TWO_VOLT)))
			{
				switchCharger(OFF);
				switchSolarArray(ON);
				changePWM_TIM1(PCT80_DUTY_CYCLE, OFF);
				canCharge = false;
				isCharging = false;
				mpptBypass(OFF);

				if (canPulse == pulseInterval)
				{
					pulse();
					canPulse = 0;
				}
			}

			// Don't do anything if the solar array voltage is below battery voltage.
			else
			{
				switchCharger(OFF);
				switchSolarArray(OFF);
				changePWM_TIM1(PCT80_DUTY_CYCLE, OFF);
				mpptBypass(OFF);
				canCharge = false;
				isCharging = false;
				canPulse = 0;
//				adsorptionFlag = false;
//				adsorptionComplete = false;
			}
		} // end if (vBattery > BAT_DROP_DEAD_VOLT)

		else // If batteries are below 8v, just keep these all off
		{
			switchCharger(OFF);
			switchSolarArray(OFF);
			changePWM_TIM1(PCT80_DUTY_CYCLE, OFF);
			mpptBypass(OFF);
			canCharge = false;
			isCharging = false;
			canPulse = 0;
			adsorptionComplete = false;
			adsorptionFlag = false;
			floatFlag = false;
		}
	} //end while
}


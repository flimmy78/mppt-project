/** HD44780.c
 * Source file for controlling HD44780 based LCD displays
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
 * TARGET DISPLAY: New Haven Display NHD-0216HZ-FSW-FBW-33V3C (should also be compatible with any HC44780 based display) in 4 bit mode
 * Developed using STM32CubeF4 HAL and API version 1.18.0
 *
 *
 * REVISION HISTORY
 *
 * 1.0: 12/27/2017	Created By Nicholas C. Ipri (NCI) nipri@solartechnology.com
 * 		This initial version supplies only very basic functionality to initialize and write the display in 4 bit mode
 */


#include "HD44780.h"
#include "stm32f4xx_hal.h"
#include "mppt.h"

#include <string.h>

void HD44780_WriteCommand(uint8_t data) {

	HD44780_CommandMode;
	HD44780_ClearRW;

	delay_us(50); //50

	if ( (data==0x03) || (data== 0x02) )  { // Used only for initialization

		if ( (data&0x08) == 0x08 )
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);

		if ( (data&0x04) == 0x04)
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET);

		if ( (data&0x02) == 0x02)
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);

		if ( (data&0x01) == 0x01)
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET);

		delay_us(50);
		HD44780_SetEnable;
		delay_us(50);
		HD44780_ClearEnable;
	}

	else {

		if ( (data&0x80) == 0x80 )
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);

		if ( (data&0x40) == 0x40)
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET);

		if ( (data&0x20) == 0x20)
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);

		if ( (data&0x10) == 0x10)
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET);

		delay_us(50);
		HD44780_SetEnable;
		delay_us(50);
		HD44780_ClearEnable;

		if ( (data&0x08) == 0x08 )
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);

		if ( (data&0x04) == 0x04)
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET);

		if ( (data&0x02) == 0x02)
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);

		if ( (data&0x01) == 0x01)
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET);

		delay_us(50);
		HD44780_SetEnable;
		delay_us(50);
		HD44780_ClearEnable;
	}
}

void HD44780_WriteData(uint8_t row, uint8_t col, char *data, uint8_t clearDisplay) {

	uint8_t length;
	uint8_t i;

	HD44780_ClearRW;

	if (clearDisplay)
	{
		HD44780_WriteCommand(CLEAR_DISPLAY);
		delay_us(2000);
	}

	HD44780_GotoXY(row, col);
	delay_us(5000);

	HD44780_DataMode;
	delay_us(50);

	length = strlen(data);

	for(i=0; i<length; i++) {

		if ( (*data & 0x80) == 0x80 )
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);

		if ( (*data & 0x40) == 0x40)
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET);

		if ( (*data & 0x20) == 0x20)
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);

		if ( (*data & 0x10) == 0x10)
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET);

		delay_us(50);
		HD44780_SetEnable;
		delay_us(50);
		HD44780_ClearEnable;

		if ( (*data & 0x08) == 0x08 )
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);

		if ( (*data & 0x04) == 0x04)
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_5, GPIO_PIN_RESET);

		if ( (*data & 0x02) == 0x02)
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);

		if ( (*data & 0x01) == 0x01)
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET);

		delay_us(50);
		HD44780_SetEnable;
		delay_us(50);
		HD44780_ClearEnable;

		data++;
	}
}

void HD44780_GotoXY(uint8_t row, uint8_t col) {

	uint8_t row_offsets[] = {0x00, 0x40};
	HD44780_WriteCommand(SET_DDRAM_ADDRESS | (col + row_offsets[row]));
}

void HD44780_Init() {

	HD44780_WriteCommand(0x03);
	delay_us(50);

	HD44780_WriteCommand(0x03);
	delay_us(50);

	HD44780_WriteCommand(0x03);
	delay_us(50);

	HD44780_WriteCommand(0x02);
	delay_us(50);

	HD44780_WriteCommand(FUNCTION_SET | SET_2LINE);
	delay_us(50);

	HD44780_WriteCommand(DISPLAY_ON_OFF_CONTROL | SET_DISPLAY_ON | SET_CURSOR_OFF);
	delay_us(50);

	HD44780_WriteCommand(CLEAR_DISPLAY);
	delay_us(50);

	HD44780_WriteCommand(ENTRY_MODE_SET | SET_CURSOR_INC);
	delay_us(2000);

}





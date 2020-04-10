#include "driver/dac_driver.h"
#include "stm32f10x.h"
#include "macros.h"

#define DAC_SYNCC_LOW GPIO_ResetBits(GPIOA, GPIO_Pin_6);
#define DAC_SYNCC_HI GPIO_SetBits(GPIOA, GPIO_Pin_6);
#define DAC_DATA_LOW GPIO_ResetBits(GPIOA, GPIO_Pin_4);
#define DAC_DATA_HI GPIO_SetBits(GPIOA, GPIO_Pin_4);
#define DAC_CLK_LOW GPIO_ResetBits(GPIOA, GPIO_Pin_5);
#define DAC_CLK_HI GPIO_SetBits(GPIOA, GPIO_Pin_5);
#define DAC_SYNCU_LOW GPIO_ResetBits(GPIOA, GPIO_Pin_3);
#define DAC_SYNCU_HI GPIO_SetBits(GPIOA, GPIO_Pin_3);

/* Send new 16 bit DAC value */
void sendDACValue(unsigned short dac_value) {
	unsigned char i;
	unsigned short buffer = 0;
	buffer = (buffer | (dac_value<<2)) & 0x3FFF;

	for (i=0;i<16;i++) {
		if (CHECK_BIT(buffer, 15)) {
			DAC_DATA_HI;
		}
		else {
			DAC_DATA_LOW;
		}
		DAC_CLK_LOW;
		DAC_CLK_HI;
		buffer = buffer << 1;
	}
}

/* Update current DAC */
void dac_writeCDAC(unsigned short dac_value) {
	DAC_SYNCC_LOW;
	sendDACValue(dac_value);
	DAC_SYNCC_HI;
}

/* Update voltage DAC */
void dac_writeUDAC(unsigned short dac_value) {
	DAC_SYNCU_LOW;
	sendDACValue(dac_value);
	DAC_SYNCU_HI;
}

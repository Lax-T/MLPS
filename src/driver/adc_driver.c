#include "driver/adc_driver.h"
#include "stm32f10x.h"
#include "macros.h"

#define ADC_CS_LOW GPIO_ResetBits(GPIOB, GPIO_Pin_2);
#define ADC_CS_HI GPIO_SetBits(GPIOB, GPIO_Pin_2);
#define ADC_CLK_LOW GPIO_ResetBits(GPIOB, GPIO_Pin_1);
#define ADC_CLK_HI GPIO_SetBits(GPIOB, GPIO_Pin_1);
#define ADC_DIN_LOW GPIO_ResetBits(GPIOB, GPIO_Pin_11);
#define ADC_DIN_HI GPIO_SetBits(GPIOB, GPIO_Pin_11);
#define ADC_DOUT_PORT GPIOB
#define ADC_DOUT_PIN GPIO_Pin_10
// ADS1220 command definitions
#define ADC_CM_RDATA 0x10
#define ADC_CM_WREG 0x42
#define ADC_CM_START 0x80
// Configuration register values for voltage conversion
#define ADC_CONF_VOLT_R0 0x60
#define ADC_CONF_VOLT_R1 0x20
#define ADC_CONF_VOLT_R2 0x00
// Configuration register values for current conversion
#define ADC_CONF_CURR_R0 0x50
#define ADC_CONF_CURR_R1 0x20
#define ADC_CONF_CURR_R2 0x00


/* Write byte to ADC */
void writeByte(unsigned char data ) {
	unsigned char i;

	for (i=0;i<8;i++) {
		if (CHECK_BIT(data, 7)) {
			ADC_DIN_HI;
		}
		else {
			ADC_DIN_LOW;
		}
		ADC_CLK_HI;
		ADC_CLK_LOW;
		data = data << 1;
	}
}

/* Read byte from ADC */
unsigned char readByte() {
	unsigned char i;
	unsigned char data = 0;

	for (i=0;i<8;i++) {
		ADC_CLK_HI;
		data = data << 1;
		if ( GPIO_ReadInputDataBit(ADC_DOUT_PORT, ADC_DOUT_PIN) ) {
			data = data | 0x01;
		}
		ADC_CLK_LOW;
	}
	return data;
}

/* Write to ADC configuration registers */
void configureADC(unsigned char reg0, unsigned char reg1, unsigned char reg2) {
	ADC_CS_LOW;
	writeByte(ADC_CM_WREG);
	writeByte(reg0);
	writeByte(reg1);
	writeByte(reg2);
	ADC_CS_HI;
}

/* Send start conversion command to ADC */
void startADCConversion() {
	ADC_CS_LOW;
	writeByte(ADC_CM_START);
	ADC_CS_HI;
}

/* Start voltage conversion */
void adc_StartVoltageConvertion() {
	configureADC(ADC_CONF_VOLT_R0, ADC_CONF_VOLT_R1, ADC_CONF_VOLT_R2);
	startADCConversion();
}

/* Start current conversion */
void adc_StartCurrentConversion() {
	configureADC(ADC_CONF_CURR_R0, ADC_CONF_CURR_R1, ADC_CONF_CURR_R2);
	startADCConversion();
}

/* Read 24 bit conversion result */
unsigned int adc_ReadData() {
	unsigned int value = 0;
	ADC_CS_LOW;
	writeByte(ADC_CM_RDATA);
	value = value | readByte();
	value = value << 8;
	value = value | readByte();
	value = value << 8;
	value = value | readByte();
	ADC_CS_HI;
	return value;
}

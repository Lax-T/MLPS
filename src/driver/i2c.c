#include "driver/i2c.h"
#include "stm32f10x.h"
#include "macros.h"
#include "delays.h"

#define I2C_SDA_LOW GPIO_ResetBits(GPIOB, GPIO_Pin_5);
#define I2C_SDA_HI GPIO_SetBits(GPIOB, GPIO_Pin_5);
#define I2C_SDA_PORT GPIOB
#define I2C_SDA_PIN GPIO_Pin_5
#define I2C_SCL_LOW GPIO_ResetBits(GPIOB, GPIO_Pin_4);
#define I2C_SCL_HI GPIO_SetBits(GPIOB, GPIO_Pin_4);

void i2c_Start() {
	I2C_SDA_HI;
	uSDelay(USDELAY_CALC(4));
	I2C_SCL_HI;
	uSDelay(USDELAY_CALC(4));
	I2C_SDA_LOW;
	uSDelay(USDELAY_CALC(4));
	I2C_SCL_LOW;
}

void i2c_Stop() {
	I2C_SDA_LOW;
	uSDelay(USDELAY_CALC(4));
	I2C_SCL_HI;
	uSDelay(USDELAY_CALC(4));
	I2C_SDA_HI;
	uSDelay(USDELAY_CALC(4));
}

void i2c_SAck() {
	I2C_SDA_HI;
	uSDelay(USDELAY_CALC(4));
	I2C_SCL_HI;
	uSDelay(USDELAY_CALC(4));
	I2C_SCL_LOW;
	uSDelay(USDELAY_CALC(4));
}

void i2c_MAck() {
	I2C_SDA_LOW;
	uSDelay(USDELAY_CALC(4));
	I2C_SCL_HI;
	uSDelay(USDELAY_CALC(4));
	I2C_SCL_LOW;
	uSDelay(USDELAY_CALC(4));
	I2C_SDA_HI;
	uSDelay(USDELAY_CALC(4));
}

void i2c_MNAck() {
	I2C_SDA_HI;
	uSDelay(USDELAY_CALC(4));
	I2C_SCL_HI;
	uSDelay(USDELAY_CALC(4));
	I2C_SCL_LOW;
	uSDelay(USDELAY_CALC(4));
}

void i2c_TXByte(unsigned char data) {
	char i = 0;
	for (i=0; i < 8; i++) {
		if (CHECK_BIT(data, 7)) {
			I2C_SDA_HI;
		}
		else {
			I2C_SDA_LOW;
		}
		uSDelay(USDELAY_CALC(4));
		I2C_SCL_HI;
		uSDelay(USDELAY_CALC(4));
		I2C_SCL_LOW;
		uSDelay(USDELAY_CALC(4));
		data = data << 1;
	}
}

unsigned char i2c_RXByte() {
	unsigned char data=0;
	char i = 0;
	for (i=0; i < 8; i++) {
		data = data << 1;
		I2C_SCL_HI;
		uSDelay(USDELAY_CALC(4));
		if ( GPIO_ReadInputDataBit(I2C_SDA_PORT, I2C_SDA_PIN) ) {
			data = data | 0x01;
		}
		uSDelay(USDELAY_CALC(4));
		I2C_SCL_LOW;
		uSDelay(USDELAY_CALC(4));
	}
	return data;
}

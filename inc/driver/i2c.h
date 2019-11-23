#include "stm32f10x.h"
#include "macros.h"
#include "delays.h"

#define I2C_SDA_LOW GPIO_ResetBits(GPIOB, GPIO_Pin_5);
#define I2C_SDA_HI GPIO_SetBits(GPIOB, GPIO_Pin_5);
#define I2C_SDA_PORT GPIOB
#define I2C_SDA_PIN GPIO_Pin_5

#define I2C_SCL_LOW GPIO_ResetBits(GPIOB, GPIO_Pin_4);
#define I2C_SCL_HI GPIO_SetBits(GPIOB, GPIO_Pin_4);

void i2c_Start();
void i2c_Stop();
void i2c_SAck();
void i2c_MAck();
void i2c_MNAck();
void i2c_TXByte(unsigned char data);
unsigned char i2c_RXByte();

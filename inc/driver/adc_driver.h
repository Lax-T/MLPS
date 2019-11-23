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


void adcStartVoltageConvertion();
void adcStartCurrentConversion();
unsigned int adcReadData();

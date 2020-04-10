#include "init.h"
#include "stm32f10x.h"

GPIO_InitTypeDef GPIOInit;
TIM_TimeBaseInitTypeDef  TIMInit;
DAC_InitTypeDef DACInit;
ADC_InitTypeDef ADCInit;

void init() {

	// Enable peripheral clock
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6 , ENABLE);

	// Set system clock to HSI > PLL x4 (16 MHz)
	RCC_SYSCLKConfig(RCC_SYSCLKSource_HSI);
	RCC_PLLCmd(DISABLE);
	RCC_PLLConfig(RCC_PLLSource_HSI_Div2, RCC_PLLMul_4);
	RCC_PLLCmd(ENABLE);
	// If PLL is not starting with internal RC something is really wrong...
	while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
		{
		}
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

	// PORT A init
	GPIOInit.GPIO_Mode = GPIO_Mode_AIN;
	GPIOInit.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2;
	GPIO_Init(GPIOA, &GPIOInit);

	GPIOInit.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIOInit.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_11 | GPIO_Pin_12;
	GPIOInit.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init(GPIOA, &GPIOInit);

	GPIO_ResetBits(GPIOA, GPIO_Pin_12); // DISP clock low
	GPIO_ResetBits(GPIOA, GPIO_Pin_11); // DISP driver latch low
	GPIO_SetBits(GPIOA, GPIO_Pin_3); // DAC SYNC high
	GPIO_SetBits(GPIOA, GPIO_Pin_5); // DAC CLOCK high
	GPIO_SetBits(GPIOA, GPIO_Pin_6); // DAC SYNC high

	// PORT B init
	GPIOInit.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIOInit.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_11 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
	GPIOInit.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init(GPIOB, &GPIOInit);
	GPIO_SetBits(GPIOB, GPIO_Pin_14); // DISP OE high (output disable)
	GPIO_ResetBits(GPIOB, GPIO_Pin_13); // DISP register latch low
	GPIO_SetBits(GPIOB, GPIO_Pin_2); // ADC CS HI
	GPIO_ResetBits(GPIOB, GPIO_Pin_1); // ADC CLK LOW
	GPIO_ResetBits(GPIOB, GPIO_Pin_6); // FAN OFF
	GPIO_ResetBits(GPIOB, GPIO_Pin_7); // OUT OFF

	GPIOInit.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIOInit.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_4;  // EEPROM SDA, SCL
	GPIOInit.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init(GPIOB, &GPIOInit);
	GPIO_SetBits(GPIOB, GPIO_Pin_5);
	GPIO_SetBits(GPIOB, GPIO_Pin_4);

	// TIMER 6 init - delays counter
	TIMInit.TIM_Period  = 0xffff;
	TIMInit.TIM_Prescaler = (1)-1;
	TIMInit.TIM_ClockDivision = TIM_CKD_DIV1;
	TIMInit.TIM_CounterMode =  TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM6, &TIMInit);
	TIM_Cmd(TIM6, ENABLE);

	// ADC init
	RCC_ADCCLKConfig(RCC_PCLK2_Div2);
	ADCInit.ADC_Mode = ADC_Mode_Independent;
	ADCInit.ADC_ScanConvMode = DISABLE;
	ADCInit.ADC_ContinuousConvMode = DISABLE;
	ADCInit.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
	ADCInit.ADC_DataAlign = ADC_DataAlign_Right;
	ADCInit.ADC_NbrOfChannel = 1;
	ADC_Init ( ADC1, &ADCInit);

	ADC_Cmd (ADC1, ENABLE);  //enable ADC1

	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
}


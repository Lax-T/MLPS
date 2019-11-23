
#define DAC_SYNCC_LOW GPIO_ResetBits(GPIOA, GPIO_Pin_6);
#define DAC_SYNCC_HI GPIO_SetBits(GPIOA, GPIO_Pin_6);
#define DAC_DATA_LOW GPIO_ResetBits(GPIOA, GPIO_Pin_4);
#define DAC_DATA_HI GPIO_SetBits(GPIOA, GPIO_Pin_4);
#define DAC_CLK_LOW GPIO_ResetBits(GPIOA, GPIO_Pin_5);
#define DAC_CLK_HI GPIO_SetBits(GPIOA, GPIO_Pin_5);
#define DAC_SYNCU_LOW GPIO_ResetBits(GPIOA, GPIO_Pin_3);
#define DAC_SYNCU_HI GPIO_SetBits(GPIOA, GPIO_Pin_3);


void dac_writeCDAC(unsigned short dac_value);
void dac_writeUDAC(unsigned short dac_value);

#define DISP_DR_LE_LOW GPIO_ResetBits(GPIOA, GPIO_Pin_11);
#define DISP_DR_LE_HI GPIO_SetBits(GPIOA, GPIO_Pin_11);
#define DISP_CLK_LOW GPIO_ResetBits(GPIOA, GPIO_Pin_12);
#define DISP_CLK_HI GPIO_SetBits(GPIOA, GPIO_Pin_12);

#define DISP_REG_LD_LOW GPIO_ResetBits(GPIOB, GPIO_Pin_13);
#define DISP_REG_LD_HI GPIO_SetBits(GPIOB, GPIO_Pin_13);
#define DISP_OE_LOW GPIO_ResetBits(GPIOB, GPIO_Pin_14);
#define DISP_OE_HI GPIO_SetBits(GPIOB, GPIO_Pin_14);
#define DISP_DOUT_LOW GPIO_ResetBits(GPIOB, GPIO_Pin_15);
#define DISP_DOUT_HI GPIO_SetBits(GPIOB, GPIO_Pin_15);

void dc_taskDisplayControl(void *arg);
void dc_writeDisplayBuffer(unsigned char buffer[], unsigned char leds);

#include "stm32f10x.h"

#define INPUT_VOLTAGE_CORR_COEFF 8.55
#define ZERO_DISPLAY_TRESHOLD 1000
#define VREG_ERR_TRESHOLD 100
#define VREG_ERR_MIN_CURRENT 20
#define CREG_ERR_TRESHOLD 200
#define REG_FAIL_DELAY_CYCLES 3

#define mFAN_OFF GPIO_ResetBits(GPIOB, GPIO_Pin_6);
#define mFAN_ON GPIO_SetBits(GPIOB, GPIO_Pin_6);
#define mOUT_OFF GPIO_ResetBits(GPIOB, GPIO_Pin_7);
#define mOUT_ON GPIO_SetBits(GPIOB, GPIO_Pin_7);
#define FAN_ON_TEMP 50
#define FAN_OFF_TEMP 40
#define OTP_TRIG_TEMP 80
#define OTP_FALLBACK_TEMP 65

void sys_taskSystemControl(void *arg);

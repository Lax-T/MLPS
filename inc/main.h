#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "tasks/display_control.h"
#include "tasks/system.h"
#include "tasks/ui_control.h"
#include "tasks/adc_readout.h"
#include "globals.h"
#include "init.h"

#define DAC_RESOLUTION_10B
//#define DAC_RESOLUTION_12B

#if defined(DAC_RESOLUTION_10B) && defined (DAC_RESOLUTION_12B)
#error Invalid DAC resolution definition!
#endif
#if !defined(DAC_RESOLUTION_10B) && !defined (DAC_RESOLUTION_12B)
#error DAC resolution is not defined
#endif
/*
 *** PORT A ***
 *
 * DAC_SYNCC - 3
 * DAC_DATA - 4
 * DAC_CLK - 5
 * DAC_SYNCU - 6
 *
 * DISP_DR_LE - 11
 * DISP_CLK - 12
 *
 *
 *** PORT B ***
 *
 * CV_SENSE - 9
 * CC_SENSE - 8
 *
 * OUT_ON - 7
 * FAN_ON - 6
 *
 * I2C_SDA - 5
 * I2C_SCL - 4
 *
 * DISP_DOUT - 15
 * DISP_OE - 14
 * DISP_REG_LD - 13
 * DISP_DIN - 12
 *
 * ADC_CLK - 1
 * ADC_CS - 2
 * ADC_DOUT - 10
 * ADC_DIN - 11
 *
 * */

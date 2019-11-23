#include "stm32f10x.h"
#include "delays.h"

void iadcInit();
unsigned short iadcMeasureInputVoltage();
unsigned short iadcMeasureRegTemp();
unsigned char iadc_validateResult(unsigned short value);

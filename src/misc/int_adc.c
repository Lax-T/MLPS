#include "misc/int_adc.h"

#define LOW_ADC_TRESHOLD 5
#define HI_ADC_TRESHOLD 0x0FFC

void iadcInit() {
	// ADC calibration (optional, but recommended at power on)
	ADC_ResetCalibration(ADC1); // Reset previous calibration
	while(ADC_GetResetCalibrationStatus(ADC1));
	ADC_StartCalibration(ADC1); // Start new calibration (ADC must be off at that time)
	while(ADC_GetCalibrationStatus(ADC1));
}

unsigned short iadcMeasureInputVoltage() {
	ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_28Cycles5); // define regular conversion config
	ADC_Cmd (ADC1,ENABLE);  //enable ADC1
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
	uSDelay(USDELAY_CALC(10));
	return ADC_GetConversionValue(ADC1);
}

unsigned short iadcMeasureRegTemp() {
	ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 1, ADC_SampleTime_28Cycles5); // define regular conversion config
	ADC_Cmd (ADC1,ENABLE);  //enable ADC1
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
	uSDelay(USDELAY_CALC(10));
	return ADC_GetConversionValue(ADC1);

}

unsigned char iadc_validateResult(unsigned short value) {
	if (value > LOW_ADC_TRESHOLD && value < HI_ADC_TRESHOLD) {
		return 1;
	}
	else {
		return 0;
	}
}


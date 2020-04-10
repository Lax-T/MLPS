#include "misc/int_adc.h"
#include "stm32f10x.h"
#include "delays.h"

#define LOW_ADC_TRESHOLD 5
#define HI_ADC_TRESHOLD 0x0FFC
#define CONVERSION_DELAY USDELAY_CALC(10)
#define CALLIBRATION_DELAY USDELAY_CALC(100)
#define CALIBRATION_MAX_CYCLES 100

/* Initialize and calibrate internal ADC */
void iadc_internalADCInit() {
	unsigned char i;
	// Reset previous calibration
	ADC_ResetCalibration(ADC1);
	// Wait for calibration reset
	i = 0;
	while(ADC_GetResetCalibrationStatus(ADC1)) {
		// Calibration fail is not system critical because it's not the main ADC,
		// but break added to avoid stalling entire program if something goes wrong.
		if (++i >= CALIBRATION_MAX_CYCLES) {
			break;
		}
	};
	// Start new calibration
	ADC_StartCalibration(ADC1);
	// Wait for calibration to complete
	i = 0;
	while(ADC_GetCalibrationStatus(ADC1)) {
		if (++i >= CALIBRATION_MAX_CYCLES) {
			break;
		}
	};
}

/* Measure value from input voltage channel */
unsigned short iadc_measureInputVoltage() {
	ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_28Cycles5); // define regular conversion config
	ADC_Cmd (ADC1,ENABLE);  //enable ADC1
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
	uSDelay(CONVERSION_DELAY);
	return ADC_GetConversionValue(ADC1);
}

/* Measure value from temperature sensor channel */
unsigned short iadc_measureRegTemp() {
	ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 1, ADC_SampleTime_28Cycles5); // define regular conversion config
	ADC_Cmd (ADC1,ENABLE);  //enable ADC1
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
	uSDelay(CONVERSION_DELAY);
	return ADC_GetConversionValue(ADC1);
}

/* Validate if conversion results is within thresholds */
unsigned char iadc_validateResult(unsigned short value) {
	if (value > LOW_ADC_TRESHOLD && value < HI_ADC_TRESHOLD) {
		return 1;
	}
	else {
		return 0;
	}
}

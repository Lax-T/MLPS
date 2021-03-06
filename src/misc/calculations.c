#include "misc/calculations.h"

static float ntc_corr_constants[] = {
		352, 129, 101.6, 86.57, 76.3, 68.45, 62.07, 56.66,
	    51.92, 47.69, 43.83, 40.26, 36.92, 33.75, 30.72, 27.8,
	    24.96, 22.16, 19.4, 16.65, 13.88, 11.07, 8.19, 5.213,
	    2.094, -1.22};

/* Calculate temperature from ADC RAW value for 10K NTC */
unsigned short cl_calculateTemperature(unsigned short adc_value) {
	unsigned char range;
	float temp;
	adc_value = adc_value >> 2;
	// Check if value out of range
	if (adc_value >= 787) {return 0;}
	if (adc_value <= 69) {return 99;}
	// Divide ADC value by 32 to define range
	range = adc_value >> 5;
	// Get range from lookup table and define step per ADC LSB for this range
	temp = (ntc_corr_constants[range] - ntc_corr_constants[range + 1]) / 32;
	// Calculate offset for given range by multiplying step and 5 ADC LSB
	temp = (adc_value & 0x1F) * temp;
	// Calculate final value. Range base value + calculated offset
	temp = ntc_corr_constants[range] - temp;
	return (unsigned short) temp;
}

/* Check if external ADC RAW value is above threshold.
 *  Negative value is masked, zero is always returned. */
unsigned char cl_checkOverTreshold(unsigned int raw_value, unsigned short treshold) {
	if ((raw_value & 0x00800000) != 0) { //Value is negative
		return 0;
	}
	else if (raw_value > treshold) {
		return 1;
	}
	else {
		return 0;
	}
}

/* Calculate voltage/current value from external ADC RAW value.
 * Both range coefficient and zero offset are applied */
unsigned short cl_calculateADCValue(unsigned int raw_value, float correction_coeff, unsigned short zero_offset) {
	signed int temp;
	float val;
	unsigned short result, rest;

	// Handling negative value
	if ((raw_value & 0x00800000) != 0) { // If negative value
		temp = (~raw_value & 0x007FFFFF) + 1;
	}
	else {
		temp = raw_value;
	}
	// Offset correction
	if (zero_offset != 0) {
		if ((zero_offset & 0x8000) != 0) { // If negative correction
			zero_offset &= 0x7FFF;
			temp -= zero_offset;
		}
		else { // If positive correction
			temp += zero_offset;
		}
	}
	// Zero check
	if (temp <= 0) {
		return 0;
	}
	// Apply correction
	val = (temp >> 5) / correction_coeff;
	result = (unsigned short)val;
	rest = (unsigned short)((val - result) * 10);
	// If rest from devision is >= 5, increment result by one.
	if (rest >= 6) {
		result++;
	}
	return result;
}

/* Calculate RAW value for external voltage/current DAC.
 * Both range coefficient and zero offset are applied */
unsigned short cl_calculateDACValue(unsigned short raw_value, float correction_coeff, unsigned short zero_offset) {
	float val;
	unsigned short result, rest;
	val = raw_value * correction_coeff;
	result = (unsigned short)val;
	rest = (unsigned short)((val - result) * 10);

	if (rest >= 6) {
		result++;
	}
	if (zero_offset != 0) {
		if ((zero_offset & 0x8000) != 0) {
			zero_offset &= 0x7FFF;
			if (zero_offset < result) {
				result -= zero_offset;
			}
			else {
				result = 0;
			}
		}
		else {
			result += zero_offset;
		}
	}

	return result;
}

/* Average data set of RAW external ADC values.
 * Used to increase correction accuracy. */
signed int cl_averageDataset(unsigned int dataset[], unsigned char dataset_size) {
	signed int temp = 0;
	unsigned char i;
	for (i=0; i<dataset_size; i++) {
		if ((dataset[i] & 0x00800000) == 0) { // If value is positive
			temp += dataset[i];
		}
		else { // If value is negative
			temp -= (~dataset[i] & 0x007FFFFF) + 1;
		}
	}
	if (temp != 0) {
		return (signed int)(temp / dataset_size);
	}
	else {
		return 0;
	}
}

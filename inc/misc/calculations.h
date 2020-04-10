unsigned short cl_calculateTemperature(unsigned short adc_value);
unsigned short cl_calculateADCValue(unsigned int raw_value, float correction_coeff, unsigned short zero_offset);
unsigned short cl_calculateDACValue(unsigned short raw_value, float correction_coeff, unsigned short zero_offset);
signed int cl_averageDataset(unsigned int dataset[], unsigned char dataset_size);
unsigned char cl_checkOverTreshold(unsigned int raw_value, unsigned short treshold);

#include "misc/settings_manager.h"
#include "misc/memory_manager.h"
#include "delays.h"

#define WRITE_TIMEOUT 60
#define WRITE_QUEUE_MAX_LEN 8
#define FLOAT_OP_VALUES_N 4
#define SHORT_OP_VALUES_N 8

#define SHORT_VALUES_OFFSET 10
#define NON_CRITICAL_VAL_THRESHOLD 15

float g_float_op_values[FLOAT_OP_VALUES_N];
unsigned short g_short_op_values[SHORT_OP_VALUES_N];
unsigned char g_write_queue[] = {255, 255, 255, 255, 255, 255, 255, 255};
unsigned char g_write_timeout = 0;

static float g_float_defaults[] = {137.9705, 93.6228, 1.9042, 2.7343, 8.7};
static unsigned short g_short_defaults[] = {0, 0, 0, 0, 0, 100, 100, 0};
static unsigned char g_crc8_table[] = {
    0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
  157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
   35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
  190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
   70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
  219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
  101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
  248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
  140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
   17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
  175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
   50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
  202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
   87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
  233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
  116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53
};

void sm_ScheduleValueWrite(unsigned char value_id);
unsigned char sm_RecallValue(unsigned char value_id);
void sm_StoreValue(unsigned char value_id);

/* Main methods */
float sm_GetFloatOpVal(unsigned char value_id) {
	return g_float_op_values[value_id];
}

unsigned short sm_GetShortOpVal(unsigned char value_id) {
	return g_short_op_values[value_id-SHORT_VALUES_OFFSET];
}

void sm_SetFloatOpVal(unsigned char value_id, float op_value) {
	g_float_op_values[value_id] = op_value;
	sm_ScheduleValueWrite(value_id);
}

void sm_SetShortOpVal(unsigned char value_id, unsigned short op_value) {
	g_short_op_values[value_id-SHORT_VALUES_OFFSET] = op_value;
	sm_ScheduleValueWrite(value_id);
}


unsigned char sm_LoadOpValues() {
	/* Loads all operating values (settings) from EEPROM on startup.
	 *  If operation successful returns 1, else - 0; */
	unsigned char i;
	unsigned char status_flags = 0;

	// Load float operation values
	for (i = 0; i < FLOAT_OP_VALUES_N; i++) {
		if (sm_RecallValue(i) != 1) {
			 g_float_op_values[i] = g_float_defaults[i];
			 status_flags |= BLOCK_1_ERR_BIT;
		}
	}

	// Load short operation values
	for (i = 0; i < SHORT_OP_VALUES_N; i++) {
		if (sm_RecallValue(i + SHORT_VALUES_OFFSET) != 1) {
			g_short_op_values[i] = g_short_defaults[i];
			if ((i + SHORT_VALUES_OFFSET) < NON_CRITICAL_VAL_THRESHOLD) { // System setting affected
				status_flags |= BLOCK_1_ERR_BIT;
			}
			else { // Non system critical setting affected
				status_flags |= BLOCK_2_ERR_BIT;
				sm_ScheduleValueWrite(i + SHORT_VALUES_OFFSET);
			}
		}
	}
	return status_flags;
}

void sm_ForceDumpAllOpValues() {
	unsigned char i;
	// Store float operation values
	for (i = 0; i < FLOAT_OP_VALUES_N; i++) {
		sm_StoreValue(i);
		mSDelay(10);
	}
	// Store short operation values
	for (i = 0; i < SHORT_OP_VALUES_N; i++) {
		sm_StoreValue(i + SHORT_VALUES_OFFSET);
		mSDelay(10);
	}
}

void sm_DumpOpValues() {
	/* Immediately store all staged values to EEPROM w/o exit. */
	unsigned char i, value_id;

	for (i=0; i < WRITE_QUEUE_MAX_LEN; i++) {
		value_id = g_write_queue[i];
		if (value_id != 255) {
			sm_StoreValue(value_id);
			mSDelay(10);
			g_write_queue[i] = 255;
		}
		else {
			break;
		}
	}
}

void sm_PeriodicWrite() {
	/*Wait set number of function calls and write staged values to EEPROM*/
	if (g_write_timeout > 0) {
		g_write_timeout--;
		if (g_write_timeout == 0) {
			sm_DumpOpValues();
		}
	}
}

/* Support functions */
void sm_ScheduleValueWrite(unsigned char value_id) {
	unsigned char i, temp;

	for (i=0; i < WRITE_QUEUE_MAX_LEN; i++) {
		temp = g_write_queue[i];
		if (temp == 255) {
			g_write_queue[i] = value_id;
			g_write_timeout = WRITE_TIMEOUT;
			break;
		}
		if (temp == value_id) {
			g_write_timeout = WRITE_TIMEOUT;
			break;
		}
	}
}

void sm_DecomposeFloat(float value, unsigned char decomposed_value[]) {
	unsigned short integer, fraction;
	integer = (unsigned short)value;
	fraction = (unsigned short)((value - integer)*10000);
	decomposed_value[0] = (integer >> 8) & 0xFF;
	decomposed_value[1] = integer & 0xFF;
	decomposed_value[2] = (fraction >> 8) & 0xFF;
	decomposed_value[3] = fraction & 0xFF;
}

void sm_DecomposeShort(unsigned short value, unsigned char decomposed_value[]) {
	decomposed_value[0] = (value >> 8) & 0xFF;
	decomposed_value[1] = value & 0xFF;
}

float sm_ComposeFloat(unsigned char value_components[]) {
	unsigned short temp = 0;
	float value = 0;
	temp = ((temp | value_components[0]) << 8) | value_components[1];
	value = temp;
	temp = 0;
	temp = ((temp | value_components[2]) << 8) | value_components[3];
	value = (temp / 10000.0) + value;
	return value;
}

unsigned short sm_ComposeShort(unsigned char value_components[]) {
	unsigned short value = 0;
	value = ((value | value_components[0]) << 8) | value_components[1];
	return value;
}

//Calculate memory offset address
unsigned short sm_CalcMemAddress(unsigned char value_id) {
	if (value_id == 0) {
		return 0;
	}
	if (value_id < SHORT_VALUES_OFFSET) {
		return value_id * 8;
	}
	else {
		return value_id * 4 + 40;
	}
}

unsigned char sm_RecallValue(unsigned char value_id) {
	unsigned char data_buffer[5], buffer_len, i;
	unsigned char crc = 0;

	if (value_id < SHORT_VALUES_OFFSET) {
		buffer_len = 5;
	}
	else {
		buffer_len = 3;
	}
	mem_ReadBlock(sm_CalcMemAddress(value_id), data_buffer, buffer_len);
	// CRC calculation
	for (i=0; i < buffer_len - 1; i++) {
		crc = g_crc8_table[crc ^ data_buffer[i]];
	}
	// Check if CRC is valid
	if (crc == data_buffer[buffer_len - 1]) {
		if (value_id < SHORT_VALUES_OFFSET) {
			g_float_op_values[value_id] = sm_ComposeFloat(data_buffer);
		}
		else {
			g_short_op_values[value_id - SHORT_VALUES_OFFSET] = sm_ComposeShort(data_buffer);
		}
		return 1;
	}
	else {
		return 0;
	}
}

void sm_StoreValue(unsigned char value_id) {
	unsigned char data_buffer[5], buffer_len, i;
	unsigned char crc = 0;

	if (value_id < SHORT_VALUES_OFFSET) {
		buffer_len = 5;
		sm_DecomposeFloat(g_float_op_values[value_id], data_buffer);
	}
	else {
		buffer_len = 3;
		sm_DecomposeShort(g_short_op_values[value_id - SHORT_VALUES_OFFSET], data_buffer);
	}

	// CRC calculation
	for (i=0; i < buffer_len - 1; i++) {
		crc = g_crc8_table[crc ^ data_buffer[i]];
	}
	data_buffer[buffer_len - 1] = crc;
	// Write block
	mem_WriteBlock(sm_CalcMemAddress(value_id), data_buffer, buffer_len);
}



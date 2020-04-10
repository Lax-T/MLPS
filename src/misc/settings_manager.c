#include "misc/settings_manager.h"
#include "misc/memory_manager.h"
#include "delays.h"

#define WRITE_TIMEOUT 60
#define WRITE_QUEUE_MAX_LEN 8
#define FLOAT_OP_VALUES_N 5
#define SHORT_OP_VALUES_N 8

#define SHORT_VALUES_OFFSET 10
#define NON_CRITICAL_VAL_THRESHOLD 15

float g_float_op_values[FLOAT_OP_VALUES_N];
unsigned short g_short_op_values[SHORT_OP_VALUES_N];
unsigned char g_write_queue[] = {255, 255, 255, 255, 255, 255, 255, 255};
unsigned char g_write_timeout = 0;
// Default operating values
static float g_float_defaults[] = {137.48, 98.89, 2.13, 1.53, 8.55};
static unsigned short g_short_defaults[] = {0, 0, 0, 0, 0, 100, 100, 0};
// Table for fast CRC calculation
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

void sm_scheduleValueWrite(unsigned char value_id);
unsigned char sm_recallValue(unsigned char value_id);
void sm_storeValue(unsigned char value_id);
unsigned char sm_checkMemMarkers();
void sm_writeMemMarkers();
unsigned char sm_recallAllValues();
unsigned char sm_formatMem();

/* Main methods */
/* Get float value */
float sm_getFloatOpVal(unsigned char value_id) {
	return g_float_op_values[value_id];
}

/* Get short (16 bit) value */
unsigned short sm_getShortOpVal(unsigned char value_id) {
	return g_short_op_values[value_id-SHORT_VALUES_OFFSET];
}

/* Set new float value */
void sm_setFloatOpVal(unsigned char value_id, float op_value) {
	g_float_op_values[value_id] = op_value;
	sm_scheduleValueWrite(value_id);
}

/* Set new short (16 bit) value */
void sm_setShortOpVal(unsigned char value_id, unsigned short op_value) {
	g_short_op_values[value_id-SHORT_VALUES_OFFSET] = op_value;
	sm_scheduleValueWrite(value_id);
}

/* Loads all operating values (settings) from EEPROM on startup. */
unsigned char sm_loadOpValues() {
	// Check memory markers
	if (sm_checkMemMarkers()) { // If markers are OK, proceed with normal load
		return sm_recallAllValues();
	} else {
		// If markers are wrong, format memory and try to read all values again to check memory
		// Additional reread is perform to validate all memory cells. It's done, because it's unclear if
		// markers were wrong because new EEPROM/software update (markers changed) or memory is damaged/non functional.
		if (sm_formatMem() && sm_recallAllValues() == 0) {
			// If sm_RecallAllValues exit code zero (no errors on load), return normal format flag
			return MEM_FORMATTED;
		} else {
			// If format/reload failed return memory error flag
			return MEM_ERROR;
		}
	}
}

/* Read all memory blocks from EEPROM. If CRC for block fails,
 * load default value to remain device operational and set appropriate flag. */
unsigned char sm_recallAllValues() {
	unsigned char i;
	unsigned char flags = 0;
	// Load float operation values
	for (i = 0; i < FLOAT_OP_VALUES_N; i++) {
		if (sm_recallValue(i) != 1) {
			 g_float_op_values[i] = g_float_defaults[i];
			 flags |= MEM_BLOCK_1_CRC;
		}
	}
	// Load short operation values
	for (i = 0; i < SHORT_OP_VALUES_N; i++) {
		if (sm_recallValue(i + SHORT_VALUES_OFFSET) != 1) {
			g_short_op_values[i] = g_short_defaults[i];
			if ((i + SHORT_VALUES_OFFSET) < NON_CRITICAL_VAL_THRESHOLD) { // System setting affected
				flags |= MEM_BLOCK_1_CRC;
			}
			else { // Non system critical setting affected
				flags |= MEM_BLOCK_2_CRC;
				sm_scheduleValueWrite(i + SHORT_VALUES_OFFSET);
			}
		}
	}
	return flags;
}

/* Load all defaults and write to memory. Also write and check memory markers.
 * Return result of markers check, 1 - check OK, 0 - check fail.  */
unsigned char sm_formatMem() {
	unsigned char i;
	// Load float defaults
	for (i = 0; i < FLOAT_OP_VALUES_N; i++) {
		g_float_op_values[i] = g_float_defaults[i];
	}
	// Load short operation values
	for (i = 0; i < SHORT_OP_VALUES_N; i++) {
		g_short_op_values[i] = g_short_defaults[i];
	}
	// Dump all values to memory
	sm_forceDumpAllOpValues();
	// Write markers
	sm_writeMemMarkers();
	// Validate
	if (sm_checkMemMarkers()) {
		return 1;
	}
	return 0;
}

/* Immediately write all values to EEPROM (staged and not staged). */
void sm_forceDumpAllOpValues() {
	unsigned char i;
	// Store float operation values
	for (i = 0; i < FLOAT_OP_VALUES_N; i++) {
		sm_storeValue(i);
		mSDelay(10);
	}
	// Store short operation values
	for (i = 0; i < SHORT_OP_VALUES_N; i++) {
		sm_storeValue(i + SHORT_VALUES_OFFSET);
		mSDelay(10);
	}
}

/* Immediately write all staged values to EEPROM. */
void sm_dumpOpValues() {
	unsigned char i, value_id;

	for (i=0; i < WRITE_QUEUE_MAX_LEN; i++) {
		// Check write queue for all non 255 (scheduled) values.
		value_id = g_write_queue[i];
		if (value_id != 255) {
			sm_storeValue(value_id);
			mSDelay(10);
			g_write_queue[i] = 255;
		}
		else {
			break;
		}
	}
}

/* Callback function, used to measure time, for delayed value write. */
void sm_periodicWrite() {
	if (g_write_timeout > 0) {
		g_write_timeout--;
		if (g_write_timeout == 0) {
			sm_dumpOpValues();
		}
	}
}

/* Support functions */
/* Add value to write queue. After each value add, write timeout is reset.
 * It's used to delay value write, to EEPROM.
 * Add value to queue and write after some time (3s or so), after value had been settled. */
void sm_scheduleValueWrite(unsigned char value_id) {
	unsigned char i, temp;

	for (i=0; i < WRITE_QUEUE_MAX_LEN; i++) {
		temp = g_write_queue[i];
		// Check queue. If 255, it means that this is the end of the queue.
		// Current value does non found in the queue and may be added.
		if (temp == 255) {
			g_write_queue[i] = value_id;
			g_write_timeout = WRITE_TIMEOUT;
			break;
		}
		// Value already in the queue, skip add.
		if (temp == value_id) {
			g_write_timeout = WRITE_TIMEOUT;
			break;
		}
	}
}

/* Decompose float value into 4 bytes.
 *  May not be the best way to do this. Review in future. */
void sm_decomposeFloat(float value, unsigned char decomposed_value[]) {
	unsigned short integer, fraction;
	integer = (unsigned short)value;
	fraction = (unsigned short)((value - integer)*10000);
	decomposed_value[0] = (integer >> 8) & 0xFF;
	decomposed_value[1] = integer & 0xFF;
	decomposed_value[2] = (fraction >> 8) & 0xFF;
	decomposed_value[3] = fraction & 0xFF;
}

/* Decompose short value into 2 bytes. */
void sm_decomposeShort(unsigned short value, unsigned char decomposed_value[]) {
	decomposed_value[0] = (value >> 8) & 0xFF;
	decomposed_value[1] = value & 0xFF;
}

/* Compose float from 4 bytes.
 * Maybe redo in future. */
float sm_composeFloat(unsigned char value_components[]) {
	unsigned short temp = 0;
	float value = 0;
	temp = ((temp | value_components[0]) << 8) | value_components[1];
	value = temp;
	temp = 0;
	temp = ((temp | value_components[2]) << 8) | value_components[3];
	value = (temp / 10000.0) + value;
	return value;
}

/* Compose short from 2 bytes. */
unsigned short sm_composeShort(unsigned char value_components[]) {
	unsigned short value = 0;
	value = ((value | value_components[0]) << 8) | value_components[1];
	return value;
}

/* Check clean memory markers. */
unsigned char sm_checkMemMarkers() {
	if (mem_readRandomByte(MEM_MARKER_H_ADR) == MEM_MARKER_H && mem_readRandomByte(MEM_MARKER_L_ADR) == MEM_MARKER_L) {
		return 1;
	}
	return 0;
}

/* Write memory markers. */
void sm_writeMemMarkers() {
	mem_writeRandomByte(MEM_MARKER_H_ADR, MEM_MARKER_H);
	mSDelay(10);
	mem_writeRandomByte(MEM_MARKER_L_ADR, MEM_MARKER_L);
	mSDelay(10);
}

/* Calculate memory block address */
unsigned short sm_calcMemAddress(unsigned char value_id) {
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

/* Read block (value) from memory and check CRC. */
unsigned char sm_recallValue(unsigned char value_id) {
	unsigned char data_buffer[5], buffer_len, i;
	unsigned char crc = 0;

	if (value_id < SHORT_VALUES_OFFSET) {
		buffer_len = 5;
	}
	else {
		buffer_len = 3;
	}
	mem_readBlock(sm_calcMemAddress(value_id), data_buffer, buffer_len);
	// CRC calculation
	for (i=0; i < buffer_len - 1; i++) {
		crc = g_crc8_table[crc ^ data_buffer[i]];
	}
	// Check if CRC is valid
	if (crc == data_buffer[buffer_len - 1]) {
		if (value_id < SHORT_VALUES_OFFSET) {
			g_float_op_values[value_id] = sm_composeFloat(data_buffer);
		}
		else {
			g_short_op_values[value_id - SHORT_VALUES_OFFSET] = sm_composeShort(data_buffer);
		}
		return 1;
	}
	else {
		return 0;
	}
}

/* Write block (value) with calculated CRC. */
void sm_storeValue(unsigned char value_id) {
	unsigned char data_buffer[5], buffer_len, i;
	unsigned char crc = 0;

	if (value_id < SHORT_VALUES_OFFSET) {
		buffer_len = 5;
		sm_decomposeFloat(g_float_op_values[value_id], data_buffer);
	}
	else {
		buffer_len = 3;
		sm_decomposeShort(g_short_op_values[value_id - SHORT_VALUES_OFFSET], data_buffer);
	}
	// CRC calculation
	for (i=0; i < buffer_len - 1; i++) {
		crc = g_crc8_table[crc ^ data_buffer[i]];
	}
	data_buffer[buffer_len - 1] = crc;
	// Write block
	mem_writeBlock(sm_calcMemAddress(value_id), data_buffer, buffer_len);
}



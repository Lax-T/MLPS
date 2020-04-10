#include "tasks/system.h"
#include "globals.h"
#include "driver/dac_driver.h"
#include "driver/adc_driver.h"
#include "tasks/ui_control.h"
#include "tasks/display_control.h"
#include "misc/settings_manager.h"
#include "misc/calculations.h"
#include "misc/int_adc.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "macros.h"
#include "delays.h"

#define SYS_TASK_TICK_PERIOD 20

#define CALL_FLAG_COLLECT_DATA 0x01
#define CALL_FLAG_DATA_READY 0x02
#define CALL_FLAG_CALL_READY 0x04

#define CALL_DATASET_SIZE 20

#define SYS_FLAG_OFF_LOCK 0x01
#define SYS_FLAG_CLL_MOD 0x04
#define SYS_FLAG_UVP 0x10
#define SYS_FLAG_T_SENSOR_FAIL 0x20
#define SYS_FLAG_OTP 0x40

unsigned char g_sys_flags = 0;
// Calibration related
unsigned char g_sys_call_type = 0;
unsigned char g_call_flags = 0;
unsigned short g_call_corr_value = 0;
unsigned int g_call_dataset[CALL_DATASET_SIZE];
unsigned char g_call_dataset_counter = 0;

extern QueueHandle_t xQueueUIEvent;
extern QueueHandle_t xQueueSysEvent;

/* Functions declarations */
void sys_checkEventQueue();
void sys_updateUISetValues();
void sys_updateUIUInTregValues(unsigned short u_in, unsigned short reg_temp);
void sys_setOutState(unsigned char new_state);
void setOutSateForCorrection();
void sys_updateUICallState(unsigned short new_state, unsigned short opt_argument);
void sys_handleCallCommand(struct SystemEventMessage sys_message);
void sys_startDataCollection();
void sys_collectDataForCall(unsigned int adc_raw);
void sys_setCallReadyState();
void sys_callibrate();
void updateUDAC(unsigned short uncorrected_value);
void updateCDAC(unsigned short uncorrected_value);
unsigned char sys_updateFanState(unsigned char current_state, unsigned short temperature, unsigned char flags);
void sys_updateUIOutState(unsigned char out_state);
void sys_updateUISettings();
void sys_sendUIErrorState(unsigned char error_code, unsigned char error_action);
void sys_sendUIOTPState(unsigned char state);
void sys_sendUIInfo(unsigned char message_id);


/* Main system task */
void sys_taskSystemControl(void *arg) {
	unsigned char tick_period_divider = 0;
	unsigned char state_counter = 0;
	unsigned int adc_raw;
	unsigned short input_voltage = 0;
	unsigned short reg_temperature = 0;
	unsigned char fan_state = 0;
	unsigned short adc_voltage = 0;
	unsigned short adc_current = 0;
	unsigned char temp_uchar = 0;
	unsigned char last_error = 0;
	unsigned char reg_fail_counter = 0;
	struct UIEventMessage ui_message;
	ui_message.type = UI_MSG_ADC_VALUES;
	// Startup operations
	// Init internal (secondary) ADC
	iadc_internalADCInit();
	// Load settings and correction coefficients from EEPROM. If 'CODE' is  non zero, send error to UI
	temp_uchar = sm_loadOpValues();
	if (MASK_BIT(temp_uchar, MEM_FORMATTED)) {
		sys_sendUIInfo(POPUP_SETT_RESET);

	} else if (temp_uchar != 0) { //If non zero code (memory error)

		if (MASK_BIT(temp_uchar, MEM_BLOCK_1_CRC)) {
			last_error = ERROR_MEM_BLOCK_1;
		}
		else if (MASK_BIT(temp_uchar, MEM_BLOCK_2_CRC)) {
			last_error = ERROR_MEM_BLOCK_2;
		}
		else if (MASK_BIT(temp_uchar, MEM_ERROR)) {
			last_error = ERROR_MEM_FAIL;
		}
		else {
			last_error = ERROR_UNKNOWN; // To avoid undefined system state
		}
		// Send message to UI, to inform user that setting/correction coefficients may be corrupted
		sys_sendUIErrorState(last_error, NON_BLOCKING_ERROR);
	}
	// Turn OUT on/off depending on setting/last state
	switch(sm_getShortOpVal(OP_VAL_SETTINGS_1) & SETT_MASK_PON_STATE) {
	case PON_STATE_OFF:
		sys_setOutState(OUT_OFF);
		sys_updateUIOutState(OUT_OFF);
		break;
	case PON_STATE_LAST:
		sys_setOutState(sm_getShortOpVal(OP_VAL_OUT_STATE));
		sys_updateUIOutState(sm_getShortOpVal(OP_VAL_OUT_STATE));
		break;
	case PON_STATE_ON:
		sys_setOutState(OUT_ON);
		sys_updateUIOutState(OUT_ON);
		break;
	}
	// Sync settings values with UI
	sys_updateUISettings();
	sys_updateUISetValues();
	//Start first, main ADC conversion (voltage)
	adc_StartVoltageConvertion();
	//Enter task loop
	while (1) {
		switch (state_counter) {
		case 1:
			adc_raw = adc_ReadData();
			// If required, collect raw ADC data for correction
			if ((g_sys_call_type == CALL_TYPE_U_HI || g_sys_call_type == CALL_TYPE_U_OFF) && CHECK_FLAG(g_call_flags, CALL_FLAG_COLLECT_DATA)) {
				sys_collectDataForCall(adc_raw);
			}
			// To avoid zero 'flickering', if ADC value below threshold - display zero
			if (cl_checkOverTreshold(adc_raw, ZERO_DISPLAY_TRESHOLD)) {
				adc_voltage = cl_calculateADCValue(adc_raw, sm_getFloatOpVal(OP_VAL_UADC_CORR), sm_getShortOpVal(OP_VAL_UADC_ZERO));
			}
			else {
				adc_voltage = 0;
			}
			adc_StartCurrentConversion();
			break;

		case 2:
			state_counter = 0; // After increment, it will be 1 (on the next cycle, case:1 will be entered)
			adc_raw = adc_ReadData();
			// If required, collect raw ADC data for correction
			if ((g_sys_call_type == CALL_TYPE_I_HI || g_sys_call_type == CALL_TYPE_I_OFF) && CHECK_FLAG(g_call_flags, CALL_FLAG_COLLECT_DATA)) {
				sys_collectDataForCall(adc_raw);
			}
			// To avoid zero 'flickering', if ADC value below threshold - display zero
			if (cl_checkOverTreshold(adc_raw, ZERO_DISPLAY_TRESHOLD)) {
				adc_current = cl_calculateADCValue(adc_raw, sm_getFloatOpVal(OP_VAL_IADC_CORR), sm_getShortOpVal(OP_VAL_IADC_ZERO));
			}
			else {
				adc_current = 0;
			}
			// If out ON and not in call mode, check for regulation fail
			if (sm_getShortOpVal(OP_VAL_OUT_STATE) && MASK_BIT(g_sys_flags, SYS_FLAG_CLL_MOD) == 0) {
				if (adc_voltage > VREG_ERR_TRESHOLD && adc_current > VREG_ERR_MIN_CURRENT && (adc_voltage - VREG_ERR_TRESHOLD) > sm_getShortOpVal(OP_VAL_SET_VOLT)) {
					reg_fail_counter++;
					if (reg_fail_counter >= REG_FAIL_DELAY_CYCLES) {
						last_error = ERROR_VOLTAGE_REG;
					}
				}
				else if (adc_current > CREG_ERR_TRESHOLD && (adc_current - CREG_ERR_TRESHOLD) > sm_getShortOpVal(OP_VAL_SET_CURR)) {
					reg_fail_counter++;
					if (reg_fail_counter >= REG_FAIL_DELAY_CYCLES) {
						last_error = ERROR_CURRENT_REG;
					}
				}
				else {
					reg_fail_counter = 0;
				}
				if (reg_fail_counter >= REG_FAIL_DELAY_CYCLES) {
					sys_setOutState(OUT_OFF);
					sys_updateUIOutState(OUT_OFF);
					g_sys_flags |= SYS_FLAG_OFF_LOCK;
					sys_sendUIErrorState(last_error, BLOCKING_ERROR);
					sm_setShortOpVal(OP_VAL_OUT_STATE, OUT_OFF);
					sm_dumpOpValues();
				}
			}
			else {
				reg_fail_counter = 0;
			}
			// Send updated values to UI
			ui_message.data[0] = adc_voltage;
			ui_message.data[1] = adc_current;
			xQueueSend(xQueueUIEvent, (void *) &ui_message, 0);
			uic_notifyUITask(UI_EVENT_WAKE_UP);
			adc_StartVoltageConvertion();
			break;
		}
		state_counter++;

		tick_period_divider++;
		if (tick_period_divider >= 2) {
			tick_period_divider = 0;
			// Since UI is button event driven, this event is used sync timed events, for example dot blinking
			uic_notifyUITask(UI_EVENT_PERIODIC);
			// Measure input voltage and regulator temperature
			input_voltage = (unsigned short)(iadc_measureInputVoltage() / sm_getFloatOpVal(OP_VAL_UIN_CORR));
			reg_temperature = iadc_measureRegTemp();
			// Check ADC output for sensor fail
			if (iadc_validateResult(reg_temperature)) {
				reg_temperature = cl_calculateTemperature(reg_temperature);
				// If temperature is valid, perform OTP check
				if (MASK_BIT(g_sys_flags, SYS_FLAG_OTP) == 0) {
					if (reg_temperature >= OTP_TRIG_TEMP) {
						g_sys_flags |= SYS_FLAG_OTP;
						sys_setOutState(OUT_OFF);
						sys_updateUIOutState(OUT_OFF);
						sys_sendUIOTPState(TRUE);
					}
				}
				else {
					if (reg_temperature <= OTP_FALLBACK_TEMP) {
						g_sys_flags &= ~SYS_FLAG_OTP;
						sys_sendUIOTPState(FALSE);
					}
				}
			}
			else {
				reg_temperature = 0; // Since data is invalid - set zero
				if (MASK_BIT(g_sys_flags, SYS_FLAG_T_SENSOR_FAIL) == 0) {
					g_sys_flags |= SYS_FLAG_T_SENSOR_FAIL;
					last_error = ERROR_T_SENSOR;
					sys_sendUIErrorState(last_error, NON_BLOCKING_ERROR);
				}
			}
			// Send update UIn and temperature values to UI
			sys_updateUIUInTregValues(input_voltage, reg_temperature);
			fan_state = sys_updateFanState(fan_state, reg_temperature, g_sys_flags);
		}

		//If conditions are met, execute calibration subtask
		if (CHECK_FLAG(g_call_flags, CALL_FLAG_DATA_READY) && CHECK_FLAG(g_call_flags, CALL_FLAG_CALL_READY)) {
			sys_callibrate();
		} else if (g_sys_call_type == CALL_TYPE_U_IN && CHECK_FLAG(g_call_flags, CALL_FLAG_CALL_READY)) {
			sys_callibrate();
		}
		// Check queue
		sys_checkEventQueue();
		// Periodic event for settings manager (to avoid creating separate thread)
		sm_periodicWrite();
		// Sleep
		vTaskDelay(SYS_TASK_TICK_PERIOD);
	}
}


/* Check and process any messages in system event queue */
void sys_checkEventQueue() {
	struct SystemEventMessage sys_message;

	while(xQueueReceive(xQueueSysEvent, &(sys_message), 0)) {
		switch (sys_message.type) {
		// Handle new DAC (output) values
		case SYS_MSG_DAC_VALUES:
			sm_setShortOpVal(OP_VAL_SET_VOLT, sys_message.data[0]);
			sm_setShortOpVal(OP_VAL_SET_CURR, sys_message.data[1]);
			// Update DAC values only if out is ON
			if (sm_getShortOpVal(OP_VAL_OUT_STATE)) {
				updateUDAC(sm_getShortOpVal(OP_VAL_SET_VOLT));
				updateCDAC(sm_getShortOpVal(OP_VAL_SET_CURR));
			}
			break;
		// New output state message (ON/OFF)
		case SYS_MSG_OUT_STATE:
			// If out is locked in OFF state or OTP, ignore change out state message
			if ((g_sys_flags & SYS_FLAG_OFF_LOCK) == 0 && (g_sys_flags & SYS_FLAG_OTP) == 0) {
				sm_setShortOpVal(OP_VAL_OUT_STATE, sys_message.data[0]);
				sys_setOutState(sm_getShortOpVal(OP_VAL_OUT_STATE));
				sys_updateUIOutState(sm_getShortOpVal(OP_VAL_OUT_STATE));
			}
			break;
		// Calibration command
		case SYS_MSG_CALL_CMD:
			sys_handleCallCommand(sys_message);
			break;
		// Message with updated settings
		case SYS_MSG_SETTINGS:
			sm_setShortOpVal(OP_VAL_SETTINGS_1, sys_message.data[0]);
			sm_dumpOpValues();
			break;
		// Message with reset command
		case SYS_MSG_RESET:
			// Turn out off before reset
			sys_setOutState(OUT_OFF);
			sys_updateUIOutState(OUT_OFF);
			// Reset settings
			if (sm_formatMem()) {
				sys_sendUIInfo(POPUP_SETT_RESET);
			} else {
				sys_sendUIInfo(POPUP_RESET_FAIL);
			}
			sys_updateUISettings();
			sys_updateUISetValues();
			break;
		}
	}
}

/* System to UI messages */
void sys_updateUISetValues() {
	struct UIEventMessage ui_message;
	ui_message.type = UI_MSG_OUT_SET_VALUES;
	ui_message.data[0] = sm_getShortOpVal(OP_VAL_SET_VOLT);
	ui_message.data[1] = sm_getShortOpVal(OP_VAL_SET_CURR);
	xQueueSend(xQueueUIEvent, (void *) &ui_message, 0);
	uic_notifyUITask(UI_EVENT_WAKE_UP);
}

void sys_updateUISettings() {
	struct UIEventMessage ui_message;
	ui_message.type = UI_MSG_SETTINGS;
	ui_message.data[0] = sm_getShortOpVal(OP_VAL_SETTINGS_1);
	ui_message.data[1] = 0;
	xQueueSend(xQueueUIEvent, (void *) &ui_message, 0);
}

void sys_updateUIUInTregValues(unsigned short u_in, unsigned short reg_temp) {
	struct UIEventMessage ui_message;
	ui_message.type = UI_MSG_UIN_TREG_VALUES;
	ui_message.data[0] = u_in;
	ui_message.data[1] = reg_temp;
	xQueueSend(xQueueUIEvent, (void *) &ui_message, 0);
}

void sys_updateUICallState(unsigned short new_state, unsigned short opt_argument) {
	struct UIEventMessage ui_message;
	ui_message.type = UI_MSG_CALL_STATE;
	ui_message.data[0] = new_state;
	ui_message.data[1] = opt_argument;
	xQueueSend(xQueueUIEvent, (void *) &ui_message, 0);
}

void sys_updateUIOutState(unsigned char out_state) {
	struct UIEventMessage ui_message;
	ui_message.type = UI_MSG_OUT_STATE;
	ui_message.data[0] = out_state;
	xQueueSend(xQueueUIEvent, (void *) &ui_message, 0);
}

void sys_sendUIErrorState(unsigned char error_code, unsigned char error_action) {
	struct UIEventMessage ui_message;
	ui_message.type = UI_MSG_ERROR;
	ui_message.data[0] = error_code;
	ui_message.data[1] = error_action;
	xQueueSend(xQueueUIEvent, (void *) &ui_message, 0);
}

void sys_sendUIOTPState(unsigned char state) {
	struct UIEventMessage ui_message;
	ui_message.type = UI_MSG_OTP_STATE;
	ui_message.data[0] = state;
	xQueueSend(xQueueUIEvent, (void *) &ui_message, 0);
}

void sys_sendUIInfo(unsigned char message_id) {
	struct UIEventMessage ui_message;
	ui_message.type = UI_MSG_INFO;
	ui_message.data[0] = message_id;
	xQueueSend(xQueueUIEvent, (void *) &ui_message, 0);
}

/* UI to system commands handlers */
/* Handle calibration command from UI */
void sys_handleCallCommand(struct SystemEventMessage sys_message) {
	switch (sys_message.data[0]) {
	// Prepare for calibration and respond to UI when ready
	case CALL_CMD_START:
		g_sys_flags |= SYS_FLAG_CLL_MOD;
		sys_setOutState(OUT_OFF);
		sys_updateUIOutState(OUT_OFF);
		sm_setShortOpVal(OP_VAL_OUT_STATE, 0); // To keep out state in sync with memory
		g_sys_call_type = sys_message.data[1];
		sys_updateUICallState(CALL_RESPONSE_READY, 0);
		break;
	// Confirmation message from UI (user) that output is in required state and out may be turned on
	case CALL_CMD_CONFIRM:
		setOutSateForCorrection();
		sys_startDataCollection();
		break;
	// Message with correction value. Last required component to calculate new calibration coefficient.
	case CALL_CMD_CORR_VALUE:
		g_call_corr_value = sys_message.data[1];
		sys_setCallReadyState();
		sys_updateUICallState(CALL_RESPONSE_STARTED, 0);
		break;
	// Command to discard and exit calibration mode.
	case CALL_CMD_RESET:
		g_sys_flags &= ~SYS_FLAG_CLL_MOD;
		sys_setOutState(OUT_OFF);
		sys_updateUIOutState(OUT_OFF);
		break;
	}
}

/* Update output sate. 0 - Out off, 1 - out on. */
void sys_setOutState(unsigned char new_state) {
	if (new_state == OUT_ON) {
		dac_writeCDAC(0x0000);
		dac_writeUDAC(0x0000);
		mSDelay(5);
		mOUT_ON;
		updateUDAC(sm_getShortOpVal(OP_VAL_SET_VOLT));
		updateCDAC(sm_getShortOpVal(OP_VAL_SET_CURR));
	}
	else {
		mOUT_OFF;
		dac_writeUDAC(0x000F);
		dac_writeCDAC(0x000F);
	}
}

/* Depending on correction type updates voltage/current DACs and turns on output. */
void setOutSateForCorrection() {
	unsigned short u_dac_value, c_dac_value;
	switch(g_sys_call_type) {
	case CALL_TYPE_U_HI:
		u_dac_value = CALL_REF_U_HI;
		c_dac_value = CALL_REF_I_HI;
		break;
	case CALL_TYPE_I_HI:
		u_dac_value = CALL_REF_U_HI;
		c_dac_value = CALL_REF_I_HI;
		break;
	case CALL_TYPE_U_OFF:
		u_dac_value = CALL_REF_U_OFF / CALL_REF_OFF_MULT;
		c_dac_value = CALL_REF_I_OFF / CALL_REF_OFF_MULT;
		dac_writeUDAC(cl_calculateDACValue(u_dac_value, sm_getFloatOpVal(OP_VAL_UDAC_CORR), 0));
		dac_writeCDAC(cl_calculateDACValue(c_dac_value, sm_getFloatOpVal(OP_VAL_IDAC_CORR), 0));
		mOUT_ON;
		return;
	case CALL_TYPE_I_OFF:
		u_dac_value = CALL_REF_U_HI;
		c_dac_value = CALL_REF_I_OFF / CALL_REF_OFF_MULT;
		dac_writeUDAC(cl_calculateDACValue(u_dac_value, sm_getFloatOpVal(OP_VAL_UDAC_CORR), 0));
		dac_writeCDAC(cl_calculateDACValue(c_dac_value, sm_getFloatOpVal(OP_VAL_IDAC_CORR), 0));
		mOUT_ON;
		return;
	}
	updateUDAC(u_dac_value);
	updateCDAC(c_dac_value);
	mOUT_ON;
}

/* Calculates, corrects and writes voltage DAC new value. */
void updateUDAC(unsigned short uncorrected_value) {
	dac_writeUDAC(cl_calculateDACValue(uncorrected_value, sm_getFloatOpVal(OP_VAL_UDAC_CORR), sm_getShortOpVal(OP_VAL_UDAC_ZERO)));
}

/* Calculates, corrects and writes current DAC new value. */
void updateCDAC(unsigned short uncorrected_value) {
	dac_writeCDAC(cl_calculateDACValue(uncorrected_value, sm_getFloatOpVal(OP_VAL_IDAC_CORR), sm_getShortOpVal(OP_VAL_IDAC_ZERO)));
}

/* Define and update fan sate considering: current state, temperature, presence of errors. */
unsigned char sys_updateFanState(unsigned char current_state, unsigned short temperature, unsigned char flags) {
	if((flags & SYS_FLAG_T_SENSOR_FAIL) != 0) { // In case of sensor fail - always ON
		mFAN_ON;
		return 1;
	}
	if (current_state == 0) { // If current state - OFF
		if (temperature >= FAN_ON_TEMP) {
			mFAN_ON;
			return 1;
		}
	}
	else { // If current state - ON
		if (temperature <= FAN_OFF_TEMP) {
			mFAN_OFF;
			return 0;
		}
	}
	return current_state;
}

/* Calibration related */
/* Set flags to start data collection, for calibration */
void sys_startDataCollection() {
	g_call_dataset_counter = 0;
	g_call_flags = g_call_flags & ~(CALL_FLAG_DATA_READY);
	g_call_flags = g_call_flags | CALL_FLAG_COLLECT_DATA;
}

/* Set flag, that required data set is collected. */
void sys_completeDataCollection() {
	g_call_flags = g_call_flags & ~(CALL_FLAG_COLLECT_DATA);
	g_call_flags = g_call_flags | CALL_FLAG_DATA_READY;
}

/* Collect ADC data for further analysis and averaging. */
void sys_collectDataForCall(unsigned int adc_raw) {
	if (g_call_dataset_counter >= 10) { // Skip first 10 measurements (wait to stabilize)
		g_call_dataset[g_call_dataset_counter - 10] = adc_raw;
	}
	g_call_dataset_counter++;
	if (g_call_dataset_counter >= (CALL_DATASET_SIZE + 10)) {
		sys_completeDataCollection();
	}
}

/* Set ready flag.
 * This indicates that when data collection is completed, main correction function may be called. */
void sys_setCallReadyState() {
	g_call_flags = g_call_flags | CALL_FLAG_CALL_READY;
}

/* Contains main calibration logic. Called when all preparations are completed, data collected. */
void sys_callibrate() {
	unsigned short offset;
	float var;
	signed int raw_adc_val, temp;
	unsigned char i;

	switch (g_sys_call_type) {
	case CALL_TYPE_U_HI:
		// Calculate DAC correction coefficient
		var = cl_calculateDACValue(CALL_REF_U_HI, sm_getFloatOpVal(OP_VAL_UDAC_CORR), sm_getShortOpVal(OP_VAL_UDAC_ZERO)) / (float)g_call_corr_value ;
		sm_setFloatOpVal(OP_VAL_UDAC_CORR, var);
		// Calculate ADC correction coefficient
		raw_adc_val = cl_averageDataset(g_call_dataset, CALL_DATASET_SIZE);
		var = (raw_adc_val >> 5) / (float)g_call_corr_value;
		sm_setFloatOpVal(OP_VAL_UADC_CORR, var);
		sm_dumpOpValues();
		break;

	case CALL_TYPE_I_HI:
		// Calculate DAC correction coefficient
		var = cl_calculateDACValue(CALL_REF_I_HI, sm_getFloatOpVal(OP_VAL_IDAC_CORR), sm_getShortOpVal(OP_VAL_IDAC_ZERO)) / (float)g_call_corr_value;
		sm_setFloatOpVal(OP_VAL_IDAC_CORR, var);
		// Calculate ADC correction coefficient
		raw_adc_val = cl_averageDataset(g_call_dataset, CALL_DATASET_SIZE);
		var = (raw_adc_val >> 5) / (float)g_call_corr_value;
		sm_setFloatOpVal(OP_VAL_IADC_CORR, var);
		sm_dumpOpValues();
		break;

	case CALL_TYPE_U_OFF:
		// Calculate DAC offset value
		if (g_call_corr_value > CALL_REF_U_OFF) {
			offset = (g_call_corr_value - CALL_REF_U_OFF) * sm_getFloatOpVal(OP_VAL_UDAC_CORR) / CALL_REF_OFF_MULT;
			offset |= 0x8000;
		}
		else {
			if (g_call_corr_value < CALL_REF_U_OFF) {
				offset = (CALL_REF_U_OFF - g_call_corr_value) * sm_getFloatOpVal(OP_VAL_UDAC_CORR) / CALL_REF_OFF_MULT;
			}
			else {
				offset = 0;
			}
		}
		sm_setShortOpVal(OP_VAL_UDAC_ZERO, offset);

		// Calculate ADC offset value
		raw_adc_val = cl_averageDataset(g_call_dataset, CALL_DATASET_SIZE);
		temp = (signed int)(sm_getFloatOpVal(OP_VAL_UADC_CORR) * g_call_corr_value / 3.125);
		if(raw_adc_val > temp) {
			offset = raw_adc_val - temp;
			offset |= 0x8000;
		}
		else if(temp > raw_adc_val) {
			offset = temp - raw_adc_val;
		}
		else {
			offset = 0;
		}
		sm_setShortOpVal(OP_VAL_UADC_ZERO, offset);
		sm_dumpOpValues();
		break;

	case CALL_TYPE_I_OFF:
		// Calculate DAC offset value
		if (g_call_corr_value > CALL_REF_I_OFF) {
			offset = (g_call_corr_value - CALL_REF_I_OFF) * sm_getFloatOpVal(OP_VAL_IDAC_CORR) / CALL_REF_OFF_MULT;
			offset |= 0x8000;
		}
		else {
			if (g_call_corr_value < CALL_REF_I_OFF) {
				offset = (CALL_REF_I_OFF - g_call_corr_value) * sm_getFloatOpVal(OP_VAL_IDAC_CORR) / CALL_REF_OFF_MULT;
			}
			else {
				offset = 0;
			}
		}
		sm_setShortOpVal(OP_VAL_IDAC_ZERO, offset);

		// Calculate ADC offset value
		raw_adc_val = cl_averageDataset(g_call_dataset, CALL_DATASET_SIZE);
		temp = (signed int)(sm_getFloatOpVal(OP_VAL_IADC_CORR) * g_call_corr_value / 3.125);
		if(raw_adc_val > temp) {
			offset = raw_adc_val - temp;
			offset |= 0x8000;
		}
		else if(temp > raw_adc_val) {
			offset = temp - raw_adc_val;
		}
		else {
			offset = 0;
		}
		sm_setShortOpVal(OP_VAL_IADC_ZERO, offset);
		sm_dumpOpValues();
		break;

	case CALL_TYPE_U_IN:
		raw_adc_val = 0;
		for (i=0; i<10; i++) {
			raw_adc_val += iadc_measureInputVoltage();
		}
		// Collected ADC raw value is nod divider by 10, because correction value is multiplied by 10
		var = raw_adc_val / (float)g_call_corr_value;
		sm_setFloatOpVal(OP_VAL_UIN_CORR, var);
		sm_dumpOpValues();
		break;
	}
	g_call_flags = 0;
	sys_updateUICallState(CALL_RESPONSE_DONE, 0);
	sys_setOutState(OUT_OFF);
	sys_updateUIOutState(OUT_OFF);
}





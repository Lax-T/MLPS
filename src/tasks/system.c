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
/*
#define COR_COEF_VOLTAGE 137.9705
#define COR_COEF_CURRENT 93.6228
#define COR_COEF_DAC_VOLTAGE 1.9042
#define COR_COEF_DAC_CURRENT 2.7343*/

#define SYS_TASK_TICK_PERIOD 20
#define CALL_FLAG_COLLECT_DATA 0
#define CALL_FLAG_DATA_READY 1
#define CALL_FLAG_CALL_READY 2
#define CALL_DATASET_SIZE 20

#define OUT_ON 1
#define OUT_OFF 0
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
void sys_SetCallReadyState();
void sys_Callibrate();
void updateUDAC(unsigned short uncorrected_value);
void updateCDAC(unsigned short uncorrected_value);
unsigned char sys_updateFanState(unsigned char current_state, unsigned short temperature, unsigned char flags);
void sys_updateUIOutState(unsigned char out_state);
void sys_updateUISettings();
void sys_sendUIErrorState(unsigned char error_code, unsigned char error_action);
void sys_sendUIOTPState(unsigned char state);


/* Main system task */
void TaskSystemControl(void *arg) {
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

	iadcInit();
	// Startup methods
	temp_uchar = sm_LoadOpValues();
	if (temp_uchar != 0) { //If non zero code (memory error)
		if (MASK_BIT(temp_uchar, BLOCK_1_ERR_BIT)) {
			last_error = ERROR_MEM_BLOCK_1;
		}
		else if (MASK_BIT(temp_uchar, BLOCK_2_ERR_BIT)) {
			last_error = ERROR_MEM_BLOCK_2;
		}
		else {
			last_error = ERROR_UNKNOWN;
		}
		sys_sendUIErrorState(last_error, NON_BLOCKING_ERROR);
	}
	switch(sm_GetShortOpVal(OP_VAL_SETTINGS_1) & SETT_MASK_PON_STATE) {
	case PON_STATE_OFF:
		sys_setOutState(0);
		sys_updateUIOutState(0);
		break;
	case PON_STATE_LAST:
		sys_setOutState(sm_GetShortOpVal(OP_VAL_OUT_STATE));
		sys_updateUIOutState(sm_GetShortOpVal(OP_VAL_OUT_STATE));
		break;
	case PON_STATE_ON:
		sys_setOutState(1);
		sys_updateUIOutState(1);
		break;
	}
	sys_updateUISettings();
	sys_updateUISetValues();

	while (1) {
		switch (state_counter) {
		case 0:
			state_counter++;
			adcStartVoltageConvertion();
			break;

		case 1:
			state_counter = 2;
			adc_raw = adcReadData();
			if ((g_sys_call_type == CALL_TYPE_U_HI || g_sys_call_type == CALL_TYPE_U_OFF) && CHECK_BIT(g_call_flags, CALL_FLAG_COLLECT_DATA)) {
				sys_collectDataForCall(adc_raw);
			}
			if (cl_checkOverTreshold(adc_raw, ZERO_TRESHOLD)) {
				adc_voltage = cl_calculateADCValue(adc_raw, sm_GetFloatOpVal(OP_VAL_UADC_CORR), sm_GetShortOpVal(OP_VAL_UADC_ZERO));
			}
			else {
				adc_voltage = 0;
			}
			adcStartCurrentConversion();
			break;

		case 2:
			state_counter = 1;
			adc_raw = adcReadData();
			if ((g_sys_call_type == CALL_TYPE_I_HI || g_sys_call_type == CALL_TYPE_I_OFF) && CHECK_BIT(g_call_flags, CALL_FLAG_COLLECT_DATA)) {
				sys_collectDataForCall(adc_raw);
			}
			if (cl_checkOverTreshold(adc_raw, ZERO_TRESHOLD)) {
				adc_current = cl_calculateADCValue(adc_raw, sm_GetFloatOpVal(OP_VAL_IADC_CORR), sm_GetShortOpVal(OP_VAL_IADC_ZERO));
			}
			else {
				adc_current = 0;
			}
			// Check for regulation fail
			if (sm_GetShortOpVal(OP_VAL_OUT_STATE) && MASK_BIT(g_sys_flags, SYS_FLAG_CLL_MOD) == 0) {
				if (adc_voltage > VREG_ERR_TRESHOLD && adc_current > VREG_ERR_MIN_CURRENT && (adc_voltage - VREG_ERR_TRESHOLD) > sm_GetShortOpVal(OP_VAL_SET_VOLT)) {
					reg_fail_counter++;
					if (reg_fail_counter >= 3) {
						last_error = ERROR_VOLTAGE_REG;
					}
				}
				else if (adc_current > CREG_ERR_TRESHOLD && (adc_current - CREG_ERR_TRESHOLD) > sm_GetShortOpVal(OP_VAL_SET_CURR)) {
					reg_fail_counter++;
					if (reg_fail_counter >= 3) {
						last_error = ERROR_CURRENT_REG;
					}
				}
				else {
					reg_fail_counter = 0;
				}
				if (reg_fail_counter >= 3) {
					sys_setOutState(0);
					sys_updateUIOutState(0);
					g_sys_flags |= SYS_FLAG_OFF_LOCK;
					sys_sendUIErrorState(last_error, BLOCKING_ERROR);
					sm_SetShortOpVal(OP_VAL_OUT_STATE, 0);
				}
			}
			else {
				reg_fail_counter = 0;
			}
			// Send updated values to UI
			ui_message.data[0] = adc_voltage;
			ui_message.data[1] = adc_current;
			xQueueSend(xQueueUIEvent, (void *) &ui_message, 0);
			uicNotifyUITask(UI_EVENT_WAKE_UP);
			adcStartVoltageConvertion();
			break;
		}

		tick_period_divider++;
		if (tick_period_divider >= 2) {
			tick_period_divider = 0;
			uicNotifyUITask(UI_EVENT_PERIODIC);
			input_voltage = (unsigned short)(iadcMeasureInputVoltage() / INPUT_VOLTAGE_CORR_COEFF);
			reg_temperature = iadcMeasureRegTemp();
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
			sys_updateUIUInTregValues(input_voltage, reg_temperature);
			fan_state = sys_updateFanState(fan_state, reg_temperature, g_sys_flags);
		}

		//If conditions are met, execute calibration task
		if (CHECK_BIT(g_call_flags, CALL_FLAG_DATA_READY) && CHECK_BIT(g_call_flags, CALL_FLAG_CALL_READY)) {
			sys_Callibrate();
			sys_setOutState(0);
			sys_updateUIOutState(0);
		}

		sys_checkEventQueue();
		sm_PeriodicWrite();
		vTaskDelay(SYS_TASK_TICK_PERIOD);
	}
}


/* Check and process any messages in system event queue */
void sys_checkEventQueue() {
	struct SystemEventMessage sys_message;

	while(xQueueReceive(xQueueSysEvent, &(sys_message), 0)) {
		switch (sys_message.type) {
		case SYS_MSG_DAC_VALUES:
			sm_SetShortOpVal(OP_VAL_SET_VOLT, sys_message.data[0]);
			sm_SetShortOpVal(OP_VAL_SET_CURR, sys_message.data[1]);
			// Update DAC values only if out is ON
			if (sm_GetShortOpVal(OP_VAL_OUT_STATE)) {
				updateUDAC(sm_GetShortOpVal(OP_VAL_SET_VOLT));
				updateCDAC(sm_GetShortOpVal(OP_VAL_SET_CURR));
			}
			break;
		case SYS_MSG_OUT_STATE:
			// If out is locked in OFF state or OTP, ignore change out state message
			if ((g_sys_flags & SYS_FLAG_OFF_LOCK) == 0 && (g_sys_flags & SYS_FLAG_OTP) == 0) {
				sm_SetShortOpVal(OP_VAL_OUT_STATE, sys_message.data[0]);
				sys_setOutState(sm_GetShortOpVal(OP_VAL_OUT_STATE));
				sys_updateUIOutState(sm_GetShortOpVal(OP_VAL_OUT_STATE));
			}
			break;
		case SYS_MSG_CALL_CMD:
			sys_handleCallCommand(sys_message);
			break;
		case SYS_MSG_SETTINGS:
			sm_SetShortOpVal(OP_VAL_SETTINGS_1, sys_message.data[0]);
			sm_DumpOpValues();
			break;
		}
	}
}

/* System to UI messages */
void sys_updateUISetValues() {
	struct UIEventMessage ui_message;
	ui_message.type = UI_MSG_OUT_SET_VALUES;
	ui_message.data[0] = sm_GetShortOpVal(OP_VAL_SET_VOLT);
	ui_message.data[1] = sm_GetShortOpVal(OP_VAL_SET_CURR);
	xQueueSend(xQueueUIEvent, (void *) &ui_message, 0);
	uicNotifyUITask(UI_EVENT_WAKE_UP);
}

void sys_updateUISettings() {
	struct UIEventMessage ui_message;
	ui_message.type = UI_MSG_SETTINGS;
	ui_message.data[0] = sm_GetShortOpVal(OP_VAL_SETTINGS_1);
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

/* UI to system commands handlers */
void sys_handleCallCommand(struct SystemEventMessage sys_message) {
	/* Handle calibration command from UI */
	switch (sys_message.data[0]) {
	case CALL_CMD_START:
		g_sys_flags |= SYS_FLAG_CLL_MOD;
		sys_setOutState(0);
		sys_updateUIOutState(0);
		sm_SetShortOpVal(OP_VAL_OUT_STATE, 0); // To keep out state in sync with memory
		g_sys_call_type = sys_message.data[1];
		sys_updateUICallState(CALL_STATE_READY, 0);
		break;
	case CALL_CMD_CONFIRM:
		setOutSateForCorrection();
		sys_startDataCollection();
		break;
	case CALL_CMD_CORR_VALUE:
		g_call_corr_value = sys_message.data[1];
		sys_SetCallReadyState();
		sys_updateUICallState(CALL_STATE_STARTED, 0);
		break;
	case CALL_CMD_RESET:
		g_sys_flags &= ~SYS_FLAG_CLL_MOD;
		sys_setOutState(0);
		sys_updateUIOutState(0);
		break;
	}
}

void sys_setOutState(unsigned char new_state) {
	/* Update output sate. 0 - Out off, 1 - out on. */
	if (new_state == 1) {
		dac_writeCDAC(0x0000);
		dac_writeUDAC(0x0000);
		mSDelay(5);
		GPIO_SetBits(GPIOB, GPIO_Pin_7);
		updateUDAC(sm_GetShortOpVal(OP_VAL_SET_VOLT));
		updateCDAC(sm_GetShortOpVal(OP_VAL_SET_CURR));
	}
	else {
		GPIO_ResetBits(GPIOB, GPIO_Pin_7);
		dac_writeUDAC(0x000F);
		dac_writeCDAC(0x000F);
	}
}

void setOutSateForCorrection() {
	/* Depending on correction type updates voltage/current DACs and turns on output. */
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
		break;
	case CALL_TYPE_I_OFF:
		u_dac_value = CALL_REF_U_HI;
		c_dac_value = CALL_REF_I_OFF / CALL_REF_OFF_MULT;
		break;
	}
	updateUDAC(u_dac_value);
	updateCDAC(c_dac_value);
	GPIO_SetBits(GPIOB, GPIO_Pin_7);
}

void updateUDAC(unsigned short uncorrected_value) {
	/* Calculates, corrects and writes voltage DAC new value. */
	dac_writeUDAC(cl_calculateDACValue(uncorrected_value, sm_GetFloatOpVal(OP_VAL_UDAC_CORR), sm_GetShortOpVal(OP_VAL_UDAC_ZERO)));
}

void updateCDAC(unsigned short uncorrected_value) {
	/* Calculates, corrects and writes current DAC new value. */
	dac_writeCDAC(cl_calculateDACValue(uncorrected_value, sm_GetFloatOpVal(OP_VAL_IDAC_CORR), sm_GetShortOpVal(OP_VAL_IDAC_ZERO)));
}

unsigned char sys_updateFanState(unsigned char current_state, unsigned short temperature, unsigned char flags) {
	/* Define and update fan sate considering: current state, temperature, presence of errors. */
	if((flags & SYS_FLAG_T_SENSOR_FAIL) != 0) { // In case of sensor fail - always ON
		FAN_ON;
		return 1;
	}
	if (current_state == 0) { // If current state - OFF
		if (temperature >= FAN_ON_TEMP) {
			FAN_ON;
			return 1;
		}
	}
	else { // If current state - ON
		if (temperature <= FAN_OFF_TEMP) {
			FAN_OFF;
			return 0;
		}
	}
	return current_state;
}

/* Calibration methods */
void sys_startDataCollection() {
	g_call_dataset_counter = 0;
	g_call_flags = g_call_flags & ~(0x01 << CALL_FLAG_DATA_READY);
	g_call_flags = g_call_flags | (0x01 << CALL_FLAG_COLLECT_DATA);
}

void sys_completeDataCollection() {
	g_call_flags = g_call_flags & ~(0x01 << CALL_FLAG_COLLECT_DATA);
	g_call_flags = g_call_flags | (0x01 << CALL_FLAG_DATA_READY);
}

void sys_collectDataForCall(unsigned int adc_raw) {
	/* Collect ADC data for further analysis and averaging. */
	if (g_call_dataset_counter >= 10) { // Skip first 10 measurements (wait to stabilize)
		g_call_dataset[g_call_dataset_counter - 10] = adc_raw;
	}
	g_call_dataset_counter++;
	if (g_call_dataset_counter >= (CALL_DATASET_SIZE + 10)) {
		sys_completeDataCollection();
	}
}

void sys_SetCallReadyState() {
	g_call_flags = g_call_flags | (0x01 << CALL_FLAG_CALL_READY);
}

void sys_Callibrate() {
	unsigned short offset;
	float var;
	signed int raw_adc_val, temp;
	switch (g_sys_call_type) {
	case CALL_TYPE_U_HI:
		// Calculate DAC correction coefficient
		var = cl_calculateDACValue(CALL_REF_U_HI, sm_GetFloatOpVal(OP_VAL_UDAC_CORR), sm_GetShortOpVal(OP_VAL_UDAC_ZERO)) / (float)g_call_corr_value ;
		sm_SetFloatOpVal(OP_VAL_UDAC_CORR, var);
		// Calculate ADC correction coefficient
		//raw_adc_val = (g_call_dataset[5] & 0x7FFFFF) >> 5;
		raw_adc_val = cl_averageDataset(g_call_dataset, CALL_DATASET_SIZE);
		var = (raw_adc_val >> 5) / (float)g_call_corr_value;
		sm_SetFloatOpVal(OP_VAL_UADC_CORR, var);
		sm_DumpOpValues();
		break;

	case CALL_TYPE_I_HI:
		// Calculate DAC correction coefficient
		var = cl_calculateDACValue(CALL_REF_I_HI, sm_GetFloatOpVal(OP_VAL_IDAC_CORR), sm_GetShortOpVal(OP_VAL_IDAC_ZERO)) / (float)g_call_corr_value;
		sm_SetFloatOpVal(OP_VAL_IDAC_CORR, var);
		// Calculate ADC correction coefficient
		//raw_adc_val = (g_call_dataset[5] & 0x7FFFFF) >> 5;
		raw_adc_val = cl_averageDataset(g_call_dataset, CALL_DATASET_SIZE);
		var = (raw_adc_val >> 5) / (float)g_call_corr_value;
		sm_SetFloatOpVal(OP_VAL_IADC_CORR, var);
		sm_DumpOpValues();
		break;

	case CALL_TYPE_U_OFF:
		// Calculate DAC offset value
		if (g_call_corr_value > CALL_REF_U_OFF) {
			offset = (g_call_corr_value - CALL_REF_U_OFF) * sm_GetFloatOpVal(OP_VAL_UDAC_CORR) / CALL_REF_OFF_MULT;
			offset |= 0x8000;
		}
		else {
			if (g_call_corr_value < CALL_REF_U_OFF) {
				offset = (CALL_REF_U_OFF - g_call_corr_value) * sm_GetFloatOpVal(OP_VAL_UDAC_CORR) / CALL_REF_OFF_MULT;
			}
			else {
				offset = 0;
			}
		}
		sm_SetShortOpVal(OP_VAL_UDAC_ZERO, offset);

		// Calculate ADC offset value
		/*
		raw_adc_val = cl_averageDataset(g_call_dataset, CALL_DATASET_SIZE);
		if (raw_adc_val < 0) {
			offset = (unsigned short)((~raw_adc_val & 0x7FFFFFFF) + 1);
		}
		else if (raw_adc_val > 0) {
			offset = (unsigned short)(raw_adc_val);
			offset |= 0x8000;
		}
		else {
			offset = 0;
		}
		sm_SetShortOpVal(OP_VAL_IADC_ZERO, offset);*/
		raw_adc_val = cl_averageDataset(g_call_dataset, CALL_DATASET_SIZE);
		temp = (signed int)(sm_GetFloatOpVal(OP_VAL_UADC_CORR) * g_call_corr_value / 3.125);
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
		sm_SetShortOpVal(OP_VAL_UADC_ZERO, offset);
		sm_DumpOpValues();
		break;

	case CALL_TYPE_I_OFF:
		// Calculate DAC offset value
		if (g_call_corr_value > CALL_REF_I_OFF) {
			offset = (g_call_corr_value - CALL_REF_I_OFF) * sm_GetFloatOpVal(OP_VAL_IDAC_CORR) / CALL_REF_OFF_MULT;
			offset |= 0x8000;
		}
		else {
			if (g_call_corr_value < CALL_REF_I_OFF) {
				offset = (CALL_REF_I_OFF - g_call_corr_value) * sm_GetFloatOpVal(OP_VAL_IDAC_CORR) / CALL_REF_OFF_MULT;
			}
			else {
				offset = 0;
			}
		}
		sm_SetShortOpVal(OP_VAL_IDAC_ZERO, offset);

		// Calculate ADC offset value
		/*
		raw_adc_val = cl_averageDataset(g_call_dataset, CALL_DATASET_SIZE);
		if (raw_adc_val < 0) {
			offset = (unsigned short)((~raw_adc_val & 0x7FFFFFFF) + 1);
		}
		else if (raw_adc_val > 0) {
			offset = (unsigned short)(raw_adc_val);
			offset |= 0x8000;
		}
		else {
			offset = 0;
		}
		sm_SetShortOpVal(OP_VAL_UADC_ZERO, offset);*/
		raw_adc_val = cl_averageDataset(g_call_dataset, CALL_DATASET_SIZE);
		temp = (signed int)(sm_GetFloatOpVal(OP_VAL_IADC_CORR) * g_call_corr_value / 3.125);
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
		sm_SetShortOpVal(OP_VAL_IADC_ZERO, offset);
		sm_DumpOpValues();
		break;
	}
	g_call_flags = 0;
	sys_updateUICallState(CALL_STATE_DONE, 0);
}





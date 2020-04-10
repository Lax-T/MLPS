#include "tasks/ui_control.h"
#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "macros.h"
#include "globals.h"
#include "tasks/display_control.h"
#include "misc/graphics.h"
#include "misc/settings_manager.h"

/* Declarations section */
#define VIEW_MAIN 0
#define VIEW_USET 1
#define VIEW_CSET 2
#define VIEW_MENU 3
#define VIEW_INFO_SM 4
#define VIEW_CALL_SM 5
#define VIEW_U_IN 6
#define VIEW_T_REG 7
#define VIEW_CALL 8
#define VIEW_PON_SATE 9
#define VIEW_POPUP 10
#define VIEW_RESET_CONFIRM 11

#define CALL_STEP_WAIT_START 0
#define CALL_STEP_CONFIRM 1
#define CALL_STEP_CORR_INPUT 2
#define CALL_STEP_RUN 3
#define CALL_STEP_DONE 4
#define CALL_STEP_FAIL 5

#define BTN_STATE_NONE 0
#define BTN_STATE_WAIT_LP 1
#define BTN_STATE_WAIT_REPEAT 2
#define BTN_STATE_REPEAT 3
#define BTN_LP_DELAY 20
#define BTN_REPEAT_DELAY 6

#define BLINK_BIT_PERIOD 5

#define UI_FLAG_OUT_STATE 0x01
#define UI_FLAG_LO_UIN 0x10
#define UI_FLAG_OTP 0x20
#define UI_FLAG_KEY_LOCK 0x40
// Alternative function for button lookup
static unsigned char g_const_btn_alternate[] = {
		BTN_STATE_WAIT_LP, BTN_STATE_NONE, BTN_STATE_NONE, BTN_STATE_WAIT_REPEAT,
		BTN_STATE_NONE, BTN_STATE_WAIT_REPEAT, BTN_STATE_NONE, BTN_STATE_NONE};

unsigned char g_seg_buffer[] = {0x55, 0x55, 0x55, 0x55, 0xff, 0xff, 0xff, 0xff};
unsigned char g_led_buffer = 0;
unsigned char g_view_id = 0;
unsigned char g_sub_view_id = 0;
unsigned char g_error_code = 0; // Last error code
unsigned char g_popup_fallback_view = 0; // View code, to return from Info/Error POPUP
unsigned char g_popup_code = 0; // Message code for Info/Error POPUP
unsigned char g_uic_flags = 0;
unsigned short g_settings_1 = 0; // Current system settings (updated from system task)
// Correction related variables
unsigned char g_call_type = 0;
unsigned char g_call_step = 0;

unsigned short g_volt_real_value = 0;
unsigned short g_curr_real_value = 0;
unsigned short g_volt_set_value = 0;
unsigned short g_curr_set_value = 0;
unsigned short g_uin_value = 0;
unsigned short g_treg_value = 0;
unsigned short g_top_row = 0;
unsigned short g_btm_row = 0;

unsigned char g_cursor_pos = 0;
unsigned char g_blink_bit = 0;
unsigned char g_blink_bit_divider = 0;

unsigned char g_btn_on_hold = 0;
unsigned char g_btn_af_counter = 0;
unsigned char g_btn_af_state = 0;

extern TaskHandle_t UIControlTaskHandle;
extern QueueHandle_t xQueueUIEvent;
extern QueueHandle_t xQueueSysEvent;
extern QueueHandle_t xQueueButtonEvent;

void uic_checkButtonQueue();
void uic_checkEventQueue();
void uic_buttonAFPeriodicCheck();
void uic_processButtonPress(unsigned char btn_n);
void uic_renderView();
void blinkDigit(unsigned char pos, unsigned char row);
void uic_handleCallResponse(struct UIEventMessage message);
void showPopUp(unsigned char code);

/* Code section */
/* Notify (wake up) UI handle task */
void uic_notifyUITask(unsigned int event_type) {
	xTaskNotify(UIControlTaskHandle, event_type, eSetBits);
}

/* Main UI handle task */
void uic_taskUIControl(void *arg) {
	unsigned int NotifiedValue;
	unsigned int xResult;

	uic_renderView();
	dc_writeDisplayBuffer(g_seg_buffer, g_led_buffer);

	while (1) {
		xResult = xTaskNotifyWait( pdFALSE, 0xffffffff,	&NotifiedValue, pdMS_TO_TICKS( 800 ));

		if (xResult == pdPASS) {
			// A notification was received. See which bits were set.
			if ((NotifiedValue & UI_EVENT_WAKE_UP) != 0) {
				// Pass, nothing specific to do
			}
			if ((NotifiedValue & UI_EVENT_BTN_STATE) != 0) {
				// Pass, nothing specific to do
			}
			if ((NotifiedValue & UI_EVENT_PERIODIC) != 0) {
				g_blink_bit_divider++;
				if (g_blink_bit_divider >= BLINK_BIT_PERIOD) {
					g_blink_bit = g_blink_bit ^ 1;
					g_blink_bit_divider = 0;
				}
				uic_buttonAFPeriodicCheck();
			}
		}
		uic_checkEventQueue();
		uic_checkButtonQueue();
		uic_renderView();
		dc_writeDisplayBuffer(g_seg_buffer, g_led_buffer);
	}
}

/* Update info on currently pressed button for AF handler */
void uic_updateButtonAF(unsigned char btn_n) {
	if (btn_n != 0) {
		g_btn_af_state = g_const_btn_alternate[btn_n - 1];
	}
	else {
		g_btn_af_state = BTN_STATE_NONE;
	}
	g_btn_on_hold = btn_n;
	g_btn_af_counter = 0;
}

/* Periodically check and trigger alternative function for currently pressed button. */
void uic_buttonAFPeriodicCheck() {
	switch(g_btn_af_state) {
	case BTN_STATE_WAIT_LP:
		g_btn_af_counter++;
		if (g_btn_af_counter >= BTN_LP_DELAY) {
			g_btn_af_state = BTN_STATE_NONE;
			uic_processButtonPress(g_btn_on_hold + 7);
		}
		break;

	case BTN_STATE_WAIT_REPEAT:
		g_btn_af_counter++;
		if (g_btn_af_counter >= BTN_REPEAT_DELAY) {
			g_btn_af_state = BTN_STATE_REPEAT;
			uic_processButtonPress(g_btn_on_hold);
			//Lock blink bit while fast repeat
			g_blink_bit = 1;
		}
		break;

	case BTN_STATE_REPEAT:
		uic_processButtonPress(g_btn_on_hold);
		//Lock blink bit while fast repeat
		g_blink_bit = 1;
		break;
	}
}

/* Process messages from 'button' queue */
void uic_checkButtonQueue() {
	unsigned char btn_value;
	while (xQueueReceive(xQueueButtonEvent, &btn_value, 0)) {
		uic_processButtonPress(btn_value);
		uic_updateButtonAF(btn_value);
	}
}

/* Process messages from event queue */
void uic_checkEventQueue() {
	struct UIEventMessage message;

	while(xQueueReceive(xQueueUIEvent, &(message), 0)) {
		switch (message.type) {

		case UI_MSG_ADC_VALUES:
			g_volt_real_value = message.data[0];
			g_curr_real_value = message.data[1];
			break;

		case UI_MSG_OUT_SET_VALUES:
			g_volt_set_value = message.data[0];
			g_curr_set_value = message.data[1];
			break;

		case UI_MSG_UIN_TREG_VALUES:
			g_uin_value = message.data[0];
			g_treg_value = message.data[1];
			// Check input voltage value
			if (g_uin_value < LO_UIN_TRESHOLD || (g_uin_value - LO_UIN_DELTA) < (g_volt_set_value / 10)) {
				g_uic_flags |= UI_FLAG_LO_UIN;
			}
			if (g_uin_value >= (LO_UIN_TRESHOLD + LO_UIN_HYST) && (g_uin_value - LO_UIN_DELTA - LO_UIN_HYST) >= (g_volt_set_value / 10)) {
				g_uic_flags &= ~UI_FLAG_LO_UIN;
			}
			break;

		case UI_MSG_CALL_STATE:
			uic_handleCallResponse(message);
			break;

		case UI_MSG_OUT_STATE:
			g_uic_flags = (g_uic_flags & ~UI_FLAG_OUT_STATE) | (message.data[0] & 0x01);
			break;

		case UI_MSG_OTP_STATE:
			if (message.data[0] & 0x01) {
				g_uic_flags |= UI_FLAG_OTP;
			} else {
				g_uic_flags &= ~UI_FLAG_OTP;
			}
			break;

		case UI_MSG_SETTINGS:
			g_settings_1 = message.data[0];
			break;

		case UI_MSG_ERROR:
			g_error_code = message.data[0];
			if (message.data[1] == BLOCKING_ERROR) {
				g_uic_flags |= UI_FLAG_KEY_LOCK;
			}
			showPopUp(POPUP_ERROR);
			break;

		case UI_MSG_INFO:
			showPopUp((unsigned short)message.data[0]);
			break;
		}
	}
}

/* Messages to system task */
void uic_sendNewDACValues() {
	struct SystemEventMessage message;
	message.type = SYS_MSG_DAC_VALUES;
	message.data[0] = g_volt_set_value;
	message.data[1] = g_curr_set_value;
	xQueueSend(xQueueSysEvent, (void *) &message, 0);
}

void uic_sendNewOutState() {
	struct SystemEventMessage message;
	message.type = SYS_MSG_OUT_STATE;
	message.data[0] = g_uic_flags & 0x01;
	xQueueSend(xQueueSysEvent, (void *) &message, 0);
}

void uic_sendCallCmd(unsigned short call_cmd, unsigned short arb_value) {
	struct SystemEventMessage message;
	message.type = SYS_MSG_CALL_CMD;
	message.data[0] = call_cmd;
	message.data[1] = arb_value;
	xQueueSend(xQueueSysEvent, (void *) &message, 0);
}

void uic_sendNewSettingsValues() {
	struct SystemEventMessage message;
	message.type = SYS_MSG_SETTINGS;
	message.data[0] = g_settings_1;
	message.data[1] = 0;
	xQueueSend(xQueueSysEvent, (void *) &message, 0);
}

void uic_sendResetCommand() {
	struct SystemEventMessage message;
	message.type = SYS_MSG_RESET;
	xQueueSend(xQueueSysEvent, (void *) &message, 0);
}

/* Handle system task response regarding calibration state. */
void uic_handleCallResponse(struct UIEventMessage message) {
	switch (message.data[0]) {
	case CALL_RESPONSE_READY:
		if (g_call_type == CALL_TYPE_U_IN) { // Skip confirmation for UIN correction (not needed)
			g_btm_row = CALL_REF_U_IN;
			g_call_step = CALL_STEP_CORR_INPUT;
		}
		else {
			g_call_step = CALL_STEP_CONFIRM;
		}
		break;
	case CALL_RESPONSE_STARTED:
		g_call_step = CALL_STEP_RUN;
		break;
	case CALL_RESPONSE_DONE:
		g_call_step = CALL_STEP_DONE;
		break;
	case CALL_RESPONSE_FAIL:
		g_call_step = CALL_STEP_FAIL;
		g_error_code = message.data[1];
		break;
	}
}

/* Stores current UI position and displays defined message. */
void showPopUp(unsigned char code) {
	if (g_view_id != VIEW_POPUP) {
		// Store current values for return
		g_popup_fallback_view = g_view_id;
		g_view_id = VIEW_POPUP;
	}
	// Set message code
	g_popup_code = code;
}

/* Generic UI control methods */
/* Increment specified digit in unsigned short value */
unsigned short increment16ByPosition(unsigned short value, unsigned char pos, unsigned char min_step, unsigned short max_value) {
	switch (pos) {
	case 0:
		value = value + min_step;
		break;
	case 1:
		value = value + 10;
		break;
	case 2:
		value = value + 100;
		break;
	}
	if (value > max_value) {
		value = max_value;
	}
	return value;
}

/* Decrement specified digit in unsigned short value */
unsigned short decrement16ByPosition(unsigned short value, unsigned char pos, unsigned char min_step, unsigned short min_value) {
	unsigned short diff;
	switch (pos) {
	case 0:
		diff = min_step;
		break;
	case 1:
		diff = 10;
		break;
	case 2:
		diff = 100;
		break;
	}
	if ( (value - min_value) < diff ) {
		value = min_value;
	}
	else {
		value = value - diff;
	}
	return value;
}

unsigned char increment8(unsigned char value, unsigned char max_value) {
	if (value < max_value) {
		value++;
	}
	return value;
}

unsigned char decrement8(unsigned char value, unsigned char min_value) {
	if (value > min_value) {
		value--;
	}
	return value;
}

/* View button handlers
 * Buttons: 0 - USET, 1 - ISET, 2 - <, 3 - ^, 4 - >, 5 - v, 7 - ON/OFF, 8 - USET LP */
/* Generic handlers */
void uic_genericHandlerNop() {
}

void uic_genericHandlerGoToMain() {
	g_view_id = VIEW_MAIN;
}

void uic_genericHandlerGoToMenu() {
	g_view_id = VIEW_MENU;
	g_sub_view_id = 0;
}

void uic_genericHandlerToggleOutput() {
	if ((g_uic_flags & UI_FLAG_OTP) == 0) {
		g_uic_flags = g_uic_flags ^ 0x01;
		uic_sendNewOutState();
	}
}

void uic_genericHandlerCursorLeft() {
	g_cursor_pos = increment8(g_cursor_pos, 2);
}

void uic_genericHandlerCursorRight() {
	g_cursor_pos = decrement8(g_cursor_pos, 0);
}

void uic_genericHandlerMenuUp() {
	g_sub_view_id = decrement8(g_sub_view_id, 0);
}


/* Main view handlers */
void uic_mainViewGoToUSet() {
	g_view_id = VIEW_USET;
}

void uic_mainViewGoToISet() {
	g_view_id = VIEW_CSET;
}

static void (*main_view_methods_list[])() = {uic_mainViewGoToUSet, uic_mainViewGoToISet, uic_genericHandlerNop,
	uic_genericHandlerNop, uic_genericHandlerNop, uic_genericHandlerNop, uic_genericHandlerToggleOutput, uic_genericHandlerGoToMenu};

/* Voltage set handlers */
void uic_voltSetViewIncVoltage() {
	g_volt_set_value = increment16ByPosition(g_volt_set_value, g_cursor_pos, 1, 1800);
	uic_sendNewDACValues();
}

void uic_voltSetViewDecVoltage() {
	g_volt_set_value = decrement16ByPosition(g_volt_set_value, g_cursor_pos, 1, 10);
	uic_sendNewDACValues();
}

static void (*voltage_set_view_methods_list[])() = {uic_genericHandlerGoToMain, uic_mainViewGoToISet, uic_genericHandlerCursorLeft,
	uic_voltSetViewIncVoltage, uic_genericHandlerCursorRight, uic_voltSetViewDecVoltage, uic_genericHandlerToggleOutput, uic_genericHandlerGoToMenu};

/* Current set handler */
void uic_currSetViewIncCurrent() {
	g_curr_set_value = increment16ByPosition(g_curr_set_value, g_cursor_pos, 1, 2600);
	uic_sendNewDACValues();
}

void uic_currSetViewDecCurrent() {
	g_curr_set_value = decrement16ByPosition(g_curr_set_value, g_cursor_pos, 1, 1);
	uic_sendNewDACValues();
}

static void (*current_set_view_methods_list[])() = {uic_mainViewGoToUSet, uic_genericHandlerGoToMain, uic_genericHandlerCursorLeft,
	uic_currSetViewIncCurrent, uic_genericHandlerCursorRight, uic_currSetViewDecCurrent, uic_genericHandlerToggleOutput, uic_genericHandlerGoToMenu};

/* Main menu handler */
#define MENU_SUB_VIEW_PON 0
#define MENU_SUB_VIEW_INFO 1
#define MENU_SUB_VIEW_CALL 2
#define MENU_SUB_VIEW_RESET 3

#define MENU_TOTAL_ITEMS 4

unsigned char main_menu_sub_views[] = {VIEW_PON_SATE, VIEW_INFO_SM, VIEW_CALL_SM, VIEW_RESET_CONFIRM};

void uic_mainMenuDown() {
	g_sub_view_id = increment8(g_sub_view_id, MENU_TOTAL_ITEMS - 1);
}

void uic_mainMenuGoToSubMenu() {
	g_view_id = main_menu_sub_views[g_sub_view_id];
	if (g_sub_view_id == MENU_SUB_VIEW_PON) {
		g_sub_view_id = g_settings_1 & SETT_MASK_PON_STATE;
	}
	else {
		g_sub_view_id = 0;
	}
}

static void (*main_menu_view_methods_list[])() = {uic_genericHandlerNop, uic_genericHandlerNop, uic_genericHandlerGoToMain,
	uic_genericHandlerMenuUp, uic_mainMenuGoToSubMenu, uic_mainMenuDown, uic_genericHandlerToggleOutput, uic_genericHandlerNop};

/* Power on state sub menu */
void uic_pOnViewDown() {
	g_sub_view_id = increment8(g_sub_view_id, 2);
}

void uic_pOnViewBack() {
	g_view_id = VIEW_MENU;
	g_sub_view_id = MENU_SUB_VIEW_PON;
}

void uic_pOnViewSave() {
	g_settings_1 = (g_settings_1 & ~SETT_MASK_PON_STATE) | g_sub_view_id;
	uic_sendNewSettingsValues();
	uic_pOnViewBack();
}

static void (*pon_view_methods_list[])() = {uic_genericHandlerNop, uic_genericHandlerNop, uic_pOnViewBack,
	uic_genericHandlerMenuUp, uic_pOnViewSave, uic_pOnViewDown, uic_genericHandlerToggleOutput, uic_genericHandlerNop};

/* Info sub menu handler */
#define INFO_SUB_VIEW_UIN 0
#define INFO_SUB_VIEW_TREG 1
#define INFO_SUB_VIEW_LAST_E 2

#define INFO_SUB_VIEW_TOTAL 2

unsigned char info_menu_sub_views[] = {VIEW_U_IN, VIEW_T_REG, VIEW_POPUP};

void uic_infoSubMenuDown() {
	g_sub_view_id = increment8(g_sub_view_id, INFO_SUB_VIEW_TOTAL);
}

void uic_infoSubMenuGoToItem() {
	if (g_sub_view_id == INFO_SUB_VIEW_LAST_E) {
		g_popup_fallback_view = VIEW_INFO_SM;
		g_popup_code = POPUP_ERROR;
	}
	g_view_id = info_menu_sub_views[g_sub_view_id];
}

void uic_infoSubMenuGoBack() {
	g_view_id = VIEW_MENU;
	g_sub_view_id = MENU_SUB_VIEW_INFO;
}

static void (*info_menu_view_methods_list[])() = {uic_genericHandlerNop, uic_genericHandlerNop, uic_infoSubMenuGoBack,
	uic_genericHandlerMenuUp, uic_infoSubMenuGoToItem, uic_infoSubMenuDown, uic_genericHandlerToggleOutput, uic_genericHandlerNop};

/* Calibration sub menu handler */
#define CALL_SUB_VIEW_UOFF 0
#define CALL_SUB_VIEW_IOFF 1
#define CALL_SUB_VIEW_UHI 2
#define CALL_SUB_VIEW_IHI 3
#define CALL_SUB_VIEW_UIN 4

#define CALL_SUB_VIEW_TOTAL 4

static unsigned char call_sub_views_to_type_map[] = {CALL_TYPE_U_OFF, CALL_TYPE_I_OFF, CALL_TYPE_U_HI, CALL_TYPE_I_HI, CALL_TYPE_U_IN};

void uic_callSubMenuDown() {
	g_sub_view_id = increment8(g_sub_view_id, CALL_SUB_VIEW_TOTAL);
}

void uic_callSubMenuStart() {
	if (g_uic_flags & UI_FLAG_OTP) {
		// If in OTP protection state, show popup and do not allow to enter in calibration state
		showPopUp(POPUP_HI_T);
	} else if (g_uin_value < CALL_MIN_UIN) {
		// If input voltage lover than required minimum, show popup and do not allow to enter in calibration state
		showPopUp(POPUP_LO_UIN);
	} else {
		g_call_type = call_sub_views_to_type_map[g_sub_view_id];
		g_call_step = CALL_STEP_WAIT_START;
		g_view_id = VIEW_CALL;
		uic_sendCallCmd(CALL_CMD_START, g_call_type);
	}
}

void uic_callSubMenuGoBack() {
	g_view_id = VIEW_MENU;
	g_sub_view_id = MENU_SUB_VIEW_CALL;
}

static void (*call_menu_view_methods_list[])() = {uic_genericHandlerNop, uic_genericHandlerNop, uic_callSubMenuGoBack,
	uic_genericHandlerMenuUp, uic_callSubMenuStart, uic_callSubMenuDown, uic_genericHandlerToggleOutput, uic_genericHandlerNop};

/* Input voltage display */
void uic_uInViewGoBack() {
	g_view_id = VIEW_INFO_SM;
	g_sub_view_id = INFO_SUB_VIEW_UIN;
}

static void (*u_in_view_methods_list[])() = {uic_genericHandlerNop, uic_genericHandlerNop, uic_uInViewGoBack,
	uic_genericHandlerNop, uic_genericHandlerNop, uic_genericHandlerNop, uic_genericHandlerToggleOutput, uic_genericHandlerNop};

/* Reg temperature display */
void uic_tRegViewGoBack() {
	g_view_id = VIEW_INFO_SM;
	g_sub_view_id = INFO_SUB_VIEW_TREG;
}

static void (*t_reg_view_methods_list[])() = {uic_genericHandlerNop, uic_genericHandlerNop, uic_tRegViewGoBack,
	uic_genericHandlerNop, uic_genericHandlerNop, uic_genericHandlerNop, uic_genericHandlerToggleOutput, uic_genericHandlerNop};

/* PopUp */
void uic_popUpViewGoBack() {
	g_view_id = g_popup_fallback_view;
}

static void (*popup_view_methods_list[])() = {uic_genericHandlerNop, uic_genericHandlerNop, uic_popUpViewGoBack,
	uic_genericHandlerNop, uic_genericHandlerNop, uic_genericHandlerNop, uic_genericHandlerNop, uic_genericHandlerNop};

/* Reset confirm */
void uic_resetConfirmViewGoBack() {
	g_view_id = VIEW_MENU;
	g_sub_view_id = MENU_SUB_VIEW_RESET;
}

void uic_resetConfirmViewToggle() {
	g_sub_view_id = g_sub_view_id ^ 0x01;
}

void uic_resetConfirmViewConfirm() {
	if (g_sub_view_id) {
		uic_sendResetCommand();
		g_popup_code = POPUP_RUN;
		g_popup_fallback_view = VIEW_MENU;
		g_view_id = VIEW_POPUP;
		g_sub_view_id = MENU_SUB_VIEW_RESET;
	}
	else {
		uic_resetConfirmViewGoBack();
	}
}

static void (*reset_confirm_view_methods_list[])() = {uic_genericHandlerNop, uic_genericHandlerNop, uic_resetConfirmViewGoBack,
		uic_resetConfirmViewToggle, uic_resetConfirmViewConfirm, uic_resetConfirmViewToggle, uic_genericHandlerNop, uic_genericHandlerNop};

/*
#define CALL_STEP_CONFIRM 1
#define CALL_STEP_CORR_INPUT 2
#define CALL_STEP_RUN 3
#define CALL_STEP_DONE 4
#define CALL_STEP_FAIL 5
 * */

/* Calibration view */
void uic_callViewGoBack() {
	g_view_id = VIEW_CALL_SM;
	// g_sub_view_id = 0; Keep last state
	uic_sendCallCmd(CALL_CMD_RESET, 0);
}

void uic_callViewGoNext() {
	switch (g_call_step) {
	case CALL_STEP_CONFIRM:
		uic_sendCallCmd(CALL_CMD_CONFIRM, 0);
		if (g_call_type ==  CALL_TYPE_U_HI || g_call_type ==  CALL_TYPE_I_HI || g_call_type ==  CALL_TYPE_U_OFF || g_call_type ==  CALL_TYPE_I_OFF) {
			g_call_step = CALL_STEP_CORR_INPUT;
			switch (g_call_type) {
			case CALL_TYPE_U_HI:
				g_btm_row = CALL_REF_U_HI;
				break;
			case CALL_TYPE_I_HI:
				g_btm_row = CALL_REF_I_HI;
				break;
			case CALL_TYPE_U_OFF:
				g_btm_row = CALL_REF_U_OFF;
				break;
			case CALL_TYPE_I_OFF:
				g_btm_row = CALL_REF_I_OFF;
				break;
			}
		}
		break;
	case CALL_STEP_CORR_INPUT:
		uic_sendCallCmd(CALL_CMD_CORR_VALUE, g_btm_row);
		break;
	case CALL_STEP_DONE:
		uic_callViewGoBack();
		break;
	case CALL_STEP_FAIL:
		uic_callViewGoBack();
		break;
	}
}

void uic_callViewUp() {
	if (g_call_step == CALL_STEP_CORR_INPUT) {
		g_btm_row = increment16ByPosition(g_btm_row, g_cursor_pos, 1, 2800);
	}
}

void uic_callViewDown() {
	if (g_call_step == CALL_STEP_CORR_INPUT) {
		g_btm_row = decrement16ByPosition(g_btm_row, g_cursor_pos, 1, 10);
	}
}

void uic_callViewLeft() {
	if (g_call_step == CALL_STEP_CORR_INPUT) {
		g_cursor_pos = increment8(g_cursor_pos, 2);
	}
}

void uic_callViewRight() {
	if (g_call_step == CALL_STEP_CORR_INPUT) {
		g_cursor_pos = decrement8(g_cursor_pos, 0);
	}
}

static void (*call_view_methods_list[])() = {uic_genericHandlerNop, uic_callViewGoBack, uic_callViewLeft,
	uic_callViewUp, uic_callViewRight, uic_callViewDown, uic_callViewGoNext, uic_genericHandlerNop};

/* Button press handler */
void uic_processButtonPress(unsigned char btn_n) {
	if (MASK_BIT(g_uic_flags, UI_FLAG_KEY_LOCK)) {
		return;
	}
	if (btn_n == 0) {
		return;
	}
	else {
		btn_n = btn_n - 1;
	}
	switch (g_view_id) {
		// Main view (monitor)
		case VIEW_MAIN:
			(*main_view_methods_list[btn_n])();
			break;

		// Voltage set view
		case VIEW_USET:
			(*voltage_set_view_methods_list[btn_n])();
			break;

		// Current set view
		case VIEW_CSET:
			(*current_set_view_methods_list[btn_n])();
			break;

		// Main menu view
		case VIEW_MENU:
			(*main_menu_view_methods_list[btn_n])();
			break;

		case VIEW_PON_SATE:
			(*pon_view_methods_list[btn_n])();
			break;

		// Info menu view
		case VIEW_INFO_SM:
			(*info_menu_view_methods_list[btn_n])();
			break;

		// Call menu view
		case VIEW_CALL_SM:
			(*call_menu_view_methods_list[btn_n])();
			break;

		// Input voltage view
		case VIEW_U_IN:
			(*u_in_view_methods_list[btn_n])();
			break;

		// Regulator temperature view
		case VIEW_T_REG:
			(*t_reg_view_methods_list[btn_n])();
			break;

		// Regulator temperature view
		case VIEW_CALL:
			(*call_view_methods_list[btn_n])();
			break;

		// PopUp (message) view
		case VIEW_POPUP:
			(*popup_view_methods_list[btn_n])();
			break;

		// Reset confirm view
		case VIEW_RESET_CONFIRM:
			(*reset_confirm_view_methods_list[btn_n])();
			break;
	}
}

/* View render methods */
/* Blink specific digit on display */
void blinkDigit(unsigned char pos, unsigned char row) {
	// row 1 - top, 0 - bottom
	if (g_blink_bit == 0) {
		if (row == 1) {
			pos = 3 - pos;
		}
		else {
			pos = 7 - pos;
		}
		g_seg_buffer[pos] = g_seg_buffer[pos] & 0x80;
	}
}

unsigned char main_menu_text_strings[] = {GR_STR_PON, GR_STR_INFO, GR_STR_CALL, GR_STR_REST};
unsigned char pon_view_text_strings[] = {GR_STR_OFF, GR_STR_LAST, GR_STR_ON};
unsigned char info_menu_text_strings[] = {GR_STR_U_IN, GR_STR_T_REG, GR_STR_LST_E};
unsigned char call_menu_text_strings[] = {GR_STR_U_OFF, GR_STR_I_OFF, GR_STR_U_HI, GR_STR_I_HI, GR_STR_U_IN};
unsigned char call_view_text_strings_top[] = {GR_STR_CALL, GR_STR_BLANK, GR_STR_COR, GR_STR_CALL, GR_STR_DONE, GR_STR_FAIL};
unsigned char call_view_text_strings_btm[] = {GR_STR_3DOT, GR_STR_OUT, GR_STR_BLANK, GR_STR_RUN, GR_STR_BLANK, GR_STR_E};

/* Process current view, write display buffer. */
void uic_renderView() {
	switch (g_view_id) {

	case VIEW_MAIN:
		if ((g_uic_flags & UI_FLAG_OTP) != 0 && g_blink_bit) {
			gr_FormatStr(g_seg_buffer, GR_STR_OTP, 0);
			gr_FormatStr(g_seg_buffer, GR_STR_BLANK, 4);
			break;
		} else if (g_uic_flags & UI_FLAG_LO_UIN) {
			if (g_blink_bit) {
				gr_FormatStr(g_seg_buffer, GR_STR_LO, 0);
				gr_FormatStr(g_seg_buffer, GR_STR_U_IN, 4);
				break;
			}
		}
		g_top_row = g_volt_real_value;
		g_btm_row = g_curr_real_value;
		gr_FormatInt4(g_seg_buffer, g_top_row, 0);
		gr_SetDot(g_seg_buffer, 1);
		gr_FormatInt4(g_seg_buffer, g_btm_row, 4);
		gr_SetDot(g_seg_buffer, 4);
		break;

	case VIEW_USET:
		g_top_row = g_volt_set_value;
		g_btm_row = g_curr_real_value;
		gr_FormatInt4(g_seg_buffer, g_top_row, 0);
		gr_SetDot(g_seg_buffer, 1);
		gr_FormatInt4(g_seg_buffer, g_btm_row, 4);
		gr_SetDot(g_seg_buffer, 4);
		blinkDigit(g_cursor_pos, 1);
		break;

	case VIEW_CSET:
		g_top_row = g_volt_real_value;
		g_btm_row = g_curr_set_value;
		gr_FormatInt4(g_seg_buffer, g_top_row, 0);
		gr_SetDot(g_seg_buffer, 1);
		gr_FormatInt4(g_seg_buffer, g_btm_row, 4);
		gr_SetDot(g_seg_buffer, 4);
		blinkDigit(g_cursor_pos, 0);
		break;

	case VIEW_MENU:
		gr_FormatStr(g_seg_buffer, GR_STR_MENU, 0);
		gr_FormatStr(g_seg_buffer, main_menu_text_strings[g_sub_view_id], 4);
		break;

	case VIEW_PON_SATE:
		gr_FormatStr(g_seg_buffer, GR_STR_PON, 0);
		gr_FormatStr(g_seg_buffer, pon_view_text_strings[g_sub_view_id], 4);
		break;

	case VIEW_INFO_SM:
		gr_FormatStr(g_seg_buffer, GR_STR_INFO, 0);
		gr_FormatStr(g_seg_buffer, info_menu_text_strings[g_sub_view_id], 4);
		break;

	case VIEW_CALL_SM:
		gr_FormatStr(g_seg_buffer, GR_STR_CALL, 0);
		gr_FormatStr(g_seg_buffer, call_menu_text_strings[g_sub_view_id], 4);
		break;

	case VIEW_U_IN:
		gr_FormatStr(g_seg_buffer, GR_STR_U_IN, 0);
		gr_FormatStr(g_seg_buffer, GR_STR_BLANK, 4);
		gr_FormatInt3(g_seg_buffer, g_uin_value, 5);
		gr_SetDot(g_seg_buffer, 6);
		break;

	case VIEW_T_REG:
		gr_FormatStr(g_seg_buffer, GR_STR_T_REG, 0);
		gr_FormatStr(g_seg_buffer, GR_STR_GRAD, 4);
		gr_FormatInt2(g_seg_buffer, g_treg_value, 5);
		break;

	case VIEW_CALL:
		gr_FormatStr(g_seg_buffer, call_view_text_strings_top[g_call_step], 0);
		gr_FormatStr(g_seg_buffer, call_view_text_strings_btm[g_call_step], 4);
		if (g_call_step == CALL_STEP_CORR_INPUT) {
			gr_FormatInt4(g_seg_buffer, g_btm_row, 4);
			switch (g_call_type) {
			case CALL_TYPE_U_HI:
				gr_SetDot(g_seg_buffer, 5);
				break;
			case CALL_TYPE_I_HI:
				gr_SetDot(g_seg_buffer, 4);
				break;
			case CALL_TYPE_U_OFF:
				gr_SetDot(g_seg_buffer, 6);
				break;
			case CALL_TYPE_I_OFF:
				gr_SetDot(g_seg_buffer, 5);
				break;
			case CALL_TYPE_U_IN:
				gr_SetDot(g_seg_buffer, 5);
				break;
			}
			blinkDigit(g_cursor_pos, 0);

		} else if (g_call_step == CALL_STEP_CONFIRM) {
			if (g_call_type == CALL_TYPE_I_HI || g_call_type == CALL_TYPE_I_OFF) {
				gr_FormatStr(g_seg_buffer, GR_STR_SHORT, 0);
			}
			if (g_call_type == CALL_TYPE_U_HI || g_call_type == CALL_TYPE_U_OFF) {
				gr_FormatStr(g_seg_buffer, GR_STR_OPEN, 0);
			}
		}
		break;

	case VIEW_POPUP:
		// Select what to display based on the popup code
		switch (g_popup_code) {
		case POPUP_ERROR:
			if (g_error_code == 0) {
				gr_FormatStr(g_seg_buffer, GR_STR_NO, 0);
				gr_FormatStr(g_seg_buffer, GR_STR_ERR, 4);

			} else {
				gr_FormatStr(g_seg_buffer, GR_STR_ERR, 0);
				gr_FormatStr(g_seg_buffer, GR_STR_E, 4);
				gr_FormatInt3(g_seg_buffer, g_error_code, 5);
			}
			break;
		case POPUP_LO_UIN:
			gr_FormatStr(g_seg_buffer, GR_STR_LO, 0);
			gr_FormatStr(g_seg_buffer, GR_STR_U_IN, 4);
			break;
		case POPUP_HI_T:
			gr_FormatStr(g_seg_buffer, GR_STR_HI, 0);
			gr_FormatStr(g_seg_buffer, GR_STR_T_REG, 4);
			break;
		case POPUP_RUN:
			gr_FormatStr(g_seg_buffer, GR_STR_RUN, 0);
			gr_FormatStr(g_seg_buffer, GR_STR_BLANK, 4);
			break;
		case POPUP_SETT_RESET:
			gr_FormatStr(g_seg_buffer, GR_STR_SETT, 0);
			gr_FormatStr(g_seg_buffer, GR_STR_REST, 4);
			break;
		case POPUP_RESET_FAIL:
			gr_FormatStr(g_seg_buffer, GR_STR_REST, 0);
			gr_FormatStr(g_seg_buffer, GR_STR_FAIL, 4);
			break;
		}
		break;

	case VIEW_RESET_CONFIRM:
		gr_FormatStr(g_seg_buffer, GR_STR_REST, 0);
		if (g_blink_bit) {
			if (g_sub_view_id) {
				gr_FormatStr(g_seg_buffer, GR_STR_YES, 4);
			} else {
				gr_FormatStr(g_seg_buffer, GR_STR_NO, 4);
			}
		}
		else {
			gr_FormatStr(g_seg_buffer, GR_STR_BLANK, 4);
		}
		break;
	}
	g_led_buffer = g_uic_flags & 0x01;
}


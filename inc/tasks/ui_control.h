
#define UI_EVENT_WAKE_UP 0x01
#define UI_EVENT_BTN_STATE 0x02
#define UI_EVENT_PERIODIC 0x04

#define UI_MSG_ADC_VALUES 0x01
#define UI_MSG_OUT_SET_VALUES 0x02
#define UI_MSG_UIN_TREG_VALUES 0x03
#define UI_MSG_CALL_STATE 0x04
#define UI_MSG_OUT_STATE 0x05
#define UI_MSG_SETTINGS 0x06
#define UI_MSG_ERROR 0x07

/* Calibration states in system to UI responses (UI_MSG_CALL_STATE) */
#define CALL_STATE_READY 1
#define CALL_STATE_STARTED 2
#define CALL_STATE_DONE 3
#define CALL_STATE_FAIL 4

void TaskUIControl(void *arg);
void uicNotifyUITask (unsigned int event_type);

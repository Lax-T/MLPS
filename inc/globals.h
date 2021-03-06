
#ifndef UIEventMessage_STRUCT
#define UIEventMessage_STRUCT

struct UIEventMessage
{
    unsigned char type;
    unsigned short data[2];
};

#endif

#ifndef SystemEventMessage_STRUCT
#define SystemEventMessage_STRUCT

struct SystemEventMessage
{
	unsigned char type;
	unsigned short data[2];
};

#endif

#define SYS_MSG_DAC_VALUES 0x01
#define SYS_MSG_OUT_STATE 0x02
#define SYS_MSG_CALL_CMD 0x03
#define SYS_MSG_SETTINGS 0x04
#define SYS_MSG_RESET 0x77

#define CALL_CMD_START 1
#define CALL_CMD_CONFIRM 2
#define CALL_CMD_CORR_VALUE 3
#define CALL_CMD_RESET 4

#define CALL_TYPE_U_OFF 0
#define CALL_TYPE_I_OFF 1
#define CALL_TYPE_U_HI 2
#define CALL_TYPE_I_HI 3
#define CALL_TYPE_U_IN 4

#define CALL_REF_U_HI 1400 // 14 V
#define CALL_REF_I_HI 2200 // 2.2 A
#define CALL_REF_U_OFF 1000 // 1000 / 100 = 10 mV
#define CALL_REF_I_OFF 500 // 500 / 100 = 5 mA
#define CALL_REF_U_IN 2400 // 24 V
#define CALL_REF_OFF_MULT 100

// Error codes
#define ERROR_VOLTAGE_REG 11
#define ERROR_CURRENT_REG 12
#define ERROR_MEM_BLOCK_1 51
#define ERROR_MEM_BLOCK_2 52
#define ERROR_MEM_FAIL 55
#define ERROR_T_SENSOR 71
#define ERROR_UNKNOWN 99

#define BLOCKING_ERROR 1
#define NON_BLOCKING_ERROR 2

// Low input voltage notification
#define LO_UIN_TRESHOLD 115 // 11.5 V
#define LO_UIN_DELTA 20 // 2.0 V
#define LO_UIN_HYST 1 // 0.1 V
#define CALL_MIN_UIN 190 // 19.0 V

#define TRUE 1
#define FALSE 0

#define OUT_ON 1
#define OUT_OFF 0




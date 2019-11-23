
/*
#ifndef SystemCallState_STRUCT
#define SystemCallState_STRUCT

unsigned char g_sys_call_type = 0;
unsigned char g_call_flags = 0;
unsigned short g_call_corr_value = 0;
unsigned int g_call_dataset[CALL_DATASET_SIZE];
unsigned char g_call_dataset_counter = 0;


struct SystemCallState
{
	unsigned char type;
	unsigned short data[2];
};

#endif*/
#define INPUT_VOLTAGE_CORR_COEFF 8.55
#define ZERO_TRESHOLD 1000
#define VREG_ERR_TRESHOLD 100
#define VREG_ERR_MIN_CURRENT 20
#define CREG_ERR_TRESHOLD 200

#define FAN_OFF GPIO_ResetBits(GPIOB, GPIO_Pin_6);
#define FAN_ON GPIO_SetBits(GPIOB, GPIO_Pin_6);
#define FAN_ON_TEMP 50
#define FAN_OFF_TEMP 40

void TaskSystemControl(void *arg);

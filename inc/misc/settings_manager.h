
//Float system critical values
#define OP_VAL_UADC_CORR 0
#define OP_VAL_IADC_CORR 1
#define OP_VAL_UDAC_CORR 2
#define OP_VAL_IDAC_CORR 3
#define OP_VAL_UIN_CORR 4

//Short system critical values
#define OP_VAL_UADC_ZERO 10
#define OP_VAL_IADC_ZERO 11
#define OP_VAL_UDAC_ZERO 12
#define OP_VAL_IDAC_ZERO 13
#define OP_VAL_SETTINGS_1 14
// Settings_1 mask
#define SETT_MASK_PON_STATE 0x0003
#define PON_STATE_OFF 0
#define PON_STATE_LAST 1
#define PON_STATE_ON 2

//Short non system critical values
#define OP_VAL_SET_VOLT 15
#define OP_VAL_SET_CURR 16
#define OP_VAL_OUT_STATE 17

// Memory error flags
#define BLOCK_1_ERR_BIT 0x01
#define BLOCK_2_ERR_BIT 0x02

float sm_GetFloatOpVal(unsigned char value_id);
unsigned short sm_GetShortOpVal(unsigned char value_id);
void sm_SetFloatOpVal(unsigned char value_id, float op_value);
void sm_SetShortOpVal(unsigned char value_id, unsigned short op_value);
unsigned char sm_LoadOpValues();
void sm_ForceDumpAllOpValues();
void sm_DumpOpValues();
void sm_PeriodicWrite();

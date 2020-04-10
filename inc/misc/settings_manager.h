#define MEM_MARKER_H_ADR 0x00FF
#define MEM_MARKER_L_ADR 0x00FE
#define MEM_MARKER_H 0XAA
#define MEM_MARKER_L 0x34

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

// Memory flags
#define MEM_BLOCK_1_CRC 0x01
#define MEM_BLOCK_2_CRC 0x02
#define MEM_ERROR 0x10
#define MEM_FORMATTED 0x80

float sm_getFloatOpVal(unsigned char value_id);
unsigned short sm_getShortOpVal(unsigned char value_id);
void sm_setFloatOpVal(unsigned char value_id, float op_value);
void sm_setShortOpVal(unsigned char value_id, unsigned short op_value);
unsigned char sm_loadOpValues();
void sm_forceDumpAllOpValues();
void sm_dumpOpValues();
void sm_periodicWrite();
unsigned char sm_formatMem();

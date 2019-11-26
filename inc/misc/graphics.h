
#define GR_STR_MENU 0
#define GR_STR_CALL 1
#define GR_STR_INFO 2
#define GR_STR_U_IN 3
#define GR_STR_T_REG 4
#define GR_STR_U_OFF 5
#define GR_STR_I_OFF 6
#define GR_STR_U_HI 7
#define GR_STR_I_HI 8
#define GR_STR_GRAD 9
#define GR_STR_BLANK 10
#define GR_STR_OPEN 11
#define GR_STR_SHORT 12
#define GR_STR_OUT 13
#define GR_STR_DONE 14
#define GR_STR_FAIL 15
#define GR_STR_E 16
#define GR_STR_F 17
#define GR_STR_RUN 18
#define GR_STR_3DOT 19
#define GR_STR_U_COR 20
#define GR_STR_I_COR 21
#define GR_STR_COR 22
#define GR_STR_PON 23
#define GR_STR_OFF 24
#define GR_STR_LAST 25
#define GR_STR_ON 26
#define GR_STR_ERR 27
#define GR_STR_OTP 28
#define GR_STR_LO 29
#define GR_STR_HI 30
#define GR_STR_LST_E 31
#define GR_STR_NO 32

unsigned char grTranslateToSegment(unsigned char value);

void grFormatInt4(unsigned char buffer[], unsigned short value, unsigned char offset);
void grFormatInt3(unsigned char buffer[], unsigned short value, unsigned char offset);
void grFormatInt2(unsigned char buffer[], unsigned short value, unsigned char offset);
void grFormatStr(unsigned char buffer[], unsigned char string_id, unsigned char offset);
void grSetDot(unsigned char buffer[], unsigned char position);

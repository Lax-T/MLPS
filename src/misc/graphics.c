#include "misc/graphics.h"


#define _0_ 0
#define _1_ 1
#define _2_ 2
#define _3_ 3
#define _4_ 4
#define _5_ 5
#define _6_ 6
#define _7_ 7
#define _8_ 8
#define _9_ 9
#define _A_ 10
#define _B_ 11
#define _C_ 12
#define _D_ 13
#define _E_ 14
#define _F_ 15
#define _G_ 16
#define _H_ 17
#define _J_ 18
#define _L_ 19
#define _N_ 20
#define _P_ 21
#define _R_ 22
#define _T_ 23
#define _U_ 24
#define _sp_ 25
#define _dot_ 30
#define _3l_ 31
#define _2l_ 32
#define ___ 33
#define _1l_ 34
#define _grad_ 35

static unsigned char g_str_menu[] = {_3l_, _2l_, ___, _sp_ };
static unsigned char g_str_call[] = {_C_, _A_, _L_, _L_ };
static unsigned char g_str_info[] = {_1_, _N_, _F_, _0_ };
static unsigned char g_str_u_in[] = {_U_, _dot_, _sp_, _1_, _N_ };
static unsigned char g_str_t_reg[] = {_T_, _dot_, _R_, _E_, _G_ };
static unsigned char g_str_u_off[] = {_U_, _dot_, _0_, _F_, _F_ };
static unsigned char g_str_i_off[] = {_1_, _dot_, _0_, _F_, _F_ };
static unsigned char g_str_u_hi[] = {_U_, _dot_, _sp_, _H_, _1_ };
static unsigned char g_str_i_hi[] = {_1_, _dot_, _sp_, _H_, _1_ };
static unsigned char g_str_grad[] = {_sp_, _sp_, _sp_, _grad_ };
static unsigned char g_str_blank[] = {_sp_, _sp_, _sp_, _sp_ };
static unsigned char g_str_open[] = {_0_, _P_, _E_, _N_};
static unsigned char g_str_short[] = {_5_, _H_, _R_, _T_};
static unsigned char g_str_out[] = {_0_, _U_, _T_, _sp_};
static unsigned char g_str_done[] = {_D_, _0_, _N_, _E_};
static unsigned char g_str_fail[] = {_F_, _A_, _1_, _L_};
static unsigned char g_str_e[] = {_E_, _sp_, _sp_, _sp_};
static unsigned char g_str_f[] = {_F_, _sp_, _sp_, _sp_};
static unsigned char g_str_run[] = {_R_, _U_, _N_, _sp_};
static unsigned char g_str_3dot[] = {_dot_, _dot_, _dot_, _sp_};
static unsigned char g_str_u_cor[] = {_U_, _dot_, _C_, _0_, _R_ };
static unsigned char g_str_i_cor[] = {_1_, _dot_, _C_, _0_, _R_ };
static unsigned char g_str_cor[] = {_C_, _0_, _R_, _sp_ };
static unsigned char g_str_pon[] = {_P_, _dot_, _sp_, _0_, _N_};
static unsigned char g_str_off[] = {_sp_, _0_, _F_, _F_};
static unsigned char g_str_last[] = {_L_, _A_, _5_, _T_};
static unsigned char g_str_on[] = {_sp_, _sp_, _0_, _N_};
static unsigned char g_str_err[] = {_sp_, _E_, _R_, _R_};

static unsigned char *g_str_list[] = {
	g_str_menu, g_str_call, g_str_info, g_str_u_in, g_str_t_reg,
	g_str_u_off, g_str_i_off, g_str_u_hi, g_str_i_hi, g_str_grad,
	g_str_blank, g_str_open, g_str_short, g_str_out, g_str_done,
	g_str_fail, g_str_e, g_str_f, g_str_run, g_str_3dot,
	g_str_u_cor, g_str_i_cor, g_str_cor, g_str_pon, g_str_off,
	g_str_last, g_str_on, g_str_err};

static unsigned char g_font[] = {
	0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, // 0 - 9
	0x77, 0x7C, 0x39, 0x7C, 0x79, 0x71, 0x3D, 0x76, 0x0E, 0x38, // A, B, C, D, E, F, G, H, J, L
	0x54, 0x73, 0x50, 0x78, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, // N, P, R, T, U,  ,  ,  ,  ,
	0x80, 0x49, 0x48, 0x08, 0x40, 0x63}; // ., Z, =, _, -, grad

/*Translate value to segment number/character*/
unsigned char grTranslateToSegment(unsigned char value) {
	return g_font[value];
}

/* Format 16bit integer to 4 digit value */
void grFormatInt4(unsigned char buffer[], unsigned short value, unsigned char offset) {
	buffer[offset] = grTranslateToSegment(value / 1000);
	offset++;
	value = value % 1000;
	buffer[offset] = grTranslateToSegment(value / 100);
	offset++;
	value = value % 100;
	buffer[offset] = grTranslateToSegment(value / 10);
	offset++;
	buffer[offset] = grTranslateToSegment(value % 10);
}

void grFormatInt3(unsigned char buffer[], unsigned short value, unsigned char offset) {
	value = value % 1000;
	buffer[offset] = grTranslateToSegment(value / 100);
	offset++;
	value = value % 100;
	buffer[offset] = grTranslateToSegment(value / 10);
	offset++;
	buffer[offset] = grTranslateToSegment(value % 10);
}

void grFormatInt2(unsigned char buffer[], unsigned short value, unsigned char offset) {
	value = value % 100;
	buffer[offset] = grTranslateToSegment(value / 10);
	offset++;
	buffer[offset] = grTranslateToSegment(value % 10);
}

void grFormatStr(unsigned char buffer[], unsigned char string_id, unsigned char offset) {
	unsigned char i, symbol, str_pointer;
	unsigned char *str = g_str_list[string_id];
	str_pointer = 0;

	for (i=0; i < 4; i++) {
		symbol = str[str_pointer];
		if (symbol == _dot_) {
			offset--;
			i--;
			buffer[offset] = (buffer[offset] | g_font[symbol]);
		}
		else {
			buffer[offset] = g_font[symbol];
		}
		offset++;
		str_pointer++;
	}
}

// Set dot for given position
void grSetDot(unsigned char buffer[], unsigned char position) {
	if (position < 8) {
		buffer[position] = buffer[position] | 0x80;
	}
}


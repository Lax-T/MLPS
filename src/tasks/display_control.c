#include "stm32f10x.h"
#include "tasks/ui_control.h"
#include "tasks/display_control.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "macros.h"

#define BTN_MASK 0x10
#define BTN_RELEASE_THRESHOLD 108
#define BTN_PRESS_THRESHOLD 8
#define KEY_1_MASK 0x01
#define KEY_2_MASK 0x80
#define KEY_3_MASK 0x02
#define KEY_4_MASK 0x04
#define KEY_5_MASK 0x08
#define KEY_6_MASK 0x40
#define KEY_7_MASK 0x20

#define CHECK_CV_PIN GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_9)
#define CHECK_CC_PIN GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8)

unsigned char g_digit_buffer[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
unsigned char g_led_status = 0xFF;
const unsigned char segment_lookup[] = {0xBF, 0x7F, 0xFB, 0xF7, 0xFE, 0xEF, 0xFD, 0xDF}; //Segment to reg bit mapping - A, B, C, D, E, F, G, DP
const unsigned char ind_lookup[] = {1, 0, 3, 2, 4, 5, 6, 7}; //Ind n to driver out n mapping - DIG1 - DIG4 upper, DIG1 - DIG4 lower
const unsigned char led_mask_lookup[] = {0x00, 0x00, 0x00, 0x01, 0x04, 0x00, 0x02, 0x00}; // Segment group - LED mask lookup
const unsigned char btn_mask_lookup[] = {KEY_1_MASK, KEY_2_MASK, KEY_3_MASK, KEY_4_MASK, KEY_5_MASK, KEY_6_MASK, KEY_7_MASK};

extern QueueHandle_t xQueueButtonEvent;

unsigned char sendByte(unsigned char data_byte);
unsigned char refreshDisplay(unsigned char seg_n);
void notifyKeyRelease();
void notifyKeyPress(unsigned char key);
unsigned char checkKeyReleased(unsigned char key_state, unsigned char key_n);
unsigned char decodeKeyN(unsigned char key_state);


/* Display communication task */
void dc_taskDisplayControl(void *arg) {
	unsigned char seg_n = 0;
	unsigned char btn_counter = 0;
	unsigned char btn_status_temp = 0xFF;
	unsigned char btn_current_state;

	while (1) {
		seg_n++;
		if (seg_n >= 8) {
			seg_n = 0;
		}
		// To speedup refresh, CC/CV LEDs are not controlled from UI task, but directly from display control task.
		// When displaying segment 4, check if CV LED should be ON (in CV regulation mode)
		if (seg_n == 4) {
			if (CHECK_CV_PIN) { //CV regulation check
				g_led_status = g_led_status | 0x04;
			}
			else {
				g_led_status = g_led_status & 0xFB;
			}
		}
		// When displaying segment 6, check if CC LED should be ON (in CC regulation mode)
		if (seg_n == 6) {
			if (CHECK_CC_PIN) { //CC regulation check
				g_led_status = g_led_status | 0x02;
			}
			else {
				g_led_status = g_led_status & 0xFB;
			}

		}
		// Refresh display and get buttons state in result.
		btn_current_state = refreshDisplay(seg_n) | BTN_MASK;
		// Key press process logic
		// If more then 100, button press was registered. Now checking to confirm button release.
		if (btn_counter >= 100) {
			if (checkKeyReleased(btn_current_state, btn_status_temp)) {
				// If button is released, increase and check counter threshold.
				btn_counter++;
				if (btn_counter > BTN_RELEASE_THRESHOLD) {
					notifyKeyRelease();
					btn_status_temp = 0xFF;
					btn_counter = 0;
				}
			}
			// If not released, reset counter and wait next cycle (button debounce logic)
			else {
				btn_counter = 100;
			}
		}
		else {
			// If no button is pressed or not good contact (debounce logic), reset counter to zero.
			if (btn_current_state == 0xFF) {
				btn_counter = 0;
				btn_status_temp = 0xFF;
			}
			// If counter exceeds threshold, register button press
			else {
				if (btn_current_state == btn_status_temp) {
					btn_counter++;
					if (btn_counter > BTN_PRESS_THRESHOLD) {
						btn_status_temp = decodeKeyN(btn_current_state);
						notifyKeyPress(btn_status_temp);
						btn_counter = 100;
					}
				}
				else {
					btn_status_temp = btn_current_state;
					btn_counter = 0;
				}
			}
		}
		// Sleep till next tick
		vTaskDelay(1);
	}
}


/* Keyboard handle functions */
/* Send key release event to the UI task. */
void notifyKeyRelease() {
	unsigned char zero = 0;
	xQueueSend(xQueueButtonEvent, &zero, 0);
	uic_notifyUITask(UI_EVENT_BTN_STATE);
}

/* Send key pressed event to UI task. */
void notifyKeyPress(unsigned char key) {
	xQueueSend(xQueueButtonEvent, &key, 0);
	uic_notifyUITask(UI_EVENT_BTN_STATE);
}

/* Check if specific button is released. */
unsigned char checkKeyReleased(unsigned char key_state, unsigned char key_n) {
	if ( (key_state & btn_mask_lookup[key_n-1]) != 0 ) {
		return 1;
	}
	else {
		return 0;
	}
}

/* Decode key number according to masks. */
unsigned char decodeKeyN(unsigned char key_state) {
	unsigned char i;
	for (i=0; i<7; i++) {
		if ( (key_state & btn_mask_lookup[i]) == 0) {
			return i + 1;
		}
	}
	return 0;
}

/* Update display buffer/led status registers */
void dc_writeDisplayBuffer(unsigned char buffer[], unsigned char leds) {
	unsigned char i;

	for (i=0; i < 8; i++) {
		g_digit_buffer[i] = buffer[i];
	}
	g_led_status = leds;
}

/***** Display refresh functions *****/
/* Send byte to driver/shift register, read byte from register with buttons status. */
unsigned char sendByte(unsigned char tx_byte) {
	unsigned char rx_byte = 0;
	unsigned char i;

	for (i=0;i < 8; i++) {
		rx_byte = rx_byte << 1;

		if (CHECK_BIT(tx_byte, 7)) {
			DISP_DOUT_HI;
		}
		else {
			DISP_DOUT_LOW;
		}
		if ( GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12) ) {
			rx_byte = rx_byte | 0x01;
		}
		DISP_CLK_HI;
		DISP_CLK_LOW;
		tx_byte = tx_byte << 1;
	}
	return rx_byte;
}

/* Send additional bit for one bit latch, for status LEDs */
void sendLedBit(unsigned char led_state) {
	if (led_state != 0) {
		DISP_DOUT_HI;
	}
	else {
		DISP_DOUT_LOW;
	}
	DISP_CLK_HI;
	DISP_CLK_LOW;
}

/* Execute complete display refresh cycle with keyboard status read. */
unsigned char refreshDisplay(unsigned char seg_n) {
	unsigned char buttons = 0;
	unsigned char dr_data = 0;
	unsigned char reg_data, i;
	// Prepare driver and register data
	for (i=0; i < 8; i++) { //Iterate digits in buffer
		if(CHECK_BIT((g_digit_buffer[i]), seg_n)) {
			dr_data = dr_data | (0x01 << ind_lookup[i]);
		}
	}
	reg_data = segment_lookup[seg_n];
	// Turn off display
	DISP_OE_HI;
	// Lath button scan register
	DISP_REG_LD_HI;
	DISP_REG_LD_LOW;
	buttons = sendByte(reg_data);
	sendLedBit(g_led_status & led_mask_lookup[seg_n]);
	sendByte(dr_data);
	// Latch data in register and driver
	DISP_REG_LD_HI;
	DISP_DR_LE_HI;
	DISP_REG_LD_LOW;
	DISP_DR_LE_LOW;
	// Turn on display
	DISP_OE_LOW;
	return buttons;
}

#include "delays.h"
#include "stm32f10x.h"

/* Microsecond(s) delay */
void uSDelay(unsigned int delay ){
	/* How to calculate delay = uS * CoreMHz - 28 */
	TIM6->CNT = 0;
	while (TIM6->CNT < delay){
	}
}

/* Millisecond(s) delay */
void mSDelay(unsigned int delay){
	unsigned int i;
	for (i = 0; i < delay; i++ ) {
		uSDelay(CORE_FREQUENCY_MHz*1000-26);
	}
}

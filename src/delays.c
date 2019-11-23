#include "delays.h"

void uSDelay(unsigned int delay ){
	/* How to calculate delay = uS * CoreMHz - 28 */
	TIM6->CNT = 0;
	while (TIM6->CNT < delay){
	}
}

void mSDelay(unsigned int delay){
	unsigned int i;
	for (i = 0; i < delay; i++ ) {
		uSDelay(CoreFreqMHz*1000-26);
	}
}

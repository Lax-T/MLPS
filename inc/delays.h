#define CORE_FREQUENCY_MHz 16

#define USDELAY_CALC(delay) (delay*CORE_FREQUENCY_MHz-28)

void uSDelay(unsigned int delay);

void mSDelay(unsigned int delay);


#include "misc/memory_manager.h"
#include "driver/i2c.h"
#include "delays.h"

#define EEPROM_ADDRESS 0xA0

/* Write random EEPROM byte */
void mem_writeRandomByte(unsigned short address, unsigned char data) {
	unsigned char adr_h, adr_l;
	adr_l = address & 0xFF;
	adr_h = (address >> 7) & 0x0E;

	i2c_Start();
	i2c_TXByte(EEPROM_ADDRESS | adr_h);
	i2c_SAck();
	i2c_TXByte(adr_l);
	i2c_SAck();
	i2c_TXByte(data);
	i2c_SAck();
	i2c_Stop();
}

/* Write EEPROM block, up to 256 bytes within same block */
void mem_writeBlock(unsigned short address, unsigned char data[], unsigned char block_size) {
	unsigned char adr_h, adr_l, i;
	adr_l = address & 0xFF;
	adr_h = (address >> 7) & 0x0E;

	i2c_Start();
	i2c_TXByte(EEPROM_ADDRESS | adr_h);
	i2c_SAck();
	i2c_TXByte(adr_l);
	i2c_SAck();

	for (i=0; i < block_size; i++) {
		i2c_TXByte(data[i]);
		i2c_SAck();
	}

	i2c_Stop();
}

/* Read random EEPROM byte */
unsigned char mem_readRandomByte(unsigned short address) {
	unsigned char adr_h, adr_l, data;
	adr_l = address & 0xFF;
	adr_h = (address >> 7) & 0x0E;

	i2c_Start();
	i2c_TXByte(EEPROM_ADDRESS | adr_h);
	i2c_SAck();
	i2c_TXByte(adr_l);
	i2c_SAck();

	i2c_Start();
	i2c_TXByte(EEPROM_ADDRESS | 0x01 | adr_h);
	i2c_SAck();
	data = i2c_RXByte();
	i2c_MNAck();
	i2c_Stop();
	return data;
}

/* Read EEPROM block, up to 256 bytes within same block */
void mem_readBlock(unsigned short address, unsigned char data[], unsigned char block_size) {
	unsigned char adr_h, adr_l, i;
	adr_l = address & 0xFF;
	adr_h = (address >> 7) & 0x0E;

	i2c_Start();
	i2c_TXByte(EEPROM_ADDRESS | adr_h);
	i2c_SAck();
	i2c_TXByte(adr_l);
	i2c_SAck();

	i2c_Start();
	i2c_TXByte(EEPROM_ADDRESS | 0x01 | adr_h);
	i2c_SAck();
	data[0] = i2c_RXByte();

	for (i=1; i < block_size; i++) {
		i2c_MAck();
		data[i] = i2c_RXByte();
	}

	i2c_MNAck();
	i2c_Stop();
}


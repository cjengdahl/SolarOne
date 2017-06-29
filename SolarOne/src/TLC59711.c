/* tlc59711.c

This source file is used to configure a tlc59711
led driver with the assumption that is controlled
by an Atmega328p AVR microcontroller.

The source is heavily based of the Adafruit_TLC59711
libary, written by Limor Fried.  The original code
has been stripped of all Arduino function calls and references.
A bit-bang method of SPI transmission is utilized. 

 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "TLC59711.h"


void set_brightness(uint8_t channel, uint16_t brightness, uint16_t *pwmBuffer){
	pwmBuffer[channel] = brightness;
}

void spi_init(void){
	// SPI is bit-banged, set pins 4 and 5 as outputs on portB
	DDRB = (1 << DDB4) | (1 << DDB5);
}

void spi_write(uint16_t *pwmBuffer, uint8_t numDrivers){
	uint32_t command;
	command = 0x25;		// TLC59711 Magic Number

	command <<= 5;		// Function control bits.
	//command |= 0x16;	// OUTTMG = 1, EXTGCK = 0, TMGRST = 1, DSPRPT = 1, BLANK = 0 -> 0x16
	command |= 0x06;	// OUTTMG = 0, EXTGCK = 0, TMGRST = 1, DSPRPT = 1, BLANK = 0 -> 0x06 Attempt to reduce EMI

	command <<= 7;		// Set RGB brightness levels 
	command |= BC_R;
	command <<= 7;
	command |= BC_G;
	command <<= 7;
	command |= BC_B;

	cli();        		// Disable interrupts

	// Iterate over each driver
	for (uint8_t n = 0; n < numDrivers; n++){
		transfer(command >> 24);
		transfer(command >> 16);
		transfer(command >> 8);
		transfer(command);

		// Iterate over each channel of each driver, MSB first
		for(int8_t c=11; c >= 0 ; c--){
			transfer(pwmBuffer[c + (12 * n)]>>8);     
			transfer(pwmBuffer[c + (12 * n)]);        
		}
	}

	_delay_us(200);  //Allow for 218 LSBs to latch

	sei();	//Enable interrupts
}

// transfer is used by writeData to actually send data out of the micro-controller
void transfer(uint32_t data){

	// Bit Bang
    uint32_t transferMask = 0x80;
    for (; transferMask!=0; transferMask>>=1) {
      CLOCK_LOW();
      if (data & transferMask)  
		DATA_HIGH();
      else
		DATA_LOW();
    CLOCK_HIGH();
  }

}




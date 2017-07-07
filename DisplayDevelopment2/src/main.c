/*
 * fireflies.c
 *
 * Created: 01/25/17
 * Author : Cory J. Engdahl
 *

 */

// Constants
#ifndef F_CPU
#define F_CPU 16000000
#endif
#define NUM_DRIVERS 2								// Number of LED driver chips that are being used. Chip being used is ________
#define TOTAL_CHANNELS (12 * NUM_DRIVERS) 			// Total number of LEDs that are going to be active
#define ACTIVE_LEDS 12								// The number of flys that will be active at any given time
#define UPDATE_DELAY 1
#define UART_BAUD_RATE 9600							// Delay between updates in milliseconds
#define DEBUG_MODE 0


// Libs
 // Libs
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <util/delay_basic.h>
#include <stdbool.h>
#include "TLC59711.h"
#include "uart.h"
#include <avr/interrupt.h>

// led stages
enum stage{

	ready,
	startDelay,
	fadeIn,
	hold,
	fadeOut,
	terminated

};

// Array of LEDs
typedef struct{
	
	uint8_t startDelayTime;		
	uint8_t holdTime;
	uint16_t *fadeInTable;
	uint16_t *fadeOutTable;
	uint8_t fadeLevel;
	uint8_t fadeInTableSize;
	uint8_t fadeOutTableSize;
	uint16_t *brightness;
	enum stage stage;

}led;


uint16_t *LEDBuffer;
led *LEDs;
led *activeLEDs[ACTIVE_LEDS];
led *inactiveLEDs[TOTAL_CHANNELS-ACTIVE_LEDS];  

// brightness tables

const uint16_t fade60[61] = {0x0000, 0x0442, 0x0884, 0x0CC6, 0x1108, 0x154A,
							 0x198C, 0x1DCE, 0x2210, 0x2652, 0x2A94, 0x2ED6,
							 0x3318, 0x375A, 0x3B9C, 0x3FDE, 0x4420, 0x4862,
							 0x4CA4, 0x50E6, 0x5528, 0x596A, 0x5DAC, 0x61EE,
							 0x6630, 0x6A72, 0x6EB4, 0x72F6, 0x7738, 0x7B7A,
							 0x7FBC, 0x83FE, 0x8840, 0x8C82, 0x90C4, 0x9506,
							 0x9948, 0x9D8A, 0xA1CC, 0xA60E, 0xAA50, 0xAE92,
							 0xB2D4, 0xB716, 0xBB58, 0xBF9A, 0xC3DC, 0xC81E,
							 0xCC60, 0xD0A2, 0xD4E4, 0xD926, 0xDD68, 0xE1AA,
							 0xE5EC, 0xEA2E, 0xEE70, 0xF2B2, 0xF6F4, 0xFB36,
							 0xFF78};

const uint16_t fade90[101] = {0x0000, 0x028E, 0x051C, 0x07AA, 0x0A38, 0x0CC6,
							  0x0F54, 0x11E2, 0x1470, 0x16FE, 0x198C, 0x1C1A,
							  0x1EA8, 0x2136, 0x23C4, 0x2652, 0x28E0, 0x2B6E,
							  0x2DFC, 0x308A, 0x3318, 0x35A6, 0x3834, 0x3AC2,
							  0x3D50, 0x3FDE, 0x426C, 0x44FA, 0x4788, 0x4A16,
							  0x4CA4, 0x4F32, 0x51C0, 0x544E, 0x56DC, 0x596A,
							  0x5BF8, 0x5E86, 0x6114, 0x63A2, 0x6630, 0x68BE,
							  0x6B4C, 0x6DDA, 0x7068, 0x72F6, 0x7584, 0x7812,
							  0x7AA0, 0x7D2E, 0x7FBC, 0x824A, 0x84D8, 0x8766,
							  0x89F4, 0x8C82, 0x8F10, 0x919E, 0x942C, 0x96BA,
							  0x9948, 0x9BD6, 0x9E64, 0xA0F2, 0xA380, 0xA60E,
							  0xA89C, 0xAB2A, 0xADB8, 0xB046, 0xB2D4, 0xB562,
							  0xB7F0, 0xBA7E, 0xBD0C, 0xBF9A, 0xC228, 0xC4B6,
							  0xC744, 0xC9D2, 0xCC60, 0xCEEE, 0xD17C, 0xD40A,
							  0xD698, 0xD926, 0xDBB4, 0xDE42, 0xE0D0, 0xE35E,
							  0xE5EC, 0xE87A, 0xEB08, 0xED96, 0xF024, 0xF2B2,
							  0xF540, 0xF7CE, 0xFA5C, 0xFCEA, 0xFF78};

const uint16_t fade120[121] = {0x0000, 0x0221, 0x0442, 0x0663, 0x0884, 0x0AA5,
							   0x0CC6, 0x0EE7, 0x1108, 0x1329, 0x154A, 0x176B,
							   0x198C, 0x1BAD, 0x1DCE, 0x1FEF, 0x2210, 0x2431,
							   0x2652, 0x2873, 0x2A94, 0x2CB5, 0x2ED6, 0x30F7,
							   0x3318, 0x3539, 0x375A, 0x397B, 0x3B9C, 0x3DBD,
							   0x3FDE, 0x41FF, 0x4420, 0x4641, 0x4862, 0x4A83,
							   0x4CA4, 0x4EC5, 0x50E6, 0x5307, 0x5528, 0x5749,
							   0x596A, 0x5B8B, 0x5DAC, 0x5FCD, 0x61EE, 0x640F,
							   0x6630, 0x6851, 0x6A72, 0x6C93, 0x6EB4, 0x70D5,
							   0x72F6, 0x7517, 0x7738, 0x7959, 0x7B7A, 0x7D9B,
							   0x7FBC, 0x81DD, 0x83FE, 0x861F, 0x8840, 0x8A61,
							   0x8C82, 0x8EA3, 0x90C4, 0x92E5, 0x9506, 0x9727,
							   0x9948, 0x9B69, 0x9D8A, 0x9FAB, 0xA1CC, 0xA3ED,
							   0xA60E, 0xA82F, 0xAA50, 0xAC71, 0xAE92, 0xB0B3,
							   0xB2D4, 0xB4F5, 0xB716, 0xB937, 0xBB58, 0xBD79,
							   0xBF9A, 0xC1BB, 0xC3DC, 0xC5FD, 0xC81E, 0xCA3F,
							   0xCC60, 0xCE81, 0xD0A2, 0xD2C3, 0xD4E4, 0xD705,
							   0xD926, 0xDB47, 0xDD68, 0xDF89, 0xE1AA, 0xE3CB,
							   0xE5EC, 0xE80D, 0xEA2E, 0xEC4F, 0xEE70, 0xF091,
							   0xF2B2, 0xF4D3, 0xF6F4, 0xF915, 0xFB36, 0xFD57,
							   0xFF78};

const uint16_t fade150[151] = {0x0000, 0x01B4, 0x0368, 0x051C, 0x06D0, 0x0884,
							   0x0A38, 0x0BEC, 0x0DA0, 0x0F54, 0x1108, 0x12BC,
							   0x1470, 0x1624, 0x17D8, 0x198C, 0x1B40, 0x1CF4,
							   0x1EA8, 0x205C, 0x2210, 0x23C4, 0x2578, 0x272C,
							   0x28E0, 0x2A94, 0x2C48, 0x2DFC, 0x2FB0, 0x3164,
							   0x3318, 0x34CC, 0x3680, 0x3834, 0x39E8, 0x3B9C,
							   0x3D50, 0x3F04, 0x40B8, 0x426C, 0x4420, 0x45D4,
							   0x4788, 0x493C, 0x4AF0, 0x4CA4, 0x4E58, 0x500C,
							   0x51C0, 0x5374, 0x5528, 0x56DC, 0x5890, 0x5A44,
							   0x5BF8, 0x5DAC, 0x5F60, 0x6114, 0x62C8, 0x647C,
							   0x6630, 0x67E4, 0x6998, 0x6B4C, 0x6D00, 0x6EB4,
							   0x7068, 0x721C, 0x73D0, 0x7584, 0x7738, 0x78EC,
							   0x7AA0, 0x7C54, 0x7E08, 0x7FBC, 0x8170, 0x8324,
							   0x84D8, 0x868C, 0x8840, 0x89F4, 0x8BA8, 0x8D5C,
							   0x8F10, 0x90C4, 0x9278, 0x942C, 0x95E0, 0x9794,
							   0x9948, 0x9AFC, 0x9CB0, 0x9E64, 0xA018, 0xA1CC,
							   0xA380, 0xA534, 0xA6E8, 0xA89C, 0xAA50, 0xAC04,
							   0xADB8, 0xAF6C, 0xB120, 0xB2D4, 0xB488, 0xB63C,
							   0xB7F0, 0xB9A4, 0xBB58, 0xBD0C, 0xBEC0, 0xC074,
							   0xC228, 0xC3DC, 0xC590, 0xC744, 0xC8F8, 0xCAAC,
							   0xCC60, 0xCE14, 0xCFC8, 0xD17C, 0xD330, 0xD4E4,
							   0xD698, 0xD84C, 0xDA00, 0xDBB4, 0xDD68, 0xDF1C,
							   0xE0D0, 0xE284, 0xE438, 0xE5EC, 0xE7A0, 0xE954,
							   0xEB08, 0xECBC, 0xEE70, 0xF024, 0xF1D8, 0xF38C,
							   0xF540, 0xF6F4, 0xF8A8, 0xFA5C, 0xFC10, 0xFDC4,
							   0xFF78};

const uint16_t fade180[201] = {0x0000, 0x0147, 0x028E, 0x03D5, 0x051C, 0x0663,
							   0x07AA, 0x08F1, 0x0A38, 0x0B7F, 0x0CC6, 0x0E0D,
							   0x0F54, 0x109B, 0x11E2, 0x1329, 0x1470, 0x15B7,
							   0x16FE, 0x1845, 0x198C, 0x1AD3, 0x1C1A, 0x1D61,
							   0x1EA8, 0x1FEF, 0x2136, 0x227D, 0x23C4, 0x250B,
							   0x2652, 0x2799, 0x28E0, 0x2A27, 0x2B6E, 0x2CB5,
							   0x2DFC, 0x2F43, 0x308A, 0x31D1, 0x3318, 0x345F,
							   0x35A6, 0x36ED, 0x3834, 0x397B, 0x3AC2, 0x3C09,
							   0x3D50, 0x3E97, 0x3FDE, 0x4125, 0x426C, 0x43B3,
							   0x44FA, 0x4641, 0x4788, 0x48CF, 0x4A16, 0x4B5D,
							   0x4CA4, 0x4DEB, 0x4F32, 0x5079, 0x51C0, 0x5307,
							   0x544E, 0x5595, 0x56DC, 0x5823, 0x596A, 0x5AB1,
							   0x5BF8, 0x5D3F, 0x5E86, 0x5FCD, 0x6114, 0x625B,
							   0x63A2, 0x64E9, 0x6630, 0x6777, 0x68BE, 0x6A05,
							   0x6B4C, 0x6C93, 0x6DDA, 0x6F21, 0x7068, 0x71AF,
							   0x72F6, 0x743D, 0x7584, 0x76CB, 0x7812, 0x7959,
							   0x7AA0, 0x7BE7, 0x7D2E, 0x7E75, 0x7FBC, 0x8103,
							   0x824A, 0x8391, 0x84D8, 0x861F, 0x8766, 0x88AD,
							   0x89F4, 0x8B3B, 0x8C82, 0x8DC9, 0x8F10, 0x9057,
							   0x919E, 0x92E5, 0x942C, 0x9573, 0x96BA, 0x9801,
							   0x9948, 0x9A8F, 0x9BD6, 0x9D1D, 0x9E64, 0x9FAB,
							   0xA0F2, 0xA239, 0xA380, 0xA4C7, 0xA60E, 0xA755,
							   0xA89C, 0xA9E3, 0xAB2A, 0xAC71, 0xADB8, 0xAEFF,
							   0xB046, 0xB18D, 0xB2D4, 0xB41B, 0xB562, 0xB6A9,
							   0xB7F0, 0xB937, 0xBA7E, 0xBBC5, 0xBD0C, 0xBE53,
							   0xBF9A, 0xC0E1, 0xC228, 0xC36F, 0xC4B6, 0xC5FD,
							   0xC744, 0xC88B, 0xC9D2, 0xCB19, 0xCC60, 0xCDA7,
							   0xCEEE, 0xD035, 0xD17C, 0xD2C3, 0xD40A, 0xD551,
							   0xD698, 0xD7DF, 0xD926, 0xDA6D, 0xDBB4, 0xDCFB,
							   0xDE42, 0xDF89, 0xE0D0, 0xE217, 0xE35E, 0xE4A5,
							   0xE5EC, 0xE733, 0xE87A, 0xE9C1, 0xEB08, 0xEC4F,
							   0xED96, 0xEEDD, 0xF024, 0xF16B, 0xF2B2, 0xF3F9,
							   0xF540, 0xF687, 0xF7CE, 0xF915, 0xFA5C, 0xFBA3,
							   0xFCEA, 0xFE31, 0xFF78};


// all fade tables
const uint16_t *fade[]={fade60, fade90, fade120, fade150, fade180};
uint8_t tableSizes[5]= {61,101,121,151,201};

// Function Prototypes
void adc_init(void);
void display_init(void);
void update_display(void);
led make_led(uint8_t index);
void refresh_led(led *deadLED);
void replace_led(led **deadLED, led **freshLED);
uint8_t PRNG (uint8_t min, uint8_t max);
uint16_t adc_read(uint8_t ch);
void print(char *s);


int main(){

	// debug


	if(DEBUG_MODE > 0){
		cli();
		uart_init(UART_BAUD_SELECT(UART_BAUD_RATE,F_CPU));
		sei();
	}

	LEDs = (led *)calloc(12*NUM_DRIVERS, sizeof(led));
	LEDBuffer = (uint16_t *)calloc(TOTAL_CHANNELS, sizeof(uint16_t));
//	LEDBuffer = (uint16_t *)calloc(2, TOTAL_CHANNELS*sizeof(uint16_t));
//	LEDBuffer = (uint16_t *)calloc(2, TOTAL_CHANNELS);


//	check for failed allocation
	if (LEDs==NULL){
		if(DEBUG_MODE > 0){
			print("fl\n\r");
		}
	}

	if (LEDBuffer==NULL){
		if(DEBUG_MODE > 0){
			print("fb\n\r");
		}
	}


//	adc_init();

	

	// seed rand
	// srand(adc_read(0));


		spi_init();
		display_init();
	    spi_write(LEDBuffer, NUM_DRIVERS);


	while(1){

		if(DEBUG_MODE > 0){
			print("u\n\r");
		}

		update_display();
		spi_write(LEDBuffer, NUM_DRIVERS);
		_delay_ms(UPDATE_DELAY);

	}

}


void display_init(void){

	// Create array to hold randomly selected starting points
	int8_t randSelect[ACTIVE_LEDS];
	// initialize random array each element to a value above TOTAL_CHANNELS
	for(uint8_t i=0; i<ACTIVE_LEDS; i++ ){randSelect[i]=TOTAL_CHANNELS;}

	// initialize all leds
	for(uint8_t i=0; i<TOTAL_CHANNELS; i++ ){
		LEDs[i] = make_led(i);
	}


	// Generate unique set of leds to initiate
	for(uint8_t i = 0; i <ACTIVE_LEDS; i++){
		uint8_t select = 0;
		uint8_t new = 0;

		// check that random number has not already been selected
		do{
			select = 0;
			new = PRNG(0, TOTAL_CHANNELS);



			for(uint8_t j=0; j<ACTIVE_LEDS; j++ ){

				// not unique number, generate new
				if (new == randSelect[j]){
					select = 1;
					break;
				}
			}

		}while(select);

		// record the random integer
		randSelect[i] = new;
	}


	// Mark inactive leds
	uint8_t i; // total count
	uint8_t j; // active count
	uint8_t k; // inactive count
	for(i=0, j=0, k=0; i<TOTAL_CHANNELS; i++ ){

		uint8_t select = 0;

		// check if led is active (in randArray)
		for(uint16_t l=0; l<ACTIVE_LEDS; l++){
			if(i == randSelect[l]){
				select = 1;
				break;
			}
		}

		if(select){
			activeLEDs[j] = &LEDs[i];
			j++;
		}
		else{
			inactiveLEDs[k] = &LEDs[i];
			k++;
		}


	}


}


void replace_led(led **deadLED, led **freshLED){

	

	led *tmpLED = *deadLED; 

	*deadLED = *freshLED;
	*freshLED = tmpLED;

	// refresh recently active led's parameters
	refresh_led(*freshLED);

}

led make_led(uint8_t index){

	// select fade speed here

	 uint8_t fadeInSelect = PRNG(0,2);
	 uint8_t fadeOutSelect = PRNG(0,2);


	led new = {
					.startDelayTime= PRNG(60, 180),
					.holdTime= PRNG(60,120),
					.fadeInTable= fade[fadeInSelect],
					.fadeOutTable= fade[fadeOutSelect],
					.fadeInTableSize= tableSizes[fadeInSelect],
					.fadeOutTableSize= tableSizes[fadeOutSelect],
					.fadeLevel= 0,
					.brightness= &(LEDBuffer[index]),
					.stage= ready
				};

	

	return new;

}



void refresh_led(led *deadLED){


	
	// TODO: upper limit is dependent on number of fade tables
	uint8_t fadeInSelect = PRNG(0,2);
	uint8_t fadeOutSelect = (PRNG(0,2));


	// MOD changing default brightness to be 0x0000
	//	(*deadLED).fadeInTableSize = 60 + (fadeInSelect*30);
	//	(*deadLED).fadeOutTableSize = 60 + (fadeOutSelect*30);
	//  *((*deadLED).brightness) = (*deadLED).fadeInTable[0];		// set brightness to 0x0000 upon init

	// Generate and assign random parameters
	(*deadLED).startDelayTime = PRNG(60,180);
	(*deadLED).holdTime = PRNG(60,120);
	(*deadLED).fadeInTable = fade[fadeInSelect];
	(*deadLED).fadeOutTable = fade[fadeOutSelect];
	(*deadLED).fadeInTableSize= tableSizes[fadeInSelect];
	(*deadLED).fadeOutTableSize= tableSizes[fadeOutSelect];
	(*deadLED).fadeLevel = 0;
	*((*deadLED).brightness) = 0x0000;
	(*deadLED).stage = ready;

}


void update_display(void){

	if(DEBUG_MODE > 0){
		print("ud\n\r");
	}
	
	// loop through all active leds
	for(uint8_t i=0; i<ACTIVE_LEDS; i++){

		// jump to state
		switch((*activeLEDs[i]).stage){

			case ready :

				if(DEBUG_MODE > 0){
					print("r\n\r");
				}

				(*activeLEDs[i]).stage = startDelay;
				break;

			case startDelay :

				if(DEBUG_MODE > 0){
					print("r\n\r");
				}

				// if delay has elapsed, go to next stage
				if(!((*activeLEDs[i]).startDelayTime)){
					(*activeLEDs[i]).stage = fadeIn;
				}

				else{
					((*activeLEDs[i]).startDelayTime)--;
				}

				break;

			case fadeIn :

				if(DEBUG_MODE > 0){
					print("fi\n\r");
				}
				
				// if max brightness has been reached go to next stage
				if((*activeLEDs[i]).fadeLevel >= (*activeLEDs[i]).fadeInTableSize){
					(*activeLEDs[i]).stage = hold;
				}

				else{
					*((*activeLEDs[i]).brightness) = (*activeLEDs[i]).fadeInTable[(*activeLEDs[i]).fadeLevel];
					((*activeLEDs[i]).fadeLevel)++;
				}

				break;

			case hold :

				if(DEBUG_MODE > 0){
					print("h\n\r");
				}

				// if delay has elapsed, go to next stage
				if(!((*activeLEDs[i]).holdTime)){
					(*activeLEDs[i]).fadeLevel = ((*activeLEDs[i]).fadeOutTableSize)-1;
					(*activeLEDs[i]).stage = fadeOut;
				}

				else{
					
					((*activeLEDs[i]).holdTime)--;
				}

				break;


			case fadeOut :


				if(DEBUG_MODE > 0){
					print("fo\n\r");
				}

				// if lowest brightness has been reached go to next stage
				// NOTE, fadelevel is unsigned.  If it reaches 0, and then updates again, we have underflow
				if((*activeLEDs[i]).fadeLevel <= 0){
					// *((*activeLEDs[i]).brightness) = (*activeLEDs[i]).fadeOutTable[0];
					*((*activeLEDs[i]).brightness) = 0x0000;
					(*activeLEDs[i]).stage = terminated;
				}

				else{
					*((*activeLEDs[i]).brightness) = (*activeLEDs[i]).fadeOutTable[(*activeLEDs[i]).fadeLevel];
					
					((*activeLEDs[i]).fadeLevel)--;
				}

				break;

			case terminated :


			if(DEBUG_MODE > 0){
					print("t\n\r");
				}
				
				// replace terminated led with randomly select inactive led
				replace_led(&activeLEDs[i], &(inactiveLEDs[PRNG(0,TOTAL_CHANNELS-ACTIVE_LEDS)]));
				break;

		}

	}

}


uint8_t PRNG (uint8_t min, uint8_t max){
	uint8_t value;
	value = ((uint8_t)((double)rand() / ((double)RAND_MAX) * (max - min))) + min;
	return value;

}


// adc used for rand seed
void adc_init(void){

    // AREF = AVcc
    ADMUX = (1<<REFS0);

    // ADC Enable and prescaler of 128
    // 16000000/128 = 125000
    ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);

}

uint16_t adc_read(uint8_t ch){

	// // DEBUG
	// if(DEBUG_MODE >= 2)
	// 	uart_puts("\n\rADC Read\n\r");

	// select the corresponding channel 0~7
	// ANDing with ’7′ will always keep the value
	// of ‘ch’ between 0 and 7
	ch &= 0b00000111;  // AND operation with 7
	ADMUX = (ADMUX & 0xF8)|ch; // clears the bottom 3 bits before ORing

	// start single conversion
	// write ’1′ to ADSC
	ADCSRA |= (1<<ADSC);

	// wait for conversion to complete
	// ADSC becomes ’0′ again
	// till then, run loop continuously
	while(ADCSRA & (1<<ADSC));

	return (ADC);

}

void print(char *s){	
	uart_puts(s);	
	_delay_ms(1);
}

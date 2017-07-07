/*
 * display.c
 *
 * Created: 01/25/17
 * Author : Cory J. Engdahl
 *

 */

// brightness tables

#include "display.h"


void write_display(void){

		update_display();
		spi_write(LEDBuffer, NUM_DRIVERS,0);
		// _delay_ms(UPDATE_DELAY);
}

void setup_display(void){

	LEDs = (led *)calloc(12*NUM_DRIVERS, sizeof(led));
	LEDBuffer = (uint16_t *)calloc(TOTAL_CHANNELS, sizeof(uint16_t));

}


void display_init(void){


	// initialize spi bus
	spi_init();
	// display_init();

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

	spi_write(LEDBuffer, NUM_DRIVERS, 1);


}


void replace_led(led **deadLED, led **freshLED){

	led *tmpLED = *deadLED;
	*deadLED = *freshLED;
	*freshLED = tmpLED;

	// refresh recently active led's parameters
	refresh_led(*freshLED);

}

led make_led(uint8_t index){

	uint8_t fadeInSelect = PRNG(0,2);
	uint8_t fadeOutSelect = PRNG(0,2);

//	uint8_t fadeInSelect = 2;
//	uint8_t fadeOutSelect = 2;


	// MOD Changing brightness to be 0x0000 by default
	// .fadeInTableSize= 60 + (fadeInSelect*30),
	// .fadeOutTableSize= 60 + (fadeOutSelect*30),
	// .brightness= &(LEDBuffer[index]),
	// Tried, may hav cause bug -->

	led new = {
					.startDelayTime= PRNG(60, 180),
					.holdTime= PRNG(60,120),
					.fadeInTable= (uint16_t *) fade[fadeInSelect],
					.fadeOutTable= (uint16_t *) fade[fadeOutSelect],
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
	(*deadLED).fadeInTable = (uint16_t *) fade[fadeInSelect];
	(*deadLED).fadeOutTable = (uint16_t *) fade[fadeOutSelect];
	(*deadLED).fadeInTableSize= tableSizes[fadeInSelect];
	(*deadLED).fadeOutTableSize= tableSizes[fadeOutSelect];
	(*deadLED).fadeLevel = 0;
	*((*deadLED).brightness) = 0x0000;
	(*deadLED).stage = ready;

}


void update_display(void){

	// loop through all active leds
	for(uint8_t i=0; i<ACTIVE_LEDS; i++){

		// jump to state
		switch((*activeLEDs[i]).stage){

			case ready :
				(*activeLEDs[i]).stage = startDelay;
				break;

			case startDelay :

				// if delay has elapsed, go to next stage
				if(!((*activeLEDs[i]).startDelayTime)){
					(*activeLEDs[i]).stage = fadeIn;
				}

				else{
					((*activeLEDs[i]).startDelayTime)--;
				}

				break;

			case fadeIn :

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

				// if lowest brightness has been reached go to next stage
				if((*activeLEDs[i]).fadeLevel <= 0){
					*((*activeLEDs[i]).brightness) = 0x0000;
					(*activeLEDs[i]).stage = terminated;
				}

				else{
					*((*activeLEDs[i]).brightness) = (*activeLEDs[i]).fadeOutTable[(*activeLEDs[i]).fadeLevel];
					((*activeLEDs[i]).fadeLevel)--;
				}

				break;

			case terminated :

				// replace terminated led with randomly select inactive led
				replace_led(&activeLEDs[i], &(inactiveLEDs[PRNG(0,TOTAL_CHANNELS-ACTIVE_LEDS)]));
				break;

		}

	}

}



void clear_leds(void){

	for(uint8_t i = 0; i<TOTAL_CHANNELS; i++){
			LEDBuffer[i] = 0x0000;

	}

	// memset(LEDBuffer, 0, sizeof(uint16_t)*TOTAL_CHANNELS);
	spi_write(LEDBuffer, NUM_DRIVERS, 1);

}




uint8_t PRNG (uint8_t min, uint8_t max){
	uint8_t value;
	value = ((uint8_t)((double)rand() / ((double)RAND_MAX) * (max - min))) + min;
	return value;

}



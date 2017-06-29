#ifndef DISPLAY_H
#define DISPLAY_H


/*
 * display.h
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
#define ACTIVE_LEDS 4								// The number of flys that will be active at any given time
#define UPDATE_DELAY 20								// Delay between updates in milliseconds


// Libs
#include <stdio.h>
#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <util/delay_basic.h>
#include <stdbool.h>
#include "TLC59711.h"

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

// random seed


// Function Prototypes
void display_init(void);
void write_display(void);
void update_display(void);
void setup_display(void);
led make_led(uint8_t index);
void refresh_led(led *deadLED);
void replace_led(led **deadLED, led **freshLED);
uint8_t PRNG (uint8_t min, uint8_t max);


#endif // DISPLAY_H

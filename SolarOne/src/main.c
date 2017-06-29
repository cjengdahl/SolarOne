/***************************************
*  Filename: main.c
*  Author: Cory Engdahl
*  --------------------
*  Firmware for operating solar-powered LED sculpture.
*  	- Manages SOC monitoring, charging, and loading
* 	- Tracks state of day (Night/Day)
*   - Carries out night-time display (LEDs)
***************************************/

/*-------------------------------------
              Libraries
------------------------------------- */

#include <stdlib.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include "uart.h"
#include "display.h"

/*-------------------------------------
                Typedefs
------------------------------------- */
typedef enum {Day,Night}sod;
typedef enum {Charged,Discharged}soc;


/*-------------------------------------
                Globals
------------------------------------- */

uint8_t SODCount;
uint8_t transition;
uint8_t watchdog_set;
uint8_t solarPanelsConnected;
uint8_t ledDriversConnected;
uint8_t ledsConnected;
uint8_t displayEnabled;
uint8_t micro_intialized;
uint16_t displayCount;
sod stateOfDay;
soc stateOfCharge;

/*-------------------------------------
               Definitions
------------------------------------- */

#ifndef F_CPU
#define F_CPU 16000000
#endif

// measurement bench markets
#define MIN_CHARGED_VOLTAGE	12.5
#define ADC_VOLTAGE_UPPER_LIMIT 14.0
#define DAY_THRESHOLD 400

// 10 bit adc value
#define CHARGED_THRESHOLD (MIN_CHARGED_VOLTAGE/ADC_VOLTAGE_UPPER_LIMIT)*1024

// loop parameters
#define NUM_SOC_CHECKS 5
#define NUM_SOD_CHECKS 1 
#define NUM_PHOTOCELLS 3
#define NUM_SOD_INIT_CHECKS 10

// adc channel assigment
#define ADC_VOLTAGE_1_CHANNEL 0
#define ADC_VOLTAGE_2_CHANNEL 1 
#define ADC_PHOTOCELL_1_CHANNEL 2
#define ADC_PHOTOCELL_2_CHANNEL 3
#define ADC_PHOTOCELL_3_CHANNEL 4
#define ADC_TEMPATURE_CHANNEL 5

// TODO: modes (dip switch)
// debug
#define UART_BAUD_RATE 9600

// 0 = no output
// 1 = essentials (states)
// 2 = call trace
#define DEBUG_MODE 1
#define DEBUG_TRACE_DELAY 10

// delays (ms)
#define NUM_SOD_INIT_CHECK_DELAY 10
#define BATTERY_STABILIZE_DELAY 10
#define DISPLAY_UPDATE_DELAY 20					

// counters (minutes)
#define DISPLAY_DURATION 1  

/*-------------------------------------
               Macros
------------------------------------- */

// adc connections
#define CONNECT_PHOTOCELLS() (PORTD |= (1<<PORTD5))					// PortD, Pin 5 (Arduino GPIO 7)
#define DISCONNECT_PHOTOCELLS() (PORTD &= (~(1<<PORTD5)))

#define CONNECT_TEMPERATURE_SENSOR() (PORTD |= (1<<PORTD6))			// PortD, Pin 6 (Arduino GPIO 8)
#define DISCONNECT_TEMPERATURE_SENSOR() (PORTD &= (~(1<<PORTD6)))


// NOTE: For now, assume that the SOC circuit is always connected

// #define CONNECT_BATTERY_VOLTAGE_ADC() (PORTD |= (1<<PORTD4))		// PortD, Pin 4 (Arduino GPIO 4)
// #define DISCONNECT_BATTER_VOLTAGE_ADC() (PORTD &= (~(1<<PORTD4)))

// #define CONNECT_BATTERY_CURRENT_ADC() (PORTD |= (1<<PORTD5))		// PortD, Pin 5 (Arduino GPIO 5)
// #define DISCONNECT_BATTER_CURRENT_ADC() (PORTD &= (~(1<<PORTD5)))


// pv and load connections

// PortD, Pin 2 (Arduino GPIO 2)
#define CONNECT_SOLAR_PANELS(){(PORTD |= (1<<PORTD2));\
							  solarPanelsConnected=1;\
							 }		

#define DISCONNECT_SOLAR_PANELS(){(PORTD &= (~(1<<PORTD2)));\
								 solarPanelsConnected=0;\
								}

// PortD, Pin 3 (Arduino GPIO 3)
#define CONNECT_LED_DRIVERS() {(PORTD |= (1<<PORTD3));\
							   ledDriversConnected=1;\
							  }

#define DISCONNECT_LED_DRIVERS() {(PORTD &= (~(1<<PORTD3)));\
								  ledDriversConnected=0;\
								 }
// PortB, Pin 4 (Arduino GPIO 4)
#define CONNECT_LEDS() {(PORTD |= (1<<PORTD4));\
						ledsConnected=1;\
					   }

#define DISCONNECT_LEDS() {(PORTD &= (~(1<<PORTD4)));\
							ledsConnected=0;\
						  }

// set and clear flags
#define SET_TRANSTION() (transition=1)
#define CLEAR_TRANSTION() (transition=0)
#define ENABLE_DISPLAY() (displayEnabled=1)
#define DISABLE_DISPLAY() (displayEnabled=0)

// state checks
#define IS_CHARGED() (stateOfCharge == Charged)

/*-------------------------------------
             Prototypes
------------------------------------- */

void micro_init(void);
void display_init(void);
void update_display(void);
void update_state_of_charge(void);
void update_state_of_day(void);
void start_timer(void);
void watchdog_init(void);
void start_watchdog_timer(void);
void stop_watchdog_timer(void);
void gpio_init(void);
void adc_init(void);
void sculpture_init(void);
void print(char *s);
uint16_t adc_read(uint8_t ch);
soc decode_charge_state(uint16_t ADCValue);
sod get_majority_day_state_reading(void);
sod decode_day_state(uint16_t ADCValue);

/***************************************
*  Function: main
*  --------------
*  Determine SOD, run either Day or Night
*  routine.  Sleep when inactive
*
***************************************/

int main(void){

	// Note: this calls watchdog_init, adc_init, and gpio_init, and setup_display
	watchdog_init();
	micro_init();
	sculpture_init();

	while(1){

		// need re-initailization after going to in low power mode
		if(!micro_intialized){
			micro_init();
		}

		// update state of day and check for change...
		sod temp = stateOfDay;
		update_state_of_day();

		if (temp != stateOfDay){
			SET_TRANSTION();
			
			// if transitioning out of sleep mode, stop watchdog
			if(watchdog_set){
				stop_watchdog_timer();
			}

		}

		// if not transition and display is not enabled, enable sleep
		else{
			if(!displayEnabled){
				sleep_enable();
			}
		}


		switch(stateOfDay){

			case Day:

				if(DEBUG_MODE > 0){
					print("DM\n\r");
				}
				
				if(transition){

					if(DEBUG_MODE > 0){
						print("N->D\n\r");	
					}

					CLEAR_TRANSTION();
					
					if(!solarPanelsConnected){
						CONNECT_SOLAR_PANELS();
					}

					if(ledsConnected){
						DISCONNECT_LEDS();
					}

					if(ledDriversConnected){
						DISCONNECT_LED_DRIVERS();
					}

					set_sleep_mode(SLEEP_MODE_PWR_DOWN);
					sleep_enable();
					start_watchdog_timer();

					// TODO: Will need to create "soft shutdown for display"

				}

				if(DEBUG_MODE > 0){
					print("slp\n\r");	
				}

				sleep_cpu();
				break;

			case Night:

				if(DEBUG_MODE > 0){
					print("NM\n\r");
				}
				
				// Assume insufficient SOC
				DISABLE_DISPLAY();

				if(transition){

					if(DEBUG_MODE > 0){
						print("D->N\n\r");	
					}	

					CLEAR_TRANSTION();
					update_state_of_charge();

					// NOTE: Probably can keep solar panels connected
					// CONNECT_SOLAR_PANELS();

					if(IS_CHARGED()){

						ENABLE_DISPLAY();
						CONNECT_LED_DRIVERS();
						// display_init(); 
						CONNECT_LEDS();

						// initialize countdown timer seconds
						displayCount = (DISPLAY_DURATION*60)-1;
						start_timer();
						
					}

				}

				while(displayEnabled){
					update_display();
					_delay_ms(DISPLAY_UPDATE_DELAY);
				}

				if(!watchdog_set){
					set_sleep_mode(SLEEP_MODE_PWR_DOWN);
					sleep_enable();
					start_watchdog_timer();
				}

				// if display is disabled, go to sleep
				sleep_cpu();
				break;

			default:
				if(DEBUG_MODE > 0){
					uart_puts("?\n\r");
				}
			    break;
		}

	}

}

/***************************************
*  Function: ISR - Watch-dog Vector
*  -----------------------
*  ISR for watch-dog timer.  This
*  function wakes up the micro from
*  power-down mode and disables sleep.
***************************************/

ISR(WDT_vect)
{
	// disable sleep until we know its safe
	sleep_disable();
	micro_intialized = 0;
	return;
}

/***************************************
*  Function: ISR - Timer1 Vector 
*  -----------------------------
*  ISR for Timer1.  When the timer is
*  active, this ISR fires on one second
*  intervals.  This ISR decrements a
*  counter used to control the operating
*  time of the load.  Once the counter
*  reaches zero, a flag is toggled to
*  disable the load. Every 8 cycles check
*  SOD.
***************************************/

ISR(TIMER1_COMPA_vect){

	if(DEBUG_MODE == 2){
		print("TISR\n\r");
	}

	--displayCount;

	// to mimic day sleep, check sod every 8 seconds
	if(displayCount % 8 == 0){
		sod temp = stateOfDay;
		update_state_of_day();
		if(temp != stateOfDay){
			SET_TRANSTION();
			DISABLE_DISPLAY();
		}
	}

	if(displayCount == 0 ){

		if(DEBUG_MODE == 2){
			print("TMR elapsed\n\r");
		}
		// stop timer, via pre-scale clear
		TCCR1B &= 0xFFF8;
		DISABLE_DISPLAY();
		// TODO: may want to implement soft shutdown...
		DISCONNECT_LEDS();
		DISCONNECT_LED_DRIVERS();

	}

	return;

}

///***************************************
//*  Function: updateDisplay
//*
//***************************************/
//
//
//void update_display(void){
//
//	// DEBUG
//	if (DEBUG_MODE == 2){
//		print("update_display()\n\r");
//	}
//
//
//	// TODO: Replace this function with actual call
//
//
//}


///***************************************
//*  Function: display_init
//*
//***************************************/
//
//
//void display_init(){
//
//	if (DEBUG_MODE == 2){
//		print("display_init()\n\r");
//	}
//
//	// intialize counter and setup timer interrupt to trigger once a second
//
//
//}

/***************************************
*  Function:  update_state_of_charge
*  ---------------------------------
*  Ensures that the source is connected
*  to battery.  Ensures that battery has
*  been at rest for a sufficient amount
*  of time.  Checks SOC of battery.
*  If sufficiently charged, sets
*  stateOfCharge to Charged, otherwise
*  sets stateOfCharge to Discharged.
***************************************/

void update_state_of_charge(void){

	if(DEBUG_MODE == 2){
		print("SOC\n\r");
	}

	DISCONNECT_SOLAR_PANELS();
	_delay_ms(BATTERY_STABILIZE_DELAY);

	// Note: right now, assume circuit is always connected
	// CONNECT_BATTERY_VOLTAGE_ADC();

	soc temp, temp2;
	uint8_t repeat;

	do{
	
		temp = decode_charge_state(adc_read(ADC_VOLTAGE_1_CHANNEL));
		repeat = 0;

		// check multiple times for stability
		for(uint8_t i =0; i<NUM_SOC_CHECKS; i++){

			temp2 = decode_charge_state(adc_read(ADC_VOLTAGE_1_CHANNEL));

			// if inconsistent read, start over
			if(temp != temp2){
				repeat = 1;
				break;
			}

			temp = temp2;
		}

	}while(repeat);

	if(DEBUG_MODE > 0){
		switch(temp){
			case Charged:
				print("CHRG\n\r");
				break;
			case Discharged:
				print("DCHRG\n\r");
				break;
		}
	}
	stateOfCharge = temp;

	// Note: right now assume circuit is always connected
	// DISCONNECT_BATTER_VOLTAGE_ADC();

}

/***************************************
*  Function:  decode_charge_state
*  ------------------------------
*	Decodes 10bit adc value to soc type
***************************************/

soc decode_charge_state(uint16_t ADCValue){

	if(DEBUG_MODE == 2){
		print("decodeSOC\n\r");
	}
	
	if(ADCValue>=CHARGED_THRESHOLD){
		return Charged;
	}

	else{
		return Discharged;
	}

}

/***************************************
*  Function: update_state_of_day
*  -----------------------------
*  Reads in majority value of
*  photo-resistor(s) and compares it to
*  the last reading. Take continual
*  readings based on SOD_CHECKS. Ensure
*  that all SOD_CHECKS are consistent,
*  otherwise start again.
***************************************/

void update_state_of_day(void){

	if(DEBUG_MODE == 2){
		print("SOD\n\r");
	}

	CONNECT_PHOTOCELLS();
	sod temp, temp2;
	uint8_t repeat;

	do{

		temp = get_majority_day_state_reading();
		repeat = 0;

		for(uint8_t i = 0; i<NUM_SOD_CHECKS; i++){

			temp2 = get_majority_day_state_reading();

			if(temp != temp2){
				repeat = 1;
				break;
			}

			temp = temp2;

		}

	}while(repeat);

	stateOfDay = temp;
	DISCONNECT_PHOTOCELLS();

}


/******************************************
*  Function: get_majority_day_state_reading
*  ----------------------------------------
*  Reads all photocells and returns the
*  majority reading.
*******************************************/

sod get_majority_day_state_reading(void){

	if(DEBUG_MODE == 2){
		print("mjrty\n\r");
	}

	uint8_t dayCount = 0;
	uint8_t nightCount = 0;

	for(uint8_t i=ADC_PHOTOCELL_1_CHANNEL; i<=ADC_PHOTOCELL_3_CHANNEL; i++){
		if(decode_day_state(adc_read(i)) == Day){
			dayCount++;
		}

		else{
			nightCount++;
		}
	}

	return (dayCount > nightCount) ? Day : Night;
}

/***************************************
*  Function: decode_day_state
*  --------------------------
*	Decodes 10 bit adc value to sod type.
***************************************/

sod decode_day_state(uint16_t ADCValue){

	if(DEBUG_MODE == 2){
		print("decodeSOD\n\r");
	}

	if(ADCValue<=DAY_THRESHOLD){
		return Day;
	}

	else{
		return Night;
	}

}

/***************************************
*  Function: watchdog_init
*  -----------------------
*  Prevents continuous reseting.  
*  Initializes watch-dog time to interrupt
*  mode and prescales counter to interrupt
*  every 8 seconds.
***************************************/

void watchdog_init(void){

	if(DEBUG_MODE == 2){
		print("w_init\n\r");
	}

	if(MCUSR & _BV(WDRF)){            			// If a reset was caused by the Watch-dog Timer
	    MCUSR &= ~_BV(WDRF);                 	// Clear the WDT reset flag
	    WDTCSR |= (_BV(WDCE) | _BV(WDE));   	// Enable the WD Change Bit
	    WDTCSR = 0x00;                      	// Disable the WDT
	}
	watchdog_set = 0;
}

/***************************************
*  Function: start_watchdog_timer
*  ------------------------------
*  Set watchdog timer to fire every 8
*  seconds.  Start timer.
***************************************/

void start_watchdog_timer(void){

	if(DEBUG_MODE == 2){
		print("str_wdog\n\r");
	}

	// temporarily disable interrupts
	cli();

	// Set up Watch Dog Timer for 8 second
	WDTCSR |= (_BV(WDCE) | _BV(WDE));   // Enable the WD Change Bit
	//WDTCSR = 0x61;					// Interrupt MODE, 8 seconds
	WDTCSR = _BV(WDIE) |              	// Enable WDT Interrupt
          	 _BV(WDP3) | _BV(WDP0);   	// Set Timeout to ~8seconds
	watchdog_set = 1;
	// re-enable interrupts
	sei();

}

/***************************************
*  Function: stop_watchdog_timer
*  -----------------------------
*  Stop watchdog timer.
***************************************/

void stop_watchdog_timer(void){

	if(DEBUG_MODE == 2){
		print("stp_wdog\n\r");
	}

	// temporarily disable interrupts
	cli();

	// Set up Watch Dog Timer for Inactivity
	WDTCSR |= (_BV(WDCE) | _BV(WDE));   // Enable the WD Change Bit
	WDTCSR = 0x00;				        // Stopped MODE (disabled)

	// re-enable interrupts
	watchdog_set = 0;
	sei();

}

/***************************************
*  Function: adc_init
*  ------------------
*  Initializes ADC by enabling it and
*  setting ADC prescale to 125kHz.
*  Because we are using an external
*  clock (16MHz) we need to user the
*  highest prescaler to get the ADC 
*  in an operable range (50-200kHz)
***************************************/

void adc_init(void){
	
	if(DEBUG_MODE == 2){
		print("adc_init\n\r");
	}

    // AREF = AVcc
    ADMUX = (1<<REFS0);

    // ADC Enable and prescaler of 128
    // 16000000/128 = 125000
    ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
}

/***************************************
*  Function: adc_read
*  ------------------
*  Reads the value of the specified
*  ADC channel and returns 10 bit value.
***************************************/

uint16_t adc_read(uint8_t ch){

	if(DEBUG_MODE == 2){
		print("adc_read\n\r");
	}

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


/***************************************
*  Function: start_timer
*  ---------------------
*  Sets up TIMER1 (16bit timer) to clear
*  on compare with the number of cycles
*  needed to reach 1 second.  Using a 
*  prescaler of 1024, it takes approx.
*  15,624 timer cycles to reach one second.
*  Clock is enabled upon setting prescaler
***************************************/

void start_timer(void){

	if(DEBUG_MODE == 2){
		print("str_tmr\n\r");
	}

	cli();
	// CTC (16MHz/1024)-1 = 15,624Hz
    OCR1A = 0x3D08;
    // Mode 4, CTC on OCR1A
    TCCR1B |= (1 << WGM12);
    //Set interrupt on compare match
    TIMSK1 |= (1 << OCIE1A);
    // set prescaler to 1024 and start the timer
    TCCR1B |= (1 << CS12) | (1 << CS10);
    // enable interrupts
    sei();

}

/***************************************
*  Function: gpio_init
*  -------------------
*  Sets direction of GPIO ports.
***************************************/

void gpio_init(void){

	if(DEBUG_MODE == 2){
		print("gpio_init\n\r");
	}

	//PORTC as input (ADCs)
	DDRC = 0x00;   

	//PORTD as output (enables)
	DDRD = 0xFF;   

	// TODO: Need to handle portB as well, single GPIO for switching, not sure what the default is

}

/***************************************
*  Function: sculputure_init
*  -------------------------
*  Ensures all peripheral devices are
*  disabled.  Updates state of day.
*  Sets transition flag.
***************************************/

void sculpture_init(void){

	if(DEBUG_MODE == 2){
		print("s_init\n\r");
	}
	
	//	setup_display();
	DISCONNECT_LEDS();
	DISCONNECT_LED_DRIVERS();
	DISCONNECT_LEDS();
	DISCONNECT_TEMPERATURE_SENSOR();

	// TODO: Consider uncommenting below to improve reliability
	update_state_of_day();

	// Start up is an automatic transition
	SET_TRANSTION();

	// uint8_t repeat;
	// sod temp, temp2;

	// do{

	// 	update_state_of_day();
	// 	temp = stateOfDay;
	// 	repeat = 0;

	// 	// set initial state of day
	// 	for(uint8_t i = 0; i<NUM_SOD_INIT_CHECKS; i++){

	// 		update_state_of_day();
	// 		temp2 = stateOfDay;

	// 		if(temp != temp2){
	// 			repeat = 1;
	// 			break;
	// 		}

	// 		temp = temp2;

	// 		// let some time pass before next read, for reliability
	// 		_delay_ms(NUM_SOD_INIT_CHECK_DELAY);

	// 	}


	// }while(repeat);

}

/***************************************
*  Function: Print
*  ---------------
*  Alias for uart_puts with added print
*  delay.
***************************************/

void print(char *s){	
	uart_puts(s);	
	_delay_ms(DEBUG_TRACE_DELAY);
}

/***************************************
*  Function: micro_init
*  -------------------
*  calls init for supporting funtionality
*  including:  UART, adc, gpios
***************************************/

void micro_init(void){

		if(DEBUG_MODE != 0){
			cli();
			uart_init(UART_BAUD_SELECT(UART_BAUD_RATE,F_CPU));
			sei(); 
		}

		if(DEBUG_MODE == 2){
			print("m_init\n\r");
		}

		adc_init();
		gpio_init();
		_delay_ms(10);	// ~.01 second delay
		micro_intialized = 1;
}
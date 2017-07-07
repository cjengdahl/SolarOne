#ifndef _TLC59711_H
#define _TLC59711_H

// SPI Macros
#define CLOCK_HIGH() (PORTB |= (1<<PB5))   // Devboard pin 13
#define CLOCK_LOW() (PORTB &= (~(1<<PB5)))
#define DATA_HIGH() (PORTB |= (1<<PB4))    // Devboard pin 12
#define DATA_LOW() (PORTB &= (~(1<<PB4)))
//#define CLOCK_HIGH() (PORTB |= (1<<PB0))   // Devboard pin 8
//#define CLOCK_LOW() (PORTB &= (~(1<<PB0)))
//#define DATA_HIGH() (PORTD |= (1<<PD7))    // Devboard pin 7
//#define DATA_LOW() (PORTD &= (~(1<<PD7)))

// RGB brightness levels. Not using for RGB LEDs, set to full brightness
#define BC_R 0x7F     
#define BC_G 0x7F
#define BC_B 0x7F

// Fucntion Prototypes
void set_brightness(uint8_t channel, uint16_t brightness, uint16_t *pwmBuffer);
void spi_init(void);
void spi_write(uint16_t *pwmBuffer, uint8_t numDrivers);
void transfer(uint32_t data); 

#endif

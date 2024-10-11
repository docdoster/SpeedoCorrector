

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/atomic.h>
#include <util/delay.h>

//#include "avr_mcu_section.h"

volatile uint16_t output_period;
volatile uint16_t speedo_count;
volatile uint16_t main_clk;
volatile uint8_t wdt_count;

void init_AD9833(void);
void AD9833_send_16(uint16_t);
void AD9833_send_32(uint16_t, uint16_t);	
void AD9833_set_freq(uint32_t, uint8_t);

#define SCLK PB0
#define SDATA PB1
#define FSYNC PB3

#define CONVERSION (uint32_t) 629434862 

#define FREQ0 0

//const struct avr_mmcu_vcd_trace_t _mytrace[] _MMCU_ = {
//	{ AVR_MCU_VCD_SYMBOL("speedo_count"), .what = (void*) &speedo_count, },
//	{ AVR_MCU_VCD_SYMBOL("main_clk"), .what = (void*) &main_clk, },
//};

void main(void) {

	uint16_t copyof_speedo_count;
	uint16_t output_freq;

	uint16_t i;
	
	/* 	PB0 = SCLK  -> Output -> Pin 5
		PB1 = SDATA -> Output -> Pin 6
		PB2 = INT0  -> Input  -> Pin 7
		PB3 = FSYNC -> Output -> Pin 2
	*/

	/* PB0, PB1 output, PB2 input.*/
	DDRB = 1<<DDB0 | 1<<DDB1 | 1<<DDB3;
	/* Pin 0, 1 and 3 are set low. All others have pullup enabled. */
	PORTB = 0xFF & ~(1<<PB0) & ~(1<<PB1) & ~(1<<PB3);

	/* Timer 1 */
	/* CLK/4 */
	TCCR1 = 0x03;
	/* Normal mode. */
	GTCCR = 0x00;
	/* Set timer values. */
	TCNT1 = 1;
	OCR1A = 0;
        /* Enable timer 1 interrupt. */
	TIMSK = TIMSK | (1<<OCIE1A);
	TIFR = 0;

	/* Warchdog timer. */
	/* Enable */
	MCUSR = MCUSR | (1<<WDRF); 
	/* Config */
	WDTCR = 0xCA;
	
	/* INT0 */
	/* Falling edge */
	MCUCR = MCUCR | (1<<ISC01);
	MCUCR = MCUCR & ~(1<<ISC00);
	/* no pin change on interrupt */
	PCMSK = 0;
	/* Enable INT0 interrupt */
	GIMSK = GIMSK | (1<<INT0);
	GIMSK = GIMSK & ~(1<<PCIE);

	main_clk = 0;
	output_period = 0x8000;
	wdt_count = 0;
	
	sei();

	/* Setup AD9833. */
	_delay_ms(10);	
	init_AD9833();
	
	i = 1000;
	
	while(1) {

		/* Get the latest speedo input count. */
		ATOMIC_BLOCK(ATOMIC_FORCEON) {
			copyof_speedo_count = speedo_count;
		}

		if (copyof_speedo_count < 20000) { 
			output_freq = (uint16_t) (CONVERSION / copyof_speedo_count);
		} else {
			output_freq = 0;
		}
		
		AD9833_set_freq(i, FREQ0);
		if (i > 20000) {
			i = 1000;
		} else {
			i=i+50;
		}
		
		//AD9833_send_16(output_freq);
		
		/* Essentially pause here for one WDT. */
		while(wdt_count == 0) {
		}
		wdt_count--;

	}
}

void init_AD9833(void) {
	
	/* Send reset, single operation to write LSB, MSB, Use FREQ0 and PHASE0. */
	AD9833_send_16(0x2100);
	/* Set FREQ0 to 0 Hz. */
	AD9833_set_freq(0, FREQ0);
	/* Set PHASE0 to 0. */
	AD9833_send_16(0xC000);
	/* Exit Reset */
	AD9833_send_16(0x2000);
	
}

void AD9833_set_freq(uint32_t value, uint8_t output) {

	uint32_t MSB, LSB;
	
	if (output == 1) {
		MSB = 0x8000;
		LSB = 0x8000;
	} else {
		MSB = 0x4000;
		LSB = 0x4000;
	}
	
	LSB = LSB | (value & 0x3FFF);
	MSB = MSB | ((value & 0xFFFC000)>>14);
	
	/* Hold reset during change of freq to prevent odd outputs */
	AD9833_send_16(0x2100);
	AD9833_send_32(LSB, MSB);
	AD9833_send_16(0x2000);
}

void AD9833_send_16(uint16_t word) {
	
	uint8_t	i;
	
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		
		/* Start condition SCLK high, FSYNC high, SDATA high */
		PORTB = PORTB | (1<<SCLK) | (1<<FSYNC);
		/* FSYNC goes low to select AD9833. */
		PORTB = PORTB & ~(1<<FSYNC);
		/* Start loop for 16 bits out. */
		for(i=0; i<16; i++) {
			/* Set data bit. */
			if ( (word & 0x8000) == 0) {
				PORTB = PORTB & ~(1<<SDATA);
			} else { 
				PORTB = PORTB | (1<<SDATA);
			}
			/* SCLK down. */
			PORTB = PORTB & ~(1<<SCLK);
			/* Shift the word down one bit. Do it here as a slight delay to the SCLK low event. */
			word = word << 1;
			_delay_us(1);
			/* SCLK up. */
			PORTB = PORTB | (1<<SCLK);
		}
		/* FSYNC up */
		PORTB = PORTB | (1<<FSYNC);
		PORTB = PORTB | (1<<SDATA);
	
	}
}

void AD9833_send_32(uint16_t LSB, uint16_t MSB) {
	
		uint8_t	i;
	
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		
		/* Start condition SCLK high, FSYNC high, SDATA X */
		PORTB = PORTB | (1<<SCLK) | (1<<FSYNC);
		/* FSYNC goes low to select AD9833. */
		PORTB = PORTB & ~(1<<FSYNC);
		
		/* Start loop for 16 bits out. LSB */
		for(i=0; i<16; i++) {
			/* Set data bit. */
			if ( (LSB & 0x8000) == 0) {
				PORTB = PORTB & ~(1<<SDATA);
			} else { 
				PORTB = PORTB | (1<<SDATA);
			}
			/* SCLK down. */
			PORTB = PORTB & ~(1<<SCLK);
			/* Shift the word down one bit. Do it here as a slight delay to the SLCK low event. */
			LSB = LSB << 1;
			_delay_us(1);
			/* SCLK up. */
			PORTB = PORTB | (1<<SCLK);
		}
		
	    /* Start loop for 16 bits out. MSB*/
		for(i=0; i<16; i++) {
			/* Set data bit. */
			if ( (MSB & 0x8000) == 0) {
				PORTB = PORTB & ~(1<<SDATA);
			} else { 
				PORTB = PORTB | (1<<SDATA);
			}
			/* SCLK down. */
			PORTB = PORTB & ~(1<<SCLK);
			/* Shift the word down one bit. Do it here as a slight delay to the SLCK low event. */
			MSB = MSB << 1;
			_delay_us(1);
			/* SCLK up. */
			PORTB = PORTB | (1<<SCLK);
		}
		
		/* FSYNC up */
		PORTB = PORTB | (1<<FSYNC);
		PORTB = PORTB | (1<<SDATA);
	}
}

ISR(TIMER1_COMPA_vect) {
	main_clk++;
}

ISR(WDT_vect) {

	wdt_count++;
	WDTCR = WDTCR | (1<<WDIE);

}

ISR(INT0_vect) {

	/* This is the input from the speed sensor on the transmission. When the int is triggered we see how many 
	 * timer 1 counts have occured since the last interrupt. */

	int16_t count_delta;
	static uint16_t count_last;

	count_delta = main_clk - count_last;
	if(count_delta < 0) {
		count_delta =+ 2^16;
	}
	count_last = main_clk;

	speedo_count = count_delta;
	
}

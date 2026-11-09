/*
 * Custom Library Function: delay_ms
 * Generates a rough ONE  millisecond delay using a software loop.
 *  For a 11.0592 MHz crystal oscillator.
 */

#include <reg51.h>


sbit LED = P1^0;

void delay_ms(unsigned int ms) {
    unsigned int  j;
	unsigned int i;
    for(i = 0; i < ms; i++) 
	{
        for(j = 0; j < 120; j++); 
    }
}

void main() {
    while(1) {
        LED = ~LED;      
        delay_ms(500);   // Wait for 500ms (Half a second)
    }
}
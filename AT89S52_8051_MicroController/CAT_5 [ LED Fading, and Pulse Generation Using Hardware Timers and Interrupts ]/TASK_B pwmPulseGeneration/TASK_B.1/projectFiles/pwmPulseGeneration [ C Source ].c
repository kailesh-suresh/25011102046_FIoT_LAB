

#include <reg51.h>

sbit LED = P1^0;

unsigned char count = 0;
unsigned char duty = 0;
unsigned int speed = 0;
bit up = 1;

void timer0_ISR(void) interrupt 1
{
    TH0 = 0xFF;
    TL0 = 0xA4;          // Timer interrupt every ~100 us

    /* PWM */
    if(count < duty)
        LED = 1;
    else
        LED = 0;

    count++;

    if(count >= 10)
        count = 0;

    /* Change brightness slowly */
    speed++;
 
    if(speed >= 500)
    {
        speed = 0;

        if(up == 1)
        {
            duty++;

            if(duty >= 10)
                up = 0;
        }
        else
        {
            duty--;

            if(duty == 0)
                up = 1;
        }
    }
}

void main()
{
    TMOD = 0x01;       // Timer 0, Mode 1

    TH0 = 0xFF;
    TL0 = 0xA4;

    ET0 = 1;           // Enable Timer 0 interrupt
    EA = 1;            // Enable global interrupt
    TR0 = 1;           // Start Timer 0

    while(1);
}


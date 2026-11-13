//Variable frequency - square wave generation

#include <reg52.h>

sbit OUTPUT = P1^1;

unsigned char reload_high = 0xFE;
unsigned char reload_low  = 0x33;

void set_frequency(unsigned char mode)
{
    switch(mode)
    {
        case 1:
            reload_high = 0xEE;
            reload_low  = 0x00;
            break;

        case 2:
            reload_high = 0xFC;
            reload_low  = 0x66;
            break;

        case 3:
            reload_high = 0xFE;
            reload_low  = 0x33;
            break;

        case 4:
            reload_high = 0xFF;
            reload_low  = 0x1A;
            break;

        case 5:
            reload_high = 0xFF;
            reload_low  = 0xA4;
            break;

        default:
            break;
    }
}

void timer1_ISR(void) interrupt 3
{
    TH1 = reload_high;
    TL1 = reload_low;

    OUTPUT = ~OUTPUT;
}

void main(void)
{
    TMOD = 0x10;

    TH1 = reload_high;
    TL1 = reload_low;

    ET1 = 1;
    EA = 1;
    TR1 = 1;

    while(1)
    {
        /*
           Call set_frequency(1), (2), etc.
           here according to keypad input.
        */
    }
}
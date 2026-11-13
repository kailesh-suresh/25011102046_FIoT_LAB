//Burst signal 

#include <reg52.h>

sbit OUTPUT = P1^1;

unsigned char count = 0;

void timer1_ISR(void) interrupt 3
{
    TH1 = 0xFC;
    TL1 = 0x66;

    if(count < 10)
    {
        OUTPUT = ~OUTPUT;
    }
    else if(count < 20)
    {
        OUTPUT = 0;
    }
    else
    {
        OUTPUT = ~OUTPUT;
    }

    count++;

    if(count >= 30)
    {
        count = 0;
    }
}

void main(void)
{
    OUTPUT = 0;

    TMOD = 0x10;

    TH1 = 0xFC;
    TL1 = 0x66;

    ET1 = 1;
    EA = 1;
    TR1 = 1;

    while(1)
    {
    }
}
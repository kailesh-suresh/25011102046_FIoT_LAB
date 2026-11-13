
#include <reg52.h>

sbit OUTPUT = P1^1;

unsigned char count = 0;

void timer1_ISR(void) interrupt 3
{
    TH1 = 0xFC;
    TL1 = 0x66;

    if(count == 0)
    {
        OUTPUT = 1;
    }
    else
    {
        OUTPUT = 0;
    }

    count++;

    if(count >= 8)
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

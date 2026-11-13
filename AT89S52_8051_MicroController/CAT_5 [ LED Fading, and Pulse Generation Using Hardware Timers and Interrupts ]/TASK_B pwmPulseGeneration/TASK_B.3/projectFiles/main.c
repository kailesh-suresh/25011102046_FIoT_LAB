//Variable duty cycle wave (very similar to square wave generation)

#include <reg52.h>

sbit LED = P1^0;

unsigned char pwm_count = 0;
unsigned char duty = 0;

unsigned char fade_count = 0;
unsigned char direction = 1;

void timer1_ISR(void) interrupt 3
{
    TH1 = 0xFF;
    TL1 = 0xF7;

    if(pwm_count < duty)
    {
        LED = 1;
    }
    else
    {
        LED = 0;
    }

    pwm_count++;

    if(pwm_count >= 100)
    {
        pwm_count = 0;

        fade_count++;

        if(fade_count >= 10)
        {
            fade_count = 0;

            if(direction == 1)
            {
                duty++;

                if(duty >= 100)
                {
                    duty = 100;
                    direction = 0;
                }
            }
            else
            {
                if(duty > 0)
                {
                    duty--;
                }

                if(duty == 0)
                {
                    direction = 1;
                }
            }
        }
    }
}

void main(void)
{
    LED = 0;

    TMOD = 0x10;

    TH1 = 0xFF;
    TL1 = 0xF7;

    ET1 = 1;
    EA = 1;
    TR1 = 1;

    while(1)
    {
    }
}
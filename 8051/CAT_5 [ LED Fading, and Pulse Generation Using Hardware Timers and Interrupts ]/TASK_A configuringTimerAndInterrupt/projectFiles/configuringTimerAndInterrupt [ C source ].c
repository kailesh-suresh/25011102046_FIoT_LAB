#include<reg51.h>


/* sbit P0_x=P0^6;
void delay()
{
    unsigned char k;
    TMOD=0x01;
    TR0=1;
   for(k=0;k<=100;k++)
   { 
    TL0=0x00;
    TH0=0xDC;
    while(TF0!=1);
    TF0=0;
   }
}

void main()
{

    while(1)
    {
        P1_0=~P1_0;
        delay(); // 1 second delay

    }
}*/

sbit P1_0=P1^0;
unsigned char k=0;

void timer0_isr(void) interrupt 1
{
	//TMOD=0x01;
	TL0=0x00;
	TH0=0xDC;
	TF0=0;
	k++;
	
	if(k>=100)
	{
		P1_0=~P1_0;
		//P0_x=~P0_x;
		k=0;
		
	}

}

void main()
{
	TMOD=0x01;
	TL0=0x00;
	TH0=0xDC;
	TR0=1;
	
	ET0=1;
	EA=1;
    while(1)
    {
     PCON |= 0x01;  // sleep mode 

    }
	}
#include <reg51.h>

/*sbit LED0 = P1^0;
sbit LED1 = P1^1;
sbit LED2 = P1^2;
sbit LED3 = P1^3;*/

sbit prt23  = P2^3; //Switch.
sbit prt26 = P2^6;


unsigned char state = 0;

void delay_ms(unsigned int ms)
{
    unsigned int i;
    unsigned char j;
    for(i = 0; i < ms; i++){
        for(j = 0; j < 127; j++);
}
}

unsigned char button_pressed()
{
	if (prt26 == 0) {             // Check if button is pressed (Active LOW)
        delay_ms(20);            // Simple software debounce
        if (prt26 == 0) {
            while (prt26 == 0);  // WAIT HERE until the button is released!
            return 1;            // Register a single button press
        }
    }
    return 0;
}
	


void state_running(void)
{
    static unsigned char pos = 0;
    P1 = (P1 & 0xF0) | (0x01 << pos);
    
    pos++;
    if(pos > 3) pos = 0;
    delay_ms(500);
}

// State 1: Binary Counter (0 to 15)
void state_counter(void)
{
    static unsigned char count = 0;
    
    P1 = (P1 & 0xF0) | (count & 0x0F);
    
    count++;
    if(count > 15) count = 0;
    delay_ms(500);
}

// State 2: Alternating pattern (0101 <-> 1010)
void state_alternating(void)
{
    static unsigned char toggle = 0;
  if(toggle)
        P1 = (P1 & 0xF0) | 0x05;   // LED0 & LED2 ON
    else
        P1 = (P1 & 0xF0) | 0x0A;   // LED1 & LED3 ON
    
    toggle = !toggle;
    delay_ms(500);
}

// State 3: All LEDs flash together
void state_all_flash(void)
{
    static unsigned char flash = 0;
    
    if(flash)
        P1 = (P1 & 0xF0) | 0x0F;   // All ON
    else
        P1 = (P1 & 0xF0) | 0x00;   // All OFF
    
    flash = !flash;
    delay_ms(50 0);
}

void main(void)
{
    
   // P1 = P1 & 0xF0;
	prt23=0;
	prt26=1;

	
	
    while(1)
    {
        // Check button with debounce
        if(button_pressed())
        {
            state++;
            if(state > 3) state = 0;
        }
        switch(state)
        {
            case 0: state_running();      break;
            case 1: state_counter();      break;
            case 2: state_alternating();  break;
            case 3: state_all_flash();    break;
            default: state = 0;           break;
        }
    }
}
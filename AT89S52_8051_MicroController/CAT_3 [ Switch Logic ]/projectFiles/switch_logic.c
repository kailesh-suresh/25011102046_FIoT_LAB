#include <reg51.h>

// --- Hardware Connections ---
// LEDs are connected to P1.0, P1.1, P1.2, P1.3 (Lower nibble of Port 1)
// Push-button is connected to P2.6
sbit prt26 = P2^6; // Switch (Active LOW)

// Global state variable
unsigned char state = 0;

// --- Delay Function ---
void delay_ms(unsigned int ms)
{
    unsigned int i;
    unsigned char j;
    for(i = 0; i < ms; i++){
        for(j = 0; j < 127; j++);
    }
}

// --- Button Debounce Function ---
unsigned char button_pressed()
{
    if (prt26 == 0) {             // Check if button is pressed (Active LOW)
        delay_ms(20);             // Simple software debounce
        if (prt26 == 0) {
            while (prt26 == 0);   // WAIT HERE until the button is released!
            return 1;             // Register a single button press
        }
    }
    return 0;
}
    
// --- Mode 1: Running LED ---
void state_running(void)
{
    static unsigned char pos = 0;
    P1 = (P1 & 0xF0) | (0x01 << pos); // Shift the 1 across the lower 4 bits
    
    pos++;
    if(pos > 3) pos = 0;
    delay_ms(500);
}

// --- Mode 2: Binary Counter (0 to 15) ---
void state_counter(void)
{
    static unsigned char count = 0;
    
    P1 = (P1 & 0xF0) | (count & 0x0F); // Display count on lower 4 bits
    
    count++;
    if(count > 15) count = 0;
    delay_ms(500);
}

// --- Mode 3: Alternating pattern (0101 <-> 1010) ---
void state_alternating(void)
{
    static unsigned char toggle = 0;
    if(toggle)
        P1 = (P1 & 0xF0) | 0x05;   // LED0 & LED2 ON (0101)
    else
        P1 = (P1 & 0xF0) | 0x0A;   // LED1 & LED3 ON (1010)
    
    toggle = !toggle;
    delay_ms(500);
}

// --- Mode 4: All LEDs flash together ---
void state_all_flash(void)
{
    static unsigned char flash = 0;
    
    if(flash)
        P1 = (P1 & 0xF0) | 0x0F;   // All ON (1111)
    else
        P1 = (P1 & 0xF0) | 0x00;   // All OFF (0000)
    
    flash = !flash;
    delay_ms(500); // Fixed the space typo here from your original code
}

// --- Main Function ---
void main(void)
{
    // Initialization
    P1 = P1 & 0xF0; // Initially, all LEDs OFF (Clear lower 4 bits)
    prt26 = 1;      // Configure P2.6 as input (8051 requires writing 1 to use as input)
    
    while(1)
    {
        // Check button with debounce
        if(button_pressed())
        {
            state++;
            // Cycle through 4 modes. If state goes above 4, reset to Mode 1.
            if(state > 4) state = 1; 
        }
        
        // State Machine
        switch(state)
        {
            case 0: 
                P1 = (P1 & 0xF0); // Idle State: All LEDs OFF
                break;
            case 1: 
                state_running();      break;
            case 2: 
                state_counter();      break;
            case 3: 
                state_alternating();  break;
            case 4: 
                state_all_flash();    break;
            default: 
                state = 0;            break;
        }
    }
}
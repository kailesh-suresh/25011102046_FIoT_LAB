#include <reg51.h>

unsigned char timer_count = 0; 
unsigned char step_interval = 1;
unsigned char coil = 0x08; // Initial coil state for stepper motor

int keypad_scan(void)
{
    static const int keypad[4][4] =
    {
        {1, 2, 3, -1},
        {-1, -1, -1, -1},
        {-1, -1, -1, -1},
        {-1, -1, -1, -1}
    };

    static const unsigned char rowPin[4] = {7, 6, 5, 4};
    static const unsigned char colPin[4] = {3, 2, 1, 0};

    int row, col;


    for(row = 0; row < 4; row++)
    {
        // Set all rows HIGH
        P2 |= 0xF0;

        // Set current row LOW
        P2 &= ~(1 << rowPin[row]);

        // Check columns
        for(col = 0; col < 4; col++)
        {
            if((P2 & (1 << colPin[col])) == 0)
            {
                return keypad[row][col];
            }
        }
    }

    return -1;
}

void step_motor(void)
{
    if (coil == 0x01)
    {
        coil = 0x08;
    } else {
        coil = coil >> 1;
    }

    P1 = coil;
}

void set_speed(int key)
{
    switch(key)
    {
        case 1: step_interval = 1; break;
        case 2: step_interval = 5; break;
        case 3: step_interval = 10; break;
    }
}

void timer0_ISR(void) interrupt 1
{
    TH0 = 0xDC;
    TL0 = 0x00;

    timer_count++;
    if(timer_count >= step_interval) {
        timer_count = 0;
        step_motor(); // Call the step motor function
    }
}

void timer0_init(void)
{
    TMOD = 0x01;      // Timer 0, Mode 1

    TH0 = 0xDC;
    TL0 = 0x00;

    ET0 = 1;          // Enable Timer 0 interrupt
    EA  = 1;           // Enable global interrupts
    TR0 = 1;           // Start Timer 0
}

void main() {

    timer0_init();
    P1 = coil; // Initialize P1 with the initial coil state
    while(1) {
        int key = keypad_scan();
        if(key != -1) {
            set_speed(key);
        }
    }
}
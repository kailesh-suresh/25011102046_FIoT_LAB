
#include <reg51.h>

/* Function Prototypes */
void delay_ms(unsigned int ms);
void keypad_scan(void);
void display_digit(unsigned char value);
void led_control(unsigned char key);
void running_led(void);
void binary_counter(void);
void alternate_led(void);
void blink_all(void);
void reset(void);

unsigned char key_pressed = 0xFF; /* 0xFF = no key pressed */
unsigned char last_key = 0xFF;    /* Edge detector state variable */


code unsigned char seg_table[10] = {
    0x3F, /* 0 */
    0x06, /* 1 */
    0x5B, /* 2 */
    0x4F, /* 3 */
    0x66, /* 4 */
    0x6D, /* 5 */
    0x7D, /* 6 */
    0x07, /* 7 */
    0x7F, /* 8 */
    0x6F  /* 9 */
};


void main(void)
{
    P0 = 0x00;
    P1 = 0x00;
    P2 = 0xFF;
    key_pressed = 0xFF;
    last_key    = 0xFF;

    while (1)
    {
      
        if (key_pressed == 0xFF || key_pressed == last_key)
        {
            keypad_scan();
        }

    
        if (key_pressed != 0xFF && key_pressed != last_key)
        {
            last_key = key_pressed;
            display_digit(key_pressed);
            led_control(key_pressed);
        }

     
        if (key_pressed == 0xFF)
        {
            last_key = 0xFF;
        }
    }
}

void keypad_scan(void)
{
    key_pressed = 0xFF; /* Default to no key detected */


    P2 = 0x7F; // 0111 1111 (Drive Row 1 LOW)
    if      ((P2 & 0x08) == 0) key_pressed = 1; // P2.3 (Col 1)
    else if ((P2 & 0x04) == 0) key_pressed = 2; // P2.2 (Col 2)
    else if ((P2 & 0x02) == 0) key_pressed = 3; // P2.1 (Col 3)

    if (key_pressed != 0xFF) { P2 = 0xFF; return; }

    P2 = 0xBF; // 1011 1111 (Drive Row 2 LOW)
    if      ((P2 & 0x08) == 0) key_pressed = 4; // P2.3 (Col 1)
    else if ((P2 & 0x04) == 0) key_pressed = 5; // P2.2 (Col 2)
    else if ((P2 & 0x02) == 0) key_pressed = 6; // P2.1 (Col 3)

    if (key_pressed != 0xFF) { P2 = 0xFF; return; }


    P2 = 0xDF; // 1101 1111 (Drive Row 3 LOW)
    if      ((P2 & 0x08) == 0) key_pressed = 7; // P2.3 (Col 1)
    else if ((P2 & 0x04) == 0) key_pressed = 8; // P2.2 (Col 2)
    else if ((P2 & 0x02) == 0) key_pressed = 9; // P2.1 (Col 3)

    if (key_pressed != 0xFF) { P2 = 0xFF; return; }

    P2 = 0xEF; // 1110 1111 (Drive Row 4 LOW)
    if ((P2 & 0x04) == 0) key_pressed = 0;      // P2.2 (Col 2)

    P2 = 0xFF;
}

void display_digit(unsigned char value)
{
    if (value <= 9)
        P0 = seg_table[value];
    else
        P0 = 0x00; /* Blank */
}


void led_control(unsigned char key)
{
    switch (key)
    {
        case 1: P1 = 0x02; break; /* P1.1 ON */
        case 2: P1 = 0x04; break; /* P1.2 ON */
        case 3: P1 = 0x08; break; /* P1.3 ON */
        case 4: P1 = 0x10; break; /* P1.4 ON */

        case 5: running_led(); break;
        case 6: alternate_led(); break;
        case 7: binary_counter(); break;
        case 8: blink_all(); break;

        case 9: P1 = 0x00; break; /* All LEDs OFF */
        case 0: reset(); break;   /* Reset System */

        default: break;
    }
}


void running_led(void)
{
    unsigned char i, cycle;
    unsigned char pattern[6] = {0x02, 0x04, 0x08, 0x10, 0x08, 0x04};

    for (cycle = 0; cycle < 3; cycle++)
    {
        for (i = 0; i < 6; i++)
        {
            P1 = pattern[i];
            delay_ms(200);

            /* Interrupt check: Abort if a NEW key is hit */
            keypad_scan();
            if (key_pressed != 0xFF && key_pressed != 5) return;
        }
    }
}
void alternate_led(void)
{
    unsigned char i;
    for (i = 0; i < 10; i++)
    {
        P1 = 0x14; /* 0001 0100 : P1.2 & P1.4 ON */
        delay_ms(250);

        keypad_scan();
        if (key_pressed != 0xFF && key_pressed != 6) return;

        P1 = 0x0A; /* 0000 1010 : P1.1 & P1.3 ON */
        delay_ms(250);

        keypad_scan();
        if (key_pressed != 0xFF && key_pressed != 6) return;
    }
}

void binary_counter(void)
{
    unsigned char i;
    for (i = 0; i <= 15; i++)
    {
        P1 = (i << 1); /* Align binary bits to P1.1 - P1.4 */
        delay_ms(400);

      
        keypad_scan();
        if (key_pressed != 0xFF && key_pressed != 7)
        {
            return; 
        }
    }
    delay_ms(500);
    P1 = 0x00;
}

void blink_all(void)
{
    unsigned char i;
    for (i = 0; i < 10; i++)
    {
        P1 = 0x1E; 
        delay_ms(500);

        keypad_scan();
        if (key_pressed != 0xFF && key_pressed != 8) return;

        P1 = 0x00; /* All OFF */
        delay_ms(500);

        keypad_scan();
        if (key_pressed != 0xFF && key_pressed != 8) return;
    }
}

void reset(void)
{
    P1 = 0x00;          /* LEDs OFF */
    P0 = seg_table[0];  /* Display '0' */
    
}


void delay_ms(unsigned int ms)
{
    unsigned int i, j;
    for (i = 0; i < ms; i++)
        for (j = 0; j < 127; j++);
}
#include <reg52.h>

/*========================================================
  TASK 7A - LCD INITIALIZATION AND CHARACTER/STRING DISPLAY
  AMOTECH 8051 BOARD - AT89S52

  LCD RS  -> P3.2
  LCD E   -> P3.3
  LCD D4  -> P3.4
  LCD D5  -> P3.5
  LCD D6  -> P3.6
  LCD D7  -> P3.7
  LCD RW  -> GND

  LCD : 16x2 HD44780 (4-bit mode)
========================================================*/

sbit LCD_RS = P3^2;
sbit LCD_EN = P3^3;


/*========================================================
  DELAY FUNCTION
========================================================*/
void delay_ms(unsigned int ms)
{
    unsigned int i, j;

    for(i = 0; i < ms; i++)
    {
        for(j = 0; j < 123; j++);
    }
}


/*========================================================
  SEND ONE 4-BIT NIBBLE TO LCD (D4-D7 = P3.4-P3.7)
========================================================*/
void lcd_send_nibble(unsigned char nibble)
{
    /* Keep P3.0 - P3.3 unchanged, put nibble on P3.4 - P3.7 */
    P3 = (P3 & 0x0F) | (nibble & 0xF0);

    /* Pulse Enable: LCD latches data on falling edge of E */
    LCD_EN = 1;
    delay_ms(1);
    LCD_EN = 0;
    delay_ms(1);
}


/*========================================================
  SEND ONE COMPLETE BYTE (4-bit mode = two nibbles)
========================================================*/
void lcd_send_byte(unsigned char value)
{
    lcd_send_nibble(value & 0xF0);          // Higher 4 bits first
    lcd_send_nibble((value << 4) & 0xF0);   // Lower 4 bits next
}


/*========================================================
  SEND COMMAND TO LCD (RS = 0)
========================================================*/
void lcd_cmd(unsigned char command)
{
    LCD_RS = 0;
    lcd_send_byte(command);
    delay_ms(2);
}


/*========================================================
  SEND DATA / CHARACTER TO LCD (RS = 1)
========================================================*/
void lcd_data(unsigned char dataa)
{
    LCD_RS = 1;
    lcd_send_byte(dataa);   // BOTH nibbles must be sent
    delay_ms(2);
}


/*========================================================
  LCD INITIALIZATION (derived from HD44780 datasheet)
========================================================*/
void lcd_init(void)
{
    delay_ms(20);           // Wait for LCD power-up (>15 ms)
    LCD_RS = 0;
    LCD_EN = 0;

    /* Wake-up sequence: force LCD into a known state */
    lcd_send_nibble(0x30); delay_ms(5);
    lcd_send_nibble(0x30); delay_ms(1);
    lcd_send_nibble(0x30); delay_ms(1);

    /* Switch LCD to 4-bit mode */
    lcd_send_nibble(0x20); delay_ms(1);

    lcd_cmd(0x28);   // Function Set : 4-bit, 2 lines, 5x8 font
    lcd_cmd(0x0F);   // Display ON, Cursor ON, Blink ON
    lcd_cmd(0x06);   // Entry Mode   : cursor moves right, no display shift
    lcd_cmd(0x01);   // Clear Display, cursor home
    delay_ms(2);
}


/*========================================================
  MAIN - TASK 7A
  Row 1 : single characters      -> A T 8 9 S 5 2
  Row 2 : a string (char array)  -> HELLO IoT
========================================================*/
void main(void)
{
    unsigned char text[] = "HELLO IoT";   // String for row 2
    unsigned char i;

    lcd_init();

    /* ---- Individual characters on row 1 (starts at 0x80) ---- */
    lcd_data('A');
    lcd_data('T');
    lcd_data('8');
    lcd_data('9');
    lcd_data('S');
    lcd_data('5');
    lcd_data('2');

    /* ---- Move cursor to start of row 2 (DDRAM 0x40 -> 0xC0) ---- */
    lcd_cmd(0xC0);

    /* ---- String on row 2 ---- */
    for(i = 0; text[i] != '\0'; i++)
    {
        lcd_data(text[i]);
    }

    while(1)
    {
        /* Display stays as is */
    }
}

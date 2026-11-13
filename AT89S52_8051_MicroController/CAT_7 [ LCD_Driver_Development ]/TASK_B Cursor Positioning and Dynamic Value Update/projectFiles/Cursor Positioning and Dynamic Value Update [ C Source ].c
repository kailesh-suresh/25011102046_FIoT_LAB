#include <reg52.h>

/*========================================================
  TASK 7B - CURSOR POSITIONING AND DYNAMIC VALUE UPDATE
  AMOTECH 8051 BOARD - AT89S52

  LCD RS  -> P3.2
  LCD E   -> P3.3
  LCD D4  -> P3.4
  LCD D5  -> P3.5
  LCD D6  -> P3.6
  LCD D7  -> P3.7
  LCD RW  -> GND
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
  SEND ONE 4-BIT NIBBLE TO LCD
========================================================*/
void lcd_send_nibble(unsigned char nibble)
{
    P3 = (P3 & 0x0F) | (nibble & 0xF0);

    LCD_EN = 1;
    delay_ms(1);
    LCD_EN = 0;
    delay_ms(1);
}


/*========================================================
  SEND ONE COMPLETE BYTE (4-bit mode)
========================================================*/
void lcd_send_byte(unsigned char value)
{
    lcd_send_nibble(value & 0xF0);          // Higher 4 bits
    lcd_send_nibble((value << 4) & 0xF0);   // Lower 4 bits
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
    lcd_send_byte(dataa);
    delay_ms(2);
}


/*========================================================
  LCD INITIALIZATION
========================================================*/
void lcd_init(void)
{
    delay_ms(20);
    LCD_RS = 0;
    LCD_EN = 0;

    lcd_send_nibble(0x30); delay_ms(5);
    lcd_send_nibble(0x30); delay_ms(1);
    lcd_send_nibble(0x30); delay_ms(1);
    lcd_send_nibble(0x20); delay_ms(1);

    lcd_cmd(0x28);   // 4-bit, 2 lines, 5x8 font
    lcd_cmd(0x0F);   // Display ON, Cursor ON, Blink ON
    lcd_cmd(0x06);   // Cursor moves right, no display shift
    lcd_cmd(0x01);   // Clear Display
    delay_ms(2);
}


/*========================================================
  TASK 7B FUNCTIONS
========================================================*/

/* lcd_clear: Clear whole display and return cursor home */
void lcd_clear(void)
{
    lcd_cmd(0x01);
    delay_ms(2);
}

/* 1. lcd_goto: Move cursor to row (0 or 1) and column (0 to 15) */
void lcd_goto(unsigned char row, unsigned char column)
{
    unsigned char address;

    if(row == 0)
    {
        address = 0x00 + column;   // Row 0 starts at DDRAM 0x00
    }
    else
    {
        address = 0x40 + column;   // Row 1 starts at DDRAM 0x40
    }

    /* "Set DDRAM Address" command = 0x80 + address */
    lcd_cmd(0x80 | address);
}

/* 2. lcd_puts: Print a whole string until the '\0' terminator */
void lcd_puts(char *str)
{
    while(*str != '\0')
    {
        lcd_data(*str);
        str++;
    }
}

/* 3. lcd_print_number: Print an integer (0 to 65535), no libraries */
void lcd_print_number(unsigned int value)
{
    unsigned char digits[6];   // 5 digits (65535) + 1 safety
    unsigned char i = 0;

    /* Special case: value is exactly 0 */
    if(value == 0)
    {
        lcd_data('0');
        return;
    }

    /* Step A: extract digits right to left using % and / */
    while(value > 0)
    {
        digits[i] = (value % 10) + '0';   // last digit -> ASCII
        value = value / 10;               // chop off last digit
        i++;
    }

    /* Step B: print digits left to right (reverse order) */
    while(i > 0)
    {
        i--;
        lcd_data(digits[i]);
    }
}


/*========================================================
  MAIN - TASK 7B
  Row 0 : "Task 7B Ready!"
  Row 1 : "Count: " followed by a live counter
========================================================*/
void main(void)
{
    unsigned int counter = 0;

    lcd_init();
    lcd_clear();

    lcd_goto(0, 0);
    lcd_puts("Task 7B Ready!");

    lcd_goto(1, 0);
    lcd_puts("Count: ");

    while(1)
    {
        /* Erase old digits (count may shrink after 65535 -> 0) */
        lcd_goto(1, 7);
        lcd_puts("     ");

        /* Print new value right after "Count: " */
        lcd_goto(1, 7);
        lcd_print_number(counter);

        counter++;
        delay_ms(500);
    }
}

#include <reg52.h>

/*========================================================
  TASK 7C - 4x4 KEYPAD BASED CHARACTER MOVEMENT
  AMOTECH 8051 BOARD - AT89S52

  LCD RS  -> P3.2
  LCD E   -> P3.3
  LCD D4  -> P3.4
  LCD D5  -> P3.5
  LCD D6  -> P3.6
  LCD D7  -> P3.7
  LCD RW  -> GND

  Keypad Rows    : R1-R4 = P2.7 - P2.4  (outputs)
  Keypad Columns : C1-C4 = P2.3 - P2.0  (inputs)

  Key 2 -> UP      Key 8 -> DOWN
  Key 4 -> LEFT    Key 6 -> RIGHT
========================================================*/

sbit R1 = P2^7;
sbit R2 = P2^6;
sbit R3 = P2^5;
sbit R4 = P2^4;

sbit C1 = P2^3;
sbit C2 = P2^2;
sbit C3 = P2^1;
sbit C4 = P2^0;

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
    lcd_send_byte(dataa);   // character needs BOTH nibbles
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
  LCD GOTO : row (0 or 1), column (0 to 15)
========================================================*/
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

    lcd_cmd(0x80 | address);       // Set DDRAM Address
}


/*========================================================
  KEYPAD SCAN
  Pull one row LOW, then check which column goes LOW.
  Returns '2', '4', '6', '8' or 0 if nothing pressed.

  Key 2 -> R1 + C2
  Key 4 -> R2 + C1
  Key 6 -> R2 + C3
  Key 8 -> R3 + C2
========================================================*/
char keypad_scan(void)
{
    /* ---- Row 1 : key 2 ---- */
    R1 = 0;
    if(C2 == 0)
    {
        R1 = 1;
        return '2';
    }
    R1 = 1;


    /* ---- Row 2 : keys 4 and 6 ---- */
    R2 = 0;
    if(C1 == 0)
    {
        R2 = 1;
        return '4';
    }
    if(C3 == 0)
    {
        R2 = 1;
        return '6';
    }
    R2 = 1;


    /* ---- Row 3 : key 8 ---- */
    R3 = 0;
    if(C2 == 0)
    {
        R3 = 1;
        return '8';
    }
    R3 = 1;


    return 0;   // No key pressed
}


/*========================================================
  MAIN - TASK 7C
  A '*' starts at row 0, column 0 and moves with the keypad.
  Movement is limited to the 16x2 display area.
========================================================*/
void main(void)
{
    unsigned char row = 0;
    unsigned char column = 0;

    unsigned char old_row = 0;
    unsigned char old_column = 0;

    char key;

    lcd_init();
    lcd_goto(row, column);
    lcd_data('*');

    while(1)
    {
        /* Get key from keypad */
        key = keypad_scan();

        /* ---------- KEY 2 : UP ---------- */
        if(key == '2')
        {
            old_row = row;
            old_column = column;

            if(row > 0)
            {
                row--;
            }
        }

        /* ---------- KEY 8 : DOWN ---------- */
        else if(key == '8')
        {
            old_row = row;
            old_column = column;

            if(row < 1)
            {
                row++;
            }
        }

        /* ---------- KEY 4 : LEFT ---------- */
        else if(key == '4')
        {
            old_row = row;
            old_column = column;

            if(column > 0)
            {
                column--;
            }
        }

        /* ---------- KEY 6 : RIGHT ---------- */
        else if(key == '6')
        {
            old_row = row;
            old_column = column;

            if(column < 15)
            {
                column++;
            }
        }

        /* If a valid movement key was pressed, update the LCD */
        if(key == '2' || key == '8' ||
           key == '4' || key == '6')
        {
            /* Erase old character */
            lcd_goto(old_row, old_column);
            lcd_data(' ');

            /* Draw character at new position */
            lcd_goto(row, column);
            lcd_data('*');

            /* Key debounce delay */
            delay_ms(150);
        }
    }
}

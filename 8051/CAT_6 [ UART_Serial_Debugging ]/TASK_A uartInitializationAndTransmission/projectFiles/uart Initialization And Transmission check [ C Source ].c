#include <reg52.h>

/*========================================================
  MESSAGE TO BE TRANSMITTED
========================================================*/
char message[] = "Foundations of IoT - Dr.Dinesh G";

unsigned char tx_index = 0;
bit tx_done = 0;


/*========================================================
  UART INITIALIZATION
========================================================*/
void uart_init(void)
{
    /* UART Mode 1
       8-bit UART
       Receiver enabled  
    */
    SCON = 0x50;

    /* Timer 1 Mode 2
       8-bit auto-reload
    */
    TMOD &= 0x0F;
    TMOD |= 0x20;   

    /* 9600 baud
       11.0592 MHz crystal
    */
    TH1 = 0xFD;
    TL1 = 0xFD;

    /* Start Timer 1 */
    TR1 = 1;

    /* Clear transmit flag */
    TI = 0;

    /* Enable interrupts */
    EA = 1;      /* Globavl interrupt enable */
    ES = 1;      /* Serial interrupt enable */
}


/*========================================================
  SERIAL INTERRUPT SERVICE ROUTINE
========================================================*/
void serial_ISR(void) interrupt 4
{
    if (TI == 1)
    {
        /* Transmission completed */
        TI = 0;

        /* Send next character */
        if (message[tx_index] != '\0')
        {
            SBUF = message[tx_index];
            tx_index++;
        }
        else
        {
            /* Entire message transmitted */
            tx_done = 1;
        }
    }
}


/*========================================================
  MAIN PROGRAM
========================================================*/
void main(void)
{
    /* Initialize UART */
    uart_init();

    /* Start transmission with first character */
    SBUF = message[tx_index];
    tx_index++;

    /* CPU is free while UART transmits */
    while (1)
    {
        /* Other work can be done here */
    }
}
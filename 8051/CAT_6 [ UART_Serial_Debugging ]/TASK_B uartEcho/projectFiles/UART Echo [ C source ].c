#include <reg52.h>

/*========================================================
  MESSAGE TO BE TRANSMITTED
========================================================*/
char message[] = "F";

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
    TH1 = 0xFA;
    TL1 = 0xFA;

    /* Start Timer 1 */
    TR1 = 1;

    /* Clear serial flags */
    TI = 0;
    RI = 0;

    /* Enable interrupts */
    EA = 1;      /* Global interrupt enable */
    ES = 1;      /* Serial interrupt enable */
}


/*========================================================
  SERIAL INTERRUPT SERVICE ROUTINE
========================================================*/
void serial_ISR(void) interrupt 4
{
    unsigned char received_data;

    /*====================================================
      TRANSMIT SECTION
    ====================================================*/
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


    /*====================================================
      RECEIVE + ECHO SECTION
    ====================================================*/
    if (RI == 1)
    {
        /* Read received character */
        received_data = SBUF;

        /* Clear receive flag */
        RI = 0;

        /* Transmit the received character back */
        SBUF = received_data;
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

    /* CPU is free while UART handles communication */
    while (1)
    {
        /* Other work can be done here */
    }
}
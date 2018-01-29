/*
 * libtrivdl example: stream, msp430 version.
 *
 * Copyright 2018 Alexander Kulak.
 * This file is licensed under the MIT license.
 * See the LICENSE file in the project root for more information.
 */
  
#define MCU  // important

#include <msp430g2553.h>
#include "libtrivdl.h"
#include "stream.h"

// launchpad LEDs
#define RED         BIT0
#define GREEN       BIT6
#define LED_DIR     P1DIR
#define LED_OUT     P1OUT

// uart
#define TXD         BIT2
#define RXD         BIT1


t_line volatile linei;
t_line* line;

#ifdef DEBUG
// define these callbacks or undef DEBUG
void mcudebug1() {
    LED_OUT |= GREEN;
    __delay_cycles(100);
    LED_OUT &= ~GREEN;
}
void mcudebug2() {
    LED_OUT |= RED;
    __delay_cycles(100);
    LED_OUT &= ~RED;
}
#endif

void create_and_send_data ()
{
    uc pl[MAXFRAMESIZE]; // payload
    uc m;
    pl[0] = OP_STREAM_DATA;
    for (m = 1; m < FSIZE-OVERHEAD; m++)
        pl[m] = m + TXTOTAL + RXTOTAL; // pseudo rng
    build_frame (LWFR, pl, FSIZE-OVERHEAD);
    LWNEXT = 0;
    LWFLAGS |= READY; // async machine starts transmitting
}

void create_and_send_stats ()
{
    t_ctr ctr;
    ctr.c[1] = OP_STREAM_STATS;
    ctr.i[1] = TXTOTAL;
    ctr.i[2] = RXTOTAL;
    ctr.i[3] = TXERRORS;
    ctr.i[4] = RXERRORS;
    build_frame (LWFR, ctr.c+1, 9);
    TXTOTAL = RXTOTAL = TXERRORS = RXERRORS = 0;
    LWNEXT = 0;
    LWFLAGS |= READY;
}

// callback for rx
void cb_frame_rx_done (uc status)
{
    DEBUG1
    if (LRMSG == OP_STREAM_DATA || LRMSG == 0x10) {
        RXTOTAL++;
        if (status != FROK) {
            RXERRORS++;
            goto rxcont; // skip bad frames
        }
    }
    // remote commands
    switch (LRMSG) {
        case OP_STREAM_START:
            FSIZE = LRDATA[MESSAGE+1];
            STATE = ST_start;
            break;
        case OP_STREAM_STOP:
            if (STATE == ST_async_data || STATE == ST_start) {
                if (!(LWFLAGS & READY)) // txmachine transmits nothing
                    STATE = ST_stopped;
                else // wait for frame tx complete
                    STATE = ST_stop;
            }
            break;
        case OP_STREAM_DATA:
        case 0x10:
            if (STATE == ST_start)
                STATE = ST_async_data;  // seen
            if (STATE == ST_silence) {
                STATE = ST_start; // hack for lost START frame
            }
            break;
    }
    if (STATE == ST_start) {
        // the only case when rx triggers tx, hopefully once
        create_and_send_data (); // first frame in series
    }
rxcont:
    // continue rx new frame no matter what
    LRNEXT = 0;
    LRFLAGS &= ~READY;
}

void cb_frame_tx_done (uc status)
{
    DEBUG2
    if (LWMSG == OP_STREAM_DATA) {
        TXTOTAL++;
        if (status!=FROK)
            TXERRORS++;
    }
    switch (STATE) {
        case ST_start:
        case ST_async_data:
            create_and_send_data (); // continuously send new frames
            break;
        case ST_stop:
            // some frame tx finished
            STATE = ST_stopped;
            create_and_send_stats();
            break; // important
        case ST_stopped:
            STATE = ST_silence;
            break;
    }
}

// rx ISR
static void __attribute__((__interrupt__(USCIAB0RX_VECTOR))) isr_USCI0RX (void)
{
    //UC0IE &= ~UCA0RXIE; // disable reentry
    if (!(LRFLAGS & READY)) {
        incoming_char (UCA0RXBUF);
        //IFG2 &= ~UCA0RXIFG; // Clear RX flag // causes hang sometimes
    }
    //UC0IE |= UCA0RXIE; // reenable
}

// tx ISR
static void __attribute__((__interrupt__(USCIAB0TX_VECTOR))) isr_USCI0TX (void)
{
    //UC0IE &= ~UCA0TXIE; // disable reentry  TODO: disable machinery, not interrupt
    if (LWFLAGS & READY) {
        UCA0TXBUF = outgoing_char(); // TX next character, generally WDATA[WNEXT++]
        if (LWNEXT > LWLAST) { // TX over?
            LWFLAGS &= ~READY;
            X_DONE(cb_frame_tx_done, FROK);
        }
    }
    //UC0IE |= UCA0TXIE; // reenable
}

void smclk_1mhz()
{
    // calibrate and set the internal oscillator at 1 MHz
    DCOCTL = 0; // Select lowest DCOx and MODx settings<
    BCSCTL1 = CALBC1_1MHZ; // Set DCO
    DCOCTL = CALDCO_1MHZ;
}

/*
void uart_115200_smclk_1mhz()
{
    smclk_1mhz();
    // switch USCI to reset state before configuring
    UCA0CTL1 |= UCSWRST;
    // select the SMCLK as the clock source for the UART module
    UCA0CTL1 |= UCSSEL_2; // SMCLK
    // configure SMCLK (see table in user guide)
    UCA0BR0 = 0x08; // 1MHz 115200
    UCA0BR1 = 0x00; // 1MHz 115200
    UCA0MCTL = UCBRS2 + UCBRS0; // Modulation UCBRSx = 5
    UCA0CTL1 &= ~UCSWRST; // **Initialize USCI state machine**
}
*/
void uart_9600_smclk_1mhz()
{
    smclk_1mhz();
    UCA0CTL1 |=  UCSWRST;             // USCI_A0 reset mode for configuration
    __nop();
    UCA0CTL1 |=  UCSSEL_2;            // USCI Clock = SMCLK
    UCA0BR0   =  104;                 // 104 From datasheet table-  
    UCA0BR1   =  0;                   // -selects baudrate =9600,clk = SMCLK
    UCA0MCTL  =  UCBRS_1;             // Modulation value = 1 from datasheet
    UCA0CTL1 &= ~UCSWRST;             // Clear UCSWRST to enable USCI_A0
}

void setup_ports()
{
    // leds off
    LED_DIR |= RED + GREEN;                          
    LED_OUT &= ~(RED + GREEN);  
    // disable PORT2 completely
    P2DIR = 0xFF;
    P2OUT &= 0x00;
    // P1.1 and P1.2 switched to UART mode for the G2553
    P1SEL |= RXD + TXD ; // P1.1 = RXD, P1.2=TXD
    P1SEL2 |= RXD + TXD ; // P1.1 = RXD, P1.2=TXD
}

int main (void) {
    volatile t_userdata ud;

    WDTCTL = WDTPW + WDTHOLD; // Stop watchdog
    setup_ports ();
    uart_9600_smclk_1mhz ();
    ud.tx = ud.rx = ud.txerr = ud.rxerr = 0;
    ud.framesize = 4; // minimal value for the case when master command lost
    ud.state = ST_silence;
    line = (t_line*)(&linei);
    init_line (line, NULL, (t_userdata*)(&ud));
    LRNEXT = 0;
    LRFLAGS &= ~READY;
    UC0IE |= UCA0RXIE; // start rx: enable USCI_A0 RX interrupt
    UC0IE |= UCA0TXIE; // start tx too: enable USCI_A0 TX interrupt (doesn't really start till READY)
    for (;;) {
        __bis_SR_register (CPUOFF + GIE);
    }
}


/*
 * libtrivdl example: echo, msp430 version.
 *
 * Copyright 2018 Alexander Kulak.
 * This file is licensed under the MIT license.
 * See the LICENSE file in the project root for more information.
 */

#define MCU  // important

#include <msp430g2553.h>
#include "libtrivdl.h"

// launchpad LEDs
#define RED         BIT0
#define GREEN       BIT6
#define LED_DIR     P1DIR
#define LED_OUT     P1OUT

// uart
#define TXD         BIT2
#define RXD         BIT1

// first char of messages in this example
#define OP_ECHORQ   0x10
#define OP_ECHOREP  0x11


t_line volatile linei;
t_line* line;

#ifdef DEBUG
// define these callbacks or undef DEBUG
void mcudebug1() {
    LED_OUT |= GREEN;
    __delay_cycles(300);
    LED_OUT &= ~GREEN;
}
void mcudebug2() {
    LED_OUT |= RED;
    __delay_cycles(300);
    LED_OUT &= ~RED;
}
#endif

// callback for rx
void cb_frame_rx_done (uc status)
{
    DEBUG1  // signal "frame received"
    if (LWFLAGS & READY) {
        // we are already transmitting something
        return;
    }
    if (LRMSG == OP_ECHORQ) {
        // replace ECHORQ with ECHOREP in RX frame
        // this invalidates RX frame checksum, which we don't care
        LRMSG = OP_ECHOREP;
        // copy RX frame content to TX frame
        build_frame (LWFR, &(LRMSG), LRLAST-2);
        // initiate transmission
        UCA0TXBUF = outgoing_char(); // generally LWDATA[LWNEXT++]
        UC0IE |= UCA0TXIE; // Enable USCI_A0 TX interrupt
        // logically enable transmission for TX ISR
        LWFLAGS |= READY;
    }
}

// rx ISR
static void __attribute__((__interrupt__(USCIAB0RX_VECTOR))) isr_USCI0RX (void)
{
    incoming_char (UCA0RXBUF);
    IFG2 &= ~UCA0RXIFG; // Clear RX flag
}

// tx ISR
static void __attribute__((__interrupt__(USCIAB0TX_VECTOR))) isr_USCI0TX (void)
{
    if (LWFLAGS & READY) {
        UCA0TXBUF = outgoing_char(); // TX next character, generally WDATA[WNEXT++]
        if (LWNEXT > LWLAST) { // TX over?
            UC0IE &= ~UCA0TXIE; // Disable USCI_A0 TX interrupt
            LWFLAGS &= ~READY;
            init_frame (&(line->wfr)); // cleanup for next frame rx
            DEBUG2  // signal "frame transmitted"
        }
    }
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
    WDTCTL = WDTPW + WDTHOLD; // Stop watchdog
    setup_ports ();
    uart_9600_smclk_1mhz ();
    line = (t_line*)(&linei);
    init_line (line, NULL, NULL);
    UC0IE |= UCA0RXIE; // start rx: enable USCI_A0 RX interrupt
    for (;;) {
        __bis_SR_register (CPUOFF + GIE);
    }
}


// Simple UART driver ("polled") 
// Martin Thomas 9/2007
// inspired by a Keil example

#include "LPC23xx.h"                     /* LPC23xx definitions               */
#include "target.h"

#include <string.h>
#include "comm.h"

#define UART1
#define BPS 		115200

// KEIL MCP2300
// UART0 TX P0.2 RX P0.3
#define U0_TX_PINSEL_REG  PINSEL0
#define U0_TX_PINSEL     (1UL<<4)  /* PINSEL0 Value for UART0 TX */
#define U0_TX_PINMASK    (3UL<<4)  /* PINSEL0 Mask  for UART0 RX */
#define U0_RX_PINSEL_REG  PINSEL0
#define U0_RX_PINSEL     (1UL<<6)  /* PINSEL0 Value for UART0 TX */
#define U0_RX_PINMASK    (3UL<<6)  /* PINSEL0 Mask  for UART0 RX */
// UART1 TX P0.15 RX P0.16
#define U1_TX_PINSEL_REG  PINSEL0
#define U1_TX_PINSEL     (1UL<<30)  /* PINSEL0 Value for UART0 TX */
#define U1_TX_PINMASK    (3UL<<30)  /* PINSEL0 Mask  for UART0 RX */
#define U1_RX_PINSEL_REG  PINSEL1
#define U1_RX_PINSEL     (1UL<<0)  /* PINSEL0 Value for UART0 TX */
#define U1_RX_PINMASK    (3UL<<0)  /* PINSEL0 Mask  for UART0 RX */

#if defined( UART0 )
	#define UxFDR   U0FDR
	#define UxLCR   U0LCR
	#define UxDLL   U0DLL
	#define UxDLM   U0DLM
	#define UxLSR   U0LSR
	#define UxTHR   U0THR
	#define UxRBR   U0RBR
#elif defined(UART1)
	#define UxFDR   U1FDR
	#define UxLCR   U1LCR
	#define UxDLL   U1DLL
	#define UxDLM   U1DLM
	#define UxLSR   U1LSR
	#define UxTHR   U1THR
	#define UxRBR   U1RBR
#else
#error "UART undefined - define UART0 or UART1"
#endif

/* returns 1 if char avail */
int uart_test(void)
{
	return ( UxLSR & 0x01 ) ? 1 : 0;
}

BYTE uart_get(void)
{
	while ( !uart_test() ) { ; }
	
	return UxRBR;
}

static void uart_xmit( BYTE d )
{
	while ( !( UxLSR & 0x20 ) ) {
		// wait for send possible
		; 
	}
	UxTHR = d;
}

// RealView syscall for libc printf
// (used during tests until eval-version-limit
//  forced early migration to GNU-tools)
int sendchar( int ch );
int sendchar( int ch )
{
	if ( ch == '\n' ) {
		uart_xmit( '\r' );
	}
	uart_xmit( ch );
	return 1;
}

void uart_put (BYTE d)
{
	sendchar( d );
}

void uart_init (void)
{
#if defined(UART0)
	U0_TX_PINSEL_REG = ( U0_TX_PINSEL_REG & ~U0_TX_PINMASK ) | U0_TX_PINSEL;
	U0_RX_PINSEL_REG = ( U0_RX_PINSEL_REG & ~U0_RX_PINMASK ) | U0_RX_PINSEL;
#elif defined(UART1)
	U1_TX_PINSEL_REG = ( U1_TX_PINSEL_REG & ~U1_TX_PINMASK ) | U1_TX_PINSEL;
	U1_RX_PINSEL_REG = ( U1_RX_PINSEL_REG & ~U1_RX_PINMASK ) | U1_RX_PINSEL;
#endif
	UxFDR = 0;    // no extended uart
	UxLCR = 0x83; // 8,n,1
	UxDLL = ( Fpclk / 16 ) / BPS; // divider
	UxDLM = 0;
	UxLCR = 0x03;
}

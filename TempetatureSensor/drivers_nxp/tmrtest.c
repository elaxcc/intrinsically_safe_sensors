/*****************************************************************************
 *   tmrtest.c:  main C entry file for NXP LPC23xx/24xx Family Microprocessors
 *
 *   Copyright(C) 2006, NXP Semiconductor
 *   All rights reserved.
 *
 *   History
 *   2006.09.01  ver 1.00    Prelimnary version, first Release
 *
******************************************************************************/
#include "LPC23xx.H"                        /* LPC23xx/24xx definitions */
#include "type.h"
#include "target.h"
#include "irq.h"
#include "timer.h"

extern DWORD timer0_counter;

/*****************************************************************************
**   Main Function  main()
*****************************************************************************/
int main (void)
{	    		
  DWORD counter = 0;

  /************ The main Function is an endless loop ************/
  /* The timer routine is tested on the Keil MCB214x board */
  FIO2DIR = 0x000000FF;		/* P1.16..23 defined as Outputs */
  FIO2CLR = 0x000000FF;		/* turn off all the LEDs */
    
  init_timer( 0, TIME_INTERVAL );
  enable_timer( 0 );

  while (1) 
  {					/* Loop forever */
	if ( timer0_counter >= (0x20 * counter) )
	{
	  FIO2SET = 1 << counter;
	  counter++;
	  if ( counter > 8 )
	  {
		counter = 0;	
		timer0_counter = 0;
		FIO2CLR = 0x000000FF;
	  }
	}
  }
  return 0;
}

/*****************************************************************************
**                            End Of File
******************************************************************************/

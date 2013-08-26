#include "WD.h"

#include "LPC23xx.H"

void WatchDogReset(void)
{
	WDFEED = 0xAA;
	WDFEED = 0x55;
}

void WatchDogInit(void)
{
	WDTC = 0x004C4B40; // timeout interval is 5 seconds
	WDMOD = 3; // reset mode
	WatchDogReset();
}

#include "YDelay.h"

void YDelay(uint32_t ms)
{
	uint32_t ms_delay = ms * 1000;
	while (ms_delay)
	{
		__asm
		{
			nop
		}
		ms_delay--;
	}
}

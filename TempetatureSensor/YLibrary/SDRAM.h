#ifndef __SDRAM_H_
#define __SDRAM_H_

#include <stdint.h>

#include "YBool.h"

#define SDRAM_CS0_BASE 0xA0000000
#define SDRAM_BASE_ADDR 0xA0000000

#define SDRAM_VAL(address) *(unsigned int *)((unsigned int )SDRAM_BASE_ADDR + address)

void SDRAMinit(void);
YBOOL SDRAMtest(void);

#endif /*__SDRAM_H_*/

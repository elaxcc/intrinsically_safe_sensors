#include "SDRAM.h"

#include "LPC24xx.h"

#define EMC_PERIOD 20.8 // 48MHz 
#define P2C(Period) (((Period<EMC_PERIOD)?0:(unsigned int)((float)Period/EMC_PERIOD))+1)

#define SDRAM_TRP 20
#define SDRAM_TRAS 45
#define SDRAM_TXSR 1
#define SDRAM_TAPR 1
#define SDRAM_TDAL 2 + P2C(SDRAM_TRP)
#define SDRAM_TWR 2
#define SDRAM_TRC 66
#define SDRAM_TRFC 66
#define SDRAM_TRRD 15
#define SDRAM_TMRD 2
#define SDRAM_REFRESH 7813

void SDRAMinit(void)
{
	unsigned i;
	volatile unsigned short wtemp;
	
	//Using EMC_init() to initialize EMC. 
	//Step 1: configure EMC Pin function and Pin mode. 
	PINSEL5&=0xF0FCFCC0; 
	PINSEL5|=0x05010115; 
	PINMODE5&=0xF0FCFCC0; 
	PINMODE5|=0x0A02022A; 
	//p2.29(DQMOUT1),28(DQMOUT0),24(CKEOUT0),20(DYCS0),18(CLKOUT0) 
	//17(RAS),16(CAS) 
	// mode=10 (Pin has neither pull-up nor pull-down resistor enabled.) 
	PINSEL6 = 0x55555555; 
	PINMODE6 = 0xAAAAAAAA; 
	//p3.0-15=D0-15,mode=10 
	PINSEL8 &= 0xC0000000; 
	PINSEL8 |= 0x15555555; 
	PINMODE8&= 0xC0000000; 
	PINMODE8|= 0x2AAAAAAA; //p4.0-4.14=A0-14,mode=10 
	//Step 2: enable EMC and set EMC parameters. 
	PCONP|=0x800; //enable EMC power 
	EMC_CTRL=1; // enable EMC 
	EMC_DYN_RD_CFG=1;//Configures the dynamic memory read strategy(Command delayed strategy) 
	EMC_DYN_RASCAS0|=0x200;EMC_DYN_RASCAS0&=0xFFFFFEFF;//CAS latency=2 
	EMC_DYN_RASCAS0|=0x3; // RAS latency(active to read/write delay)=3 
	EMC_DYN_RP= P2C(SDRAM_TRP); 
	EMC_DYN_RAS = P2C(SDRAM_TRAS); 
	EMC_DYN_SREX = SDRAM_TXSR; 
	EMC_DYN_APR = SDRAM_TAPR; 
	EMC_DYN_DAL = SDRAM_TDAL ; 
	EMC_DYN_WR = SDRAM_TWR;
	EMC_DYN_RC = P2C(SDRAM_TRC); 
	EMC_DYN_RFC = P2C(SDRAM_TRFC); 
	EMC_DYN_XSR = SDRAM_TXSR; 
	EMC_DYN_RRD = P2C(SDRAM_TRRD); 
	EMC_DYN_MRD = SDRAM_TMRD; 
	EMC_DYN_CFG0 = 0x0000680; 
	// 16 bit external bus, 256 MB (16Mx16), 4 banks, row length = 13, column length = 9
	EMC_DYN_CTRL = 0x0183; // NOP 
	//Issue SDRAM NOP (no operation) command ; CLKOUT runs continuously; All clock 
	//enables are driven HIGH continuously 
	for(i = 200*30; i;i--); 
	EMC_DYN_CTRL|=0x100; EMC_DYN_CTRL&=0xFFFFFF7F; 
	// Issue SDRAM PALL (precharge all) command. 
	EMC_DYN_RFSH = 1; //Indicates 1X16 CCLKs between SDRAM refresh cycles. 
	for(i= 128; i; --i); // > 128 clk 
	EMC_DYN_RFSH = P2C(SDRAM_REFRESH) >> 4; 
	// Indicates SDRAM_REFRESH time between SDRAM refresh cycles. 
	EMC_DYN_CTRL|=0x80; EMC_DYN_CTRL&=0xFFFFFEFF; 
	//Issue SDRAM MODE command. 
	wtemp = *((volatile unsigned short *)(SDRAM_CS0_BASE | 0x00023000)); 
	/* 8 burst, 2 CAS latency */ 
	EMC_DYN_CTRL = 0x0000; //Issue SDRAM norm command ; 
	//CLKOUT stop; All clock enables low 
	EMC_DYN_CFG0|=0x80000; //Buffer enabled for accesses to DCS0 chip
}

YBOOL SDRAMtest(void)
{
	unsigned int i; 
	// 32 bits access 
	for (i = 0; i < 0x00000020; i+=sizeof(unsigned int)) 
	{
		SDRAM_VAL(i) = i; 
	}
	
	for (i = 0; i < 0x00000020; i+=sizeof(unsigned int )) 
	{
		if (SDRAM_VAL(i) != i) 
		{
			return YFALSE; 
		}
	}
	
	return YTRUE;
}

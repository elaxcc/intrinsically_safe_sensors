/*****************************************************************************
 *   mci.c:  SD/MMC module file for NXP LPC23xx/24xx Family Microprocessors
 *
 *   Copyright(C) 2006, NXP Semiconductor
 *   All rights reserved.
 *
 *   History
 *   YYYY.MM.DD
 *   2006.07.20  ver 1.00     Prelimnary version, first Release
 *   2007.09.13  ver n/a      Martin Thomas (not NXP) - see below
 *   2008.07	 ver n/a	  Lorenz Aebi (not NXP) - see below
 *
 *****************************************************************************
 Software that is described herein is for illustrative purposes only which 
 provides customers with programming information regarding the products.
 This software is supplied "AS IS" without any warranties. NXP Semiconductors 
 assumes no responsibility or liability for the use of the software, conveys no 
 license or title under any patent, copyright, or mask work right to the 
 product. NXP Semiconductors reserves the right to make changes in the 
 software without notification. NXP Semiconductors also make no representation 
 or warranty that such application will be suitable for the specified use 
 without further testing or modification.
 *****************************************************************************/

/*
 This code has been modified and extended by
 Martin Thomas, Kaiserslautern, Germany
 <mthomas(at)rhrk(dot)uni-kl(dot)de>
 http://www.siwawi.arubi.uni-kl.de/avr_projects
 - I'm not working for NXP -

 Modifications:
 --------------
 - volatiles of delay-loop counters
 - fixed retry count in Send_ACMD_OP_Cond (continue is evil ;-) )
 - reformat source (tab/space)
 - init MCI_CardType to unknown(when defined other then unknown
 a repeated card-init failed)
 - removed PINSEL kidnapping
 Please NXP developer: a little quality control
 will help a lot of people ot save a lot of time
 and will make the code more reuseable.
 - laebi: SDHC-Card Bug
 - laebi: config for EA_LPC2468 Board

 Extensions:
 -----------
 - added debug outputs (can be turned off)
 - Inserted license text in this file to avoid requests
 because of the NXP Copyright. I understand the license like:
 "Do whatever you want with the code but don't blame NXP if 
 it doesn't work as expected."
 And please don't blame me if my extensiones/modifications do
 not work for you. Send me an e-mail with your bug-fix.
 - SDHC support (my introduce new mistakes but
 should still work as expected for SDSC and MMC cards)
 Some information to do this came from Juri Haberland.
 - modified/extended card_init
 - write/read address
 case HC: do not multitply block-num with 512
 case SC: multiply block-num by 512
 - Extended select_card for unselect (RCA=0) to
 bring card in stby. Also needed for CMD4, 9, 10, 13 
 (see SanDisk manual V2.2 4-7)
 (I' not sure if the implementation is correct - but it
 seems to work.)
 - Added function for SD_STATUS (do not confuse with
 the status-register - it's something different)
 - laebi: Added Multiple Block Send Command (CMD25)
 */

// #define MCI_DEBUGOUT   /* undefine to disable the outputs */

#include "LPC23xx.h"   /* LPC23xx/24xx definitions */
#include "target.h"
#include "type.h"
#include "irq.h"
#include "mci.h"
#include <string.h>
#include "dma.h"

// mthomas - for debug outputs
#ifdef __GNUC__
#include "monitor.h"
#define myprintf xprintf
#else
#define myprintf printf
#endif

#ifdef MCI_DEBUGOUT
#include <stdio.h>
#define mci_debug_printf myprintf
#else
#define mci_debug_printf( fmt, args... )
#endif

volatile DWORD MCI_DataErrorProcess_count = 0;
volatile DWORD MCI_DATA_END_InterruptService_count = 0;
volatile DWORD MCI_FIFOInterruptService_count = 0;
volatile DWORD MCI_CmdProcess_count = 0;

volatile DWORD CmdCRCErrCount = 0;
volatile DWORD CmdTimeoutErrCount = 0;
volatile DWORD CmdRespEndCount = 0;
volatile DWORD CmdSentCount = 0;
volatile DWORD CmdActiveCount = 0;

volatile DWORD DataCRCErrCount = 0;
volatile DWORD DataTimeoutErrCount = 0;
volatile DWORD DataTxUnderrunErrCount = 0;
volatile DWORD DataRxOverrunErrCount = 0;
volatile DWORD DataStartbitErrCount = 0;

volatile DWORD DataEndCount = 0;
volatile DWORD DataBlockEndCount = 0;
volatile DWORD MCI_Block_End_Flag = 0;

volatile DWORD DataTxActiveCount = 0;
volatile DWORD DataRxActiveCount = 0;

volatile DWORD DataFIFOCount = 0;
volatile DWORD DataRxFIFOCount = 0;
volatile DWORD DataTxFIFOCount = 0;

volatile DWORD CardRCA;
volatile DWORD MCI_CardType;

/* Assembly modules using the ARM block transfer instruction in readfifo.s. */
// mthomas - volatile
extern DWORD MCI_ReadFifo(volatile DWORD * dest);
extern DWORD MCI_WriteFifo(volatile DWORD * src);

// mthomas
#ifdef __GNUC__
volatile BYTE *WriteBlock __attribute__ ((aligned (4))) = (volatile BYTE *)(DMA_SRC); /* treat WriteBlock as a constant address */
volatile BYTE *ReadBlock __attribute__ ((aligned (4))) = (volatile BYTE *)(DMA_DST); /* treat ReadBlock as a constant address */
#else
volatile BYTE *WriteBlock = (BYTE *)(DMA_SRC); /* treat WriteBlock as a constant address */
volatile BYTE *ReadBlock = (BYTE *)(DMA_DST); /* treat ReadBlock as a constant address */
#endif

volatile DWORD TXBlockCounter=0, RXBlockCounter=0;
// mthomas
volatile DWORD SDStatRead = 0;

/******************************************************************************
 ** Function name:		MCI_Interrupt related
 **
 ** Descriptions:		MCI interrupt handler and related APIs
 **
 **
 ** parameters:			None
 ** Returned value:		None
 ** 
 ******************************************************************************/
void MCI_TXEnable(void) {
#if MCI_DMA_ENABLED
	MCI_MASK0 |= ((DATA_END_INT_MASK)|(ERR_TX_INT_MASK));
	/* Enable TX interrupts only */
	MCI_MASK1 |= ((DATA_END_INT_MASK)|(ERR_TX_INT_MASK));
	/* Enable TX interrupts only */
#else
	MCI_MASK0 |= ((FIFO_TX_INT_MASK)|(DATA_END_INT_MASK)|(ERR_TX_INT_MASK)); /* FIFO TX interrupts only */
	MCI_MASK1 |= ((FIFO_TX_INT_MASK)|(DATA_END_INT_MASK)|(ERR_TX_INT_MASK)); /* FIFO TX interrupts only */
#endif
	return;
}
/*****************************************************************/

void MCI_TXDisable(void) {
#if MCI_DMA_ENABLED
	MCI_MASK0 &= ~((DATA_END_INT_MASK)|(ERR_TX_INT_MASK));
	/* Enable TX interrupts only */
	MCI_MASK1 &= ~((DATA_END_INT_MASK)|(ERR_TX_INT_MASK));
	/* Enable TX interrupts only */
#else
	MCI_MASK0 &= ~((FIFO_TX_INT_MASK)|(DATA_END_INT_MASK)|(ERR_TX_INT_MASK)); /* FIFO TX interrupts only */
	MCI_MASK1 &= ~((FIFO_TX_INT_MASK)|(DATA_END_INT_MASK)|(ERR_TX_INT_MASK)); /* FIFO TX interrupts only */
#endif
	return;
}
/*****************************************************************/

void MCI_RXEnable(void) {
#if MCI_DMA_ENABLED
	MCI_MASK0 |= ((DATA_END_INT_MASK)|(ERR_RX_INT_MASK));
	/* Enable RX interrupts only */
	MCI_MASK1 |= ((DATA_END_INT_MASK)|(ERR_RX_INT_MASK));
	/* Enable RX interrupts only */
#else
	MCI_MASK0 |= ((FIFO_RX_INT_MASK)|(DATA_END_INT_MASK)|(ERR_RX_INT_MASK)); /* FIFO RX interrupts only */
	MCI_MASK1 |= ((FIFO_RX_INT_MASK)|(DATA_END_INT_MASK)|(ERR_RX_INT_MASK)); /* FIFO RX interrupts only */
#endif
	return;
}
/*****************************************************************/

void MCI_RXDisable(void) {
#if MCI_DMA_ENABLED
	MCI_MASK0 &= ~((DATA_END_INT_MASK)|(ERR_RX_INT_MASK));
	/* Enable TX interrupts only */
	MCI_MASK1 &= ~((DATA_END_INT_MASK)|(ERR_RX_INT_MASK));
	/* Enable TX interrupts only */
#else
	MCI_MASK0 &= ~((FIFO_RX_INT_MASK)|(DATA_END_INT_MASK)|(ERR_RX_INT_MASK)); /* FIFO TX interrupts only */
	MCI_MASK1 &= ~((FIFO_RX_INT_MASK)|(DATA_END_INT_MASK)|(ERR_RX_INT_MASK)); /* FIFO TX interrupts only */
#endif
	return;
}

/******************************************************************************
 ** Function name:		MCI_CheckStatus
 **
 ** Descriptions:		MCI Check status before and after the block read and 
 **						write. Right after the block read and write, this routine 
 **						is important that, even the FIFO is empty, complete
 **						block has been sent, but, data is still being written
 **						to the card, this routine is to ensure that the data
 **						has been written based on the state of the card, not
 **						by the length being set.  
 **
 ** parameters:			None
 ** Returned value:		TRUE or FALSE
 ** 
 ******************************************************************************/
DWORD MCI_CheckStatus(void) {
	DWORD respValue;

	while ( 1) // mthomas TODO: possible deadlock
	{
		if ( (respValue = MCI_Send_Status()) == INVALID_RESPONSE) {
			break;
		} else {
			/* The only valid state is TRANS per MMC and SD state diagram.
			 RCV state may be seen, but, I have found that it happens
			 only when TX_ACTIVE or RX_ACTIVE occurs before the WRITE_BLOCK and 
			 READ_BLOCK cmds are being sent, which is not a valid sequence. */
			if ( (respValue & (0x0F << 8)) == 0x0900) {
				return ( TRUE );
			}
		}
	}

	return ( FALSE );
}

/******************************************************************************
 ** Function name:		MCI_CmdProcess
 **
 ** Descriptions:		Called by MCI interrupt handler
 **						To simplify the process, for card initialization, the 
 **						CMD interrupts are disabled.
 **						
 **
 ** parameters:			None
 ** Returned value:		None
 ** 
 ******************************************************************************/
// mthomas - static
static void MCI_CmdProcess(void) {
	DWORD MCIStatus;

	MCIStatus = MCI_STATUS;

	if (MCIStatus & MCI_CMD_CRC_FAIL) {
		CmdCRCErrCount++;
		MCI_CLEAR = MCI_CMD_CRC_FAIL;
	}
	if (MCIStatus & MCI_CMD_TIMEOUT) {
		CmdTimeoutErrCount++;
		MCI_CLEAR = MCI_CMD_TIMEOUT;
	}
	/* Cmd Resp End or Cmd Sent */
	if (MCIStatus & MCI_CMD_RESP_END) {
		CmdRespEndCount++;
		MCI_CLEAR = MCI_CMD_RESP_END;
	}
	if (MCIStatus & MCI_CMD_SENT) {
		CmdSentCount++;
		MCI_CLEAR = MCI_CMD_SENT;
	}
	if (MCIStatus & MCI_CMD_ACTIVE) {
		CmdActiveCount++;
		MCI_CLEAR = MCI_CMD_ACTIVE;
	}
	return;
}

/******************************************************************************
 ** Function name:		MCI_DataErrorProcess
 **
 ** Descriptions:		Called by MCI interrupt handler
 **						Process data error. 
 **
 ** parameters:			None
 ** Returned value:		None
 ** 
 ******************************************************************************/
// mthomas - static
static void MCI_DataErrorProcess(void) {
	DWORD MCIStatus;

	MCIStatus = MCI_STATUS;

	if (MCIStatus & MCI_DATA_CRC_FAIL) {
		DataCRCErrCount++;
		MCI_CLEAR = MCI_DATA_CRC_FAIL;
	}
	if (MCIStatus & MCI_DATA_TIMEOUT) {
		DataTimeoutErrCount++;
		MCI_CLEAR = MCI_DATA_TIMEOUT;
	}
	/* Underrun or overrun */
	if (MCIStatus & MCI_TX_UNDERRUN) {
		DataTxUnderrunErrCount++;
		MCI_CLEAR = MCI_TX_UNDERRUN;
	}
	if (MCIStatus & MCI_RX_OVERRUN) {
		DataRxOverrunErrCount++;
		MCI_CLEAR = MCI_RX_OVERRUN;
	}
	/* Start bit error on data signal */
	if (MCIStatus & MCI_START_BIT_ERR) {
		DataStartbitErrCount++;
		MCI_CLEAR = MCI_START_BIT_ERR;
	}
	return;
}

/******************************************************************************
 ** Function name:		MCI_DATA_END_InterruptService
 **
 ** Descriptions:		Called by MCI interrupt handler
 **						This is the last interrupt module processing 
 **						the block write and	read to and from the MM card.
 **					 
 **						FIFO interrupts are also used when DMA is disabled                                   
 **						This routine simply clears the
 **						MCI_Block_End_Flag, and increments counters for debug
 **
 ** parameters:			None
 ** Returned value:		None
 ** 
 ******************************************************************************/
// mthomas - static
static void MCI_DATA_END_InterruptService(void) {
	DWORD MCIStatus;

	MCIStatus = MCI_STATUS;

	if (MCIStatus & MCI_DATA_END) /* Data end, and Data block end  */
	{
		DataEndCount++;
		MCI_CLEAR = MCI_DATA_END;
		return;
	}
	if (MCIStatus & MCI_DATA_BLK_END) {
		DataBlockEndCount++;
		MCI_CLEAR = MCI_DATA_BLK_END;
		MCI_TXDisable();
		MCI_Block_End_Flag = 0;
		return;
	}

	/* Tx active  */
	if (MCIStatus & MCI_TX_ACTIVE) {
		DataTxActiveCount++;
	}
	/* Rx active  */
	if (MCIStatus & MCI_RX_ACTIVE) {
		DataRxActiveCount++;
	}

	return;
}

/******************************************************************************
 ** Function name:	MCI_FIFOInterruptService
 **
 ** Descriptions:	Called by MCI interrupt handler when using FIFO 
 **					interrupts and DMA is disabled
 **
 **
 ** parameters:			None
 ** Returned value:		None
 ** 
 ******************************************************************************/
// mthomas - static
static void MCI_FIFOInterruptService(void) {
#if !MCI_DMA_ENABLED
	DWORD MCIStatus;

	MCIStatus = MCI_STATUS;

	if ( MCIStatus & (FIFO_TX_INT_MASK ) )
	{
		DataTxFIFOCount++; /* if using TX_HALF_EMPTY remove one WriteFifo below */
		if ( MCIStatus & MCI_TX_HALF_EMPTY ) /* empty is multiple of 512 block size */
		{
			// mthomas
			// MCI_WriteFifo((DWORD *)&WriteBlock[TXBlockCounter]); /* write 8 words to fifo */
			MCI_WriteFifo((volatile DWORD *)&WriteBlock[TXBlockCounter]); /* write 8 words to fifo */
			TXBlockCounter += 32;
		}
		if (TXBlockCounter == BLOCK_LENGTH) /* block complete */
		{
			TXBlockCounter = 0;
			MCI_MASK0 &= ~(FIFO_TX_INT_MASK); /* disable FIFO int until next block write */
			MCI_MASK1 &= ~(FIFO_TX_INT_MASK);
			/* wait for SD card to complete sending data i.e MCI_DATA_BLK_END interrupt */
		}
	}
	else if ( MCIStatus & (FIFO_RX_INT_MASK) )
	{
		DataRxFIFOCount++;
		if ( MCIStatus & MCI_RX_HALF_FULL ) /* if using RX_HALF_FULL remove one ReadFIFO below */
		{
			// mhomas
			// MCI_ReadFifo((DWORD *)&ReadBlock[RXBlockCounter]); /* read 8 words from fifo */
			MCI_ReadFifo((volatile DWORD *)&ReadBlock[RXBlockCounter]); /* read 8 words from fifo */
			RXBlockCounter += 32;
		}
		if (RXBlockCounter == BLOCK_LENGTH) /* block complete */
		{
			RXBlockCounter = 0;
		}
		// mthomas: maybe redundant
		if ( SDStatRead && RXBlockCounter == SD_BLOCK_LENGTH ) /* SD_STATUS complete */
		{
			SDStatRead = 0;
			RXBlockCounter = 0;
		}
		// mthomas end
	}
#endif

	DataFIFOCount++;
	return;
}

/******************************************************************************
 ** Function name:		MCI_IRQHandler
 **
 ** Descriptions:		MCI interrupt handler
 **						The handler to handle the block data write and read
 **						not for the commands.
 **
 ** parameters:			None
 ** Returned value:		None
 ** 
 ******************************************************************************/
// mthomas - avoid warning missing prototype
void MCI_IRQHandler(void) __irq ;
void MCI_IRQHandler(void)__irq
{
	DWORD MCI_Status;

	MCI_Status = MCI_STATUS;

	/* handle MCI_STATUS interrupt */
	if ( MCI_Status & DATA_ERR_INT_MASK )
	{
		MCI_DataErrorProcess();
		MCI_DataErrorProcess_count++;
		VICVectAddr = 0; /* Acknowledge Interrupt */
		return;
	}
	if ( MCI_Status & DATA_END_INT_MASK )
	{
		MCI_DATA_END_InterruptService();
		MCI_DATA_END_InterruptService_count++;
		VICVectAddr = 0; /* Acknowledge Interrupt */
		return;
	}
	else if ( MCI_Status & FIFO_INT_MASK )
	{
		MCI_FIFOInterruptService();
		MCI_FIFOInterruptService_count++;
		VICVectAddr = 0; /* Acknowledge Interrupt */
		return;
	}
	else if ( MCI_Status & CMD_INT_MASK )
	{
		MCI_CmdProcess();
		MCI_CmdProcess_count++;
		VICVectAddr = 0; /* Acknowledge Interrupt */
		return;
	}

	VICVectAddr = 0; /* Acknowledge Interrupt */
	return;
}

/******************************************************************************
 ** Function name:		MCI_Set_MCIClock
 **
 ** Descriptions:		Set MCI clock rate, during initialization phase < 400K
 **						during data phase < 20Mhz.
 **
 ** parameters:			Clock rate to be set
 ** Returned value:		None
 ** 
 ******************************************************************************/
void MCI_Set_MCIClock(DWORD ClockRate) {
	volatile DWORD i; // mthomas volatile
	DWORD ClkValue = 0;

	if (ClockRate == SLOW_RATE) {
		ClkValue |= MCLKDIV_SLOW; /* slow clock */
	} else if (ClockRate == NORMAL_RATE) {
		ClkValue |= MCLKDIV_NORMAL;/* normal clock */
	} else {
		// mthomas - added default - low
		ClkValue |= MCLKDIV_SLOW;
	}

	MCI_CLOCK &= ~(0xFF);
	/* clear clock divider */
	// laebi: Disable MCI clock output when bus is idle (bit 9)
	MCI_CLOCK |= (1 << 8) | (1 << 9) | ClkValue;
	for (i = 0; i < 0x100; i++)
		; /* delay 3MCLK + 2PCLK before next write */

	return;
}

/******************************************************************************
 ** Function name:		SD_Set_BusWidth
 **
 ** Descriptions:		1-bit bus or 4-bit bus.
 **
 ** parameters:			bus width
 ** Returned value:		TRUE or FALSE
 **
 ** modified by Martin Thomas: do not set 4 bit if 
 ** parameter is SD_1_BIT
 ******************************************************************************/
DWORD SD_Set_BusWidth(DWORD width) {
	volatile DWORD i; // mthomas - volatile

	for (i = 0; i < 0x10; i++)
		; /* delay 3MCLK + 2PCLK  */

	if (width == SD_1_BIT) {
		MCI_CLOCK &= ~(1 << 11);
		/* 1 bit bus */
	} else if (width == SD_4_BIT) {
		MCI_CLOCK |= (1 << 11);
		/* 4 bit bus */
		// mthomas - moved this here from below
		if (MCI_Send_ACMD_Bus_Width( BUS_WIDTH_4BITS) == FALSE) {
			mci_debug_printf("setting 4bit width failed\n");
			return ( FALSE );
		}mci_debug_printf("setting 4bit width success\n");
	} else {
		// mthomas - added default
		MCI_CLOCK &= ~(1 << 11);
		/* 1 bit bus */
	}

#if 0
	// mthomas - TODO: why set 4bit if param is given as SD_1_BIT?
	if ( MCI_Send_ACMD_Bus_Width( BUS_WIDTH_4BITS ) == FALSE )
	{
		return( FALSE );
	}
#endif

	return ( TRUE );
}

/******************************************************************************
 ** Function name:		MCI_Init
 **
 ** Descriptions:		Set MCI clock and power registers, setup VIC for
 **						data interrupt.
 **
 ** parameters:			None
 ** Returned value:		true or false, if VIC table is full, return false
 **
 ** modified by Martin Thomas to avoid that the
 ** hard-coded pinsel-values disable other functions
 ******************************************************************************/
DWORD MCI_Init(void) {
	volatile DWORD i; // mthomas - volatile

	PCONP |= ( 1 << 28 );
	/* Enable clock to the MCI block */

	if ( MCI_CLOCK & (1 << 8)) {
		MCI_CLOCK &= ~(1 << 8);
	}
	if ( MCI_POWER & 0x02) {
		MCI_POWER = 0x00;
	}
	for (i = 0; i < 0x1000; i++)
		;

	/* Disable all interrupts for now */
	MCI_MASK0 = 0;
	MCI_MASK1 = MCI_MASK0;

	/* Due to reversed H/W logic in the MCB2300 board, the MCI power pin
	 needs to be configured as GPIO pin that I need to set it active low manually, 
	 once it's set as MCI power pin, it will be active high. */
	/*connect MCI signals to P0.19-P0.22, and P2.11-P2.13*/

/*	// laebi: EA_BOARD pinout is different
#if EA_BOARD_LPC24XX
#warning EA24
	PINSEL1 = ( PINSEL1 & ~( (3<<12) )) | ((2<<12));
	PINSEL2 = ( PINSEL2 & ~( (3<<4)|(3<<6)|(3<<10)|(3<<14)|(3<<22)|(3<<24)) )
	| ((2<<4)|(2<<6)|(2<<10)|(2<<14)|(2<<22)|(2<<24));
#elif (IAR_BOARD_LPC23XX) || (KEIL_BOARD_LPC23XX)
#warning EA,OMX23/Keil23
	PINSEL1 = ( PINSEL1 & ~( (3<<6)|(3<<8)|(3<<10)|(3<<12) ) )
	| ((2<<6)|(2<<8)|(2<<10)|(2<<12));
	PINSEL4 = ( PINSEL4 & ~((3<<22)|(3<<24)|(3<<26) ) )
	| ((2<<22)|(2<<24)|(2<<26) );
#else
#warning NXP default
	PINSEL1 = 0x2A80;
	PINSEL4 = 0x0A800000;
#endif

#if MCB2300_VERSION_0
#warning SCS MCIPWR active level high
	SCS |= 0x08;
#if 0
	PINSEL1 = 0x2280;
	PINSEL4 = 0x0A800000;
	IODIR0 = 1 << 21; // MCI_PWR as GPIO output 
	IOCLR0 = 1 << 21;
#endif
#else
	// #warning SCS MCIPWR active level low
	SCS &= ~0x08;
#if 0
	PINSEL1 = 0x2A80;
	PINSEL4 = 0x0A800000;
#endif
#endif*/

	PINSEL2 |= (2 << 4) | (2 << 6) | (2 << 12) | (2 << 14) | (2 << 22) | (2 << 24) | (2 << 10);

	/*set up clocking default mode, clear any registers as needed */
	MCI_COMMAND = 0;
	MCI_DATA_CTRL = 0;
	MCI_CLEAR = 0x7FF;
	/* clear all pending interrupts */

	MCI_POWER = 0x02;
	/* power up */
	while ( !(MCI_POWER & 0x02)) {
		;
	}
	for (i = 0; i < 0x100; i++) {
		;
	}

	/* During identification phase, the clock should be less than
	 400Khz. Once we pass this phase, the normal clock can be set up
	 to 25Mhz on SD card and 20Mhz on MMC card. */
	MCI_Set_MCIClock( SLOW_RATE);
	MCI_POWER |= 0x01;
	/* bit 1 is set already, from power up to power on */

	for (i = 0; i < 0x2000; i++) {
		;
	}
	if (install_irq( MCI_INT, (void *)MCI_IRQHandler, HIGHEST_PRIORITY) == FALSE) {
		return ( FALSE );
	}

	/* During the initialization phase, to simplify the process, the CMD related 
	 interrupts are disabled. The DATA related interrupts are enabled when
	 the FIFOs are used and just before WRITE_BLOCK READ_BLOCK cmds are issues, and 
	 disabled after the data block has been written and read. Please also note,
	 before WRITE_BLOCK only TX related data interrupts are enabled, and before
	 READ_BLOCK only RX related data interrupts are enabled. */

	mci_debug_printf("MCI_INIT ok\n");
	return ( TRUE );
}

/******************************************************************************
 ** Function name:		MCI_SendCmd
 **
 ** Descriptions:		The routine is used to send a CMD to the card		
 **
 ** parameters:			CmdIndex, Argument, ExpectResp Flag, AllowTimeout flag
 ** Returned value:		None
 ** 
 ******************************************************************************/
void MCI_SendCmd(DWORD CmdIndex, DWORD Argument, DWORD ExpectResp,
		DWORD AllowTimeout) {
	volatile DWORD i; // mthomas - volatile
	DWORD CmdData = 0;
	DWORD CmdStatus;

	/* the command engine must be disabled when we modify the argument
	 or the peripheral resends */
	while ( (CmdStatus = MCI_STATUS) & MCI_CMD_ACTIVE) /* Command in progress. */
	{
		MCI_COMMAND = 0;
		MCI_CLEAR = CmdStatus | MCI_CMD_ACTIVE;
	}
	for (i = 0; i < 0x100; i++) {
		;
	}

	/*set the command details, the CmdIndex should 0 through 0x3F only */
	CmdData |= (CmdIndex & 0x3F); /* bit 0 through 5 only */
	if (ExpectResp == EXPECT_NO_RESP) /* no response */
	{
		CmdData &= ~((1 << 6) | (1 << 7)); /* Clear long response bit as well */
	} else if (ExpectResp == EXPECT_SHORT_RESP) /* expect short response */
	{
		CmdData |= (1 << 6);
	} else if (ExpectResp == EXPECT_LONG_RESP) /* expect long response */
	{
		CmdData |= (1 << 6) | (1 << 7);
	}

	if (AllowTimeout) /* allow timeout or not */
	{
		CmdData |= (1 << 8);
	} else {
		CmdData &= ~(1 << 8);
	}

	/*send the command*/
	CmdData |= (1 << 10); /* This bit needs to be set last. */
	MCI_ARGUMENT = Argument;
	/* Set the argument first, finally command */
	MCI_COMMAND = CmdData;

	return;
}

/******************************************************************************
 ** Function name:		MCI_GetCmdResp
 **
 ** Descriptions:		Get response from the card. This module is always used
 **						in pair with MCI_SendCmd()		
 **
 ** parameters:			Expected cmd data, expect response flag, pointer to the 
 **						response
 **						Expected cmd data should be the same as that in SendCmd()
 **						expect response flag could be	EXPECT_NO_RESP
 **														EXPECT_SHORT_RESP
 **														EXPECT_LONG_RESP
 **						if GetCmdResp() is 0, check the pointer to the response
 **						field to get the response value, if GetCmdResp() returns 
 **						non-zero, no need to check the response field, just resend
 **						command or bailout. 
 ** Returned value:		Response status, 0 is valid response. 
 ** 
 ******************************************************************************/
DWORD MCI_GetCmdResp(DWORD ExpectCmdData, DWORD ExpectResp, DWORD *CmdResp) {
	DWORD CmdRespStatus = 0;
	DWORD LastCmdIndex;

	if (ExpectResp == EXPECT_NO_RESP) {
		return ( 0 );
	}

	while ( 1) {
		CmdRespStatus = MCI_STATUS;
		if (CmdRespStatus & (MCI_CMD_TIMEOUT)) {
			MCI_CLEAR = CmdRespStatus | MCI_CMD_TIMEOUT;
			MCI_COMMAND = 0;
			MCI_ARGUMENT = 0xFFFFFFFF;
			return (CmdRespStatus );
		}
		if (CmdRespStatus & MCI_CMD_CRC_FAIL) {
			MCI_CLEAR = CmdRespStatus | MCI_CMD_CRC_FAIL;
			LastCmdIndex = MCI_COMMAND & 0x003F;
			if ( (LastCmdIndex == SEND_OP_COND) || (LastCmdIndex
					== SEND_APP_OP_COND) || (LastCmdIndex == STOP_TRANSMISSION)) {
				MCI_COMMAND = 0;
				MCI_ARGUMENT = 0xFFFFFFFF;
				break; /* ignore CRC error if it's a resp for SEND_OP_COND 
				 or STOP_TRANSMISSION. */
			} else {
				mci_debug_printf("*** CRCfail");
				return (CmdRespStatus );
			}
		} else if (CmdRespStatus & MCI_CMD_RESP_END) {
			MCI_CLEAR = CmdRespStatus | MCI_CMD_RESP_END;
			break; /* cmd response is received, expecting response */
		}
	}

	if ( (MCI_RESP_CMD & 0x3F) != ExpectCmdData) {
		/* If the response is not R1, in the response field, the Expected Cmd data
		 won't be the same as the CMD data in SendCmd(). Below four cmds have
		 R2 or R3 response. We don't need to check if MCI_RESP_CMD is the same
		 as the Expected or not. */
		if ( (ExpectCmdData != SEND_OP_COND) && (ExpectCmdData
				!= SEND_APP_OP_COND) && (ExpectCmdData != ALL_SEND_CID)
				&& (ExpectCmdData != SEND_CSD)) {
			CmdRespStatus = INVALID_RESPONSE; /* Reuse error status */
			return ( INVALID_RESPONSE );
		}
	}

	if (ExpectResp == EXPECT_SHORT_RESP) {
		*CmdResp = MCI_RESP0;
	} else if (ExpectResp == EXPECT_LONG_RESP) {
		*CmdResp = MCI_RESP0;
		*(CmdResp+1) = MCI_RESP1;
		*(CmdResp+2) = MCI_RESP2;
		*(CmdResp+3) = MCI_RESP3;
	}
	return ( 0 ); /* Read MCI_RESP0 register assuming it's not long response. */
}

/******************************************************************************
 ** Function name:		MCI_Go_Idle_State
 **
 ** Descriptions:		CMD0, the very first command to be sent to initialize
 **						either MMC or SD card.
 **
 ** parameters:			None
 ** Returned value:		true or false, true if card has been initialized.
 ** 
 ******************************************************************************/
DWORD MCI_Go_Idle_State(void) {
	DWORD retryCount;
	DWORD respStatus;
	DWORD respValue[4];

	retryCount = 0x20;
	while (retryCount > 0) {
		/* Send CMD0 command repeatedly until the response is back correctly */
		MCI_SendCmd( GO_IDLE_STATE, 0x00000000, EXPECT_NO_RESP, 0);
		respStatus = MCI_GetCmdResp( GO_IDLE_STATE, EXPECT_NO_RESP, (DWORD *)respValue);
		if (respStatus == 0) {
			break;
		}
		retryCount--;
	}

	if (respStatus != 0) /* timeout, give up */
	{
		mci_debug_printf("Timeout go idle\n");
		return ( FALSE );
	}
	return ( TRUE );
}

/******************************************************************************
 ** Function name:		MCI_Send_OP_Cond
 **
 ** Descriptions:		CMD1 for MMC
 **
 ** parameters:			None
 ** Returned value:		true or false, true if card has response back before
 **						timeout, false is timeout on the command.
 ** 
 ******************************************************************************/
// mthomas - static
static DWORD MCI_Send_OP_Cond(void) {
	volatile DWORD i; // mthomas - volatile
	DWORD retryCount;
	DWORD respStatus;
	DWORD respValue[4];

	retryCount = 0x200; /* reset retry counter */
	while (retryCount > 0) {
		/* Send CMD1 command repeatedly until the response is back correctly */
		MCI_SendCmd( SEND_OP_COND, OCR_INDEX, EXPECT_SHORT_RESP, 0);
		respStatus = MCI_GetCmdResp( SEND_OP_COND, EXPECT_SHORT_RESP, (DWORD *)&respValue[0]);
		/* bit 0 and bit 2 must be zero, or it's timeout or CRC error */
		if ( !(respStatus & MCI_CMD_TIMEOUT) && (respValue[0] & 0x80000000)) {
			return ( TRUE ); /* response is back and correct. */
		}
		for (i = 0; i < 0x20; i++)
			;
		retryCount--;
	}
	return ( FALSE );
}

/******************************************************************************
 ** Function name:		MCI_Send_ACMD
 **
 ** Descriptions:		CMD55, before sending an ACMD, call this routine first
 **
 ** parameters:			None
 ** Returned value:		true or false, true if card has responded before timeout.
 **						false is timeout.
 ** 
 ******************************************************************************/
// mthomas - static
static DWORD MCI_Send_ACMD(void) {
	volatile DWORD i; // mthomas - volatile
	DWORD retryCount;
	DWORD CmdArgument;
	DWORD respStatus;
	DWORD respValue[4];

	// mthomas - warning: MCI_CardType may be set from
	// extern to other than UNKOWN and this
	// will fail on init (workaround: added MCI_CardType = UNKNOWN below)
	// laebi: add CardType SDHC_CARD, otherwise it wouldnt work
	if (MCI_CardType == SD_CARD || MCI_CardType == SDHC_CARD) {
		CmdArgument = CardRCA; /* Use the address from SET_RELATIVE_ADDR cmd */
	} else /* if MMC or unknown card type, use 0x0. */
	{
		CmdArgument = 0x00000000;
	}

	retryCount = 20;
	while (retryCount > 0) {
		/* Send CMD55 command followed by an ACMD */
		MCI_SendCmd( APP_CMD, CmdArgument, EXPECT_SHORT_RESP, 0);
		respStatus = MCI_GetCmdResp( APP_CMD, EXPECT_SHORT_RESP, (DWORD *)&respValue[0]);
		if ( !respStatus && (respValue[0] & CARD_STATUS_ACMD_ENABLE)) /* Check if APP_CMD enabled */
		{
			return ( TRUE );
		}
		for (i = 0; i < 0x20; i++) {
			;
		}
		retryCount--;
	}
	return ( FALSE );
}

/******************************************************************************
 ** Function name:		MCI_Send_ACMD_OP_Cond
 **
 ** Descriptions:		If Send_OP_Cond is timeout, it's not a MMC card, try 
 **						this combination to see if we can communicate with 
 **						a SD card.
 **
 ** parameters:			ACMD41 argument (mthomas)
 ** Returned value:		true or false, true if card has been initialized.
 **						respValue[0] in resp0
 ** 
 ** modified by Martin Thomas (in: arg, out resp0)
 ******************************************************************************/
// mthomas - static
static DWORD MCI_Send_ACMD_OP_Cond(DWORD acmd41_arg, DWORD *resp0) {
	volatile DWORD i; // mthomas - volatile
	DWORD retryCount;
	DWORD respStatus;
	DWORD respValue[4];

	/* timeout on SEND_OP_COND command on MMC, now, try SEND_APP_OP_COND 
	 command to SD */
	// mthomas - less retries retryCount = 0x200;			/* reset retry counter */
	retryCount = 0x50;
	while (retryCount > 0) {
		MCI_POWER &= ~(1 << 6 );
		/* Clear Open Drain output control for SD */
		for (i = 0; i < 0x3000; i++) {
			;
		}

		if (MCI_Send_ACMD() == FALSE) {
			retryCount--; // mthomas - line added
			continue;
		}

		/* Send ACMD41 command repeatedly until the response is back correctly */
		// MCI_SendCmd( SEND_APP_OP_COND, OCR_INDEX, EXPECT_SHORT_RESP, 0 );
		MCI_SendCmd( SEND_APP_OP_COND, acmd41_arg, EXPECT_SHORT_RESP, 0);
		respStatus = MCI_GetCmdResp( SEND_APP_OP_COND, EXPECT_SHORT_RESP, (DWORD *)&respValue[0]);
		if ( !(respStatus & MCI_CMD_TIMEOUT) && (respValue[0] & 0x80000000)) {
			if (resp0 != NULL) {
				*resp0 = respValue[0];
			}
			return ( TRUE ); /* response is back and correct. */
		}
		for (i = 0; i < 0x20; i++) {
			;
		}
		retryCount--;
	}
	return ( FALSE );
}

/******************************************************************************
 ** Function name:		MCI_Send_IF_Cond
 **
 ** Descriptions:		CMD8 to detect SD-Card >= Version 2.0
 **
 ** parameters:			None
 ** Returned value:		true or false, true if card has response back before
 **						timeout, false is timeout on the command.
 **
 ** added by Martin Thomas with information from Juri Haberland, Chan and
 ** the SanDisk Product Manuals V2.2
 ******************************************************************************/
// mthomas - static
static DWORD MCI_Send_IF_Cond(void) {
	volatile DWORD i; // mthomas - volatile
	DWORD retryCount;
	DWORD respStatus;
	DWORD respValue[4];

	retryCount = 20; /* reset retry counter */
	while (retryCount > 0) {
		MCI_POWER &= ~(1 << 6 );
		/* Clear Open Drain output control for SD */
		for (i = 0; i < 0x3000; i++) {
			;
		}

		/* Send CMD8 command repeatedly until the response is back correctly or timeout */
		MCI_SendCmd( SEND_IF_COND, SEND_IF_COND_ARG , EXPECT_SHORT_RESP, 0);
		respStatus = MCI_GetCmdResp( SEND_IF_COND, EXPECT_SHORT_RESP, (DWORD *)&respValue[0]);
		/* mthomas TODO: check result bits - for now just check if no timeout */
		if ( !(respStatus & MCI_CMD_TIMEOUT)) {
			return ( TRUE ); /* response is back and correct. */
		}
		for (i = 0; i < 0x20; i++)
			;
		retryCount--;
	}
	return ( FALSE );
}

#if 1
/******************************************************************************
 ** Function name:		MCI_CardInit
 **
 ** Descriptions:		Try CMD1 first for MMC, if it's timeout, try CMD55 
 **						and CMD41 for SD, if both failed, initialization faliure, 
 **						bailout with unknown card type. Otherwise, return the
 **						card type, either MMC or SD.		
 **
 ** parameters:			None
 ** Returned value:		Card type.
 ** 
 ** extended by Martin Thomas with information from Juri Haberland,
 ** public available Information for SDHC, Chan's fat-code and 
 ** the SanDisk SD-Card manual V2.2
 ******************************************************************************/
DWORD MCI_CardInit(void) {
	volatile DWORD i;
	DWORD CardType= CARD_UNKNOWN;
	DWORD acmd41_arg;
	DWORD resp0;

	MCI_CardType = CARD_UNKNOWN;

	if (MCI_Go_Idle_State() == FALSE) {
		mci_debug_printf("Init Go Idle failed\n");
		return ( CARD_UNKNOWN );
	}

	if (MCI_Send_IF_Cond() == TRUE) {
		mci_debug_printf("no timeout on CMD8 -> SD-Card>=Version 2.0\n");
		CardType = SDV2_CARD;
		acmd41_arg = OCR_CARD_HIGH_CAPACITY | OCR_HIGH_VOLTAGE_RANGE;
	} else {
		mci_debug_printf("timout CMD8 -> SD-Card<Version 2.0 or MMC\n");
		acmd41_arg = OCR_HIGH_VOLTAGE_RANGE;
	}

	if (MCI_Send_ACMD_OP_Cond(acmd41_arg, &resp0) == TRUE) {
		mci_debug_printf("ACMD41 success -> SD-Card SC or HC\n");
		if (resp0 & OCR_CARD_HIGH_CAPACITY) {
			mci_debug_printf("SDHC detected - EXPERIMENTAL!\n");
			CardType = SDHC_CARD;
		} else {
			mci_debug_printf("SDSC detected\n");
			CardType = SDSC_CARD;
		}
	} else {
		mci_debug_printf("MCI_Send_ACMD failed -> MMC, invalid power-range or no card\n");
		MCI_POWER |= (1 << 6 );
		/* Set Open Drain output control for MMC */
		for (i = 0; i < 0x3000; i++) {
			;
		}
		/* try CMD1 of MMC */
		if (MCI_Send_OP_Cond() == TRUE) {
			CardType = MMC_CARD;
		}
	}

	MCI_CardType = CardType;

	return (CardType );
}
#else
// NXP original:
/******************************************************************************
 ** Function name:		MCI_CardInit
 **
 ** Descriptions:		Try CMD1 first for MMC, if it's timeout, try CMD55 
 **						and CMD41 for SD, if both failed, initialization faliure, 
 **						bailout with unknown card type. Otherwise, return the
 **						card type, either MMC or SD.		
 **
 ** parameters:			None
 ** Returned value:		Card type.
 **
 ******************************************************************************/
DWORD MCI_CardInit( void )
{
	volatile DWORD i; // mthomas - volatile
	DWORD CardType;

	MCI_CardType = CARD_UNKNOWN; // mthomas - line added

	if ( MCI_Go_Idle_State() == FALSE )
	{
		mci_debug_printf("Init Go Idle failed\n");
		return( CARD_UNKNOWN );
	}

	MCI_POWER |= (1 << 6 ); /* Set Open Drain output control for MMC */
	for ( i = 0; i < 0x3000; i++ ) {;}

	/* Try CMD1 first for MMC, if it's timeout, try CMD55 and CMD41 for SD,
	 if both failed, initialization faliure, bailout. */
	if ( MCI_Send_OP_Cond() == TRUE )
	{
		CardType = MMC_CARD;
		MCI_CardType = MMC_CARD; // mthomas - line added
		return ( CardType ); /* Found the card, it's a MMC */
	}
	else {
		mci_debug_printf("MCI_Send_OP_Conf failed - try ACMD\n");
		// mthomas if ( MCI_Send_ACMD_OP_Cond() == TRUE )
		if ( MCI_Send_ACMD_OP_Cond( OCR_INDEX, NULL ) == TRUE )
		{
			CardType = SD_CARD;
			MCI_CardType = SD_CARD; // mthomas - line added
			return ( CardType ); /* Found the card, it's a SD */
		}
		else {
			mci_debug_printf("MCI_Send_ACMD failed\n");
			/* tried both MMC and SD card, give up */
			return ( CARD_UNKNOWN );
		}
	}
}
#endif

/******************************************************************************
 ** Function name:		MCI_Check_CID
 **
 ** Descriptions:		Send CMD2, ALL_SEND_CID
 **
 ** parameters:			None
 ** Returned value:		If not timeout, return true.
 ** 
 ******************************************************************************/
DWORD MCI_Check_CID(void) {
	volatile DWORD i; // mthomas - volatile
	DWORD retryCount;
	DWORD respStatus;
	DWORD respValue[4];

	/* This command is normally after CMD1(MMC) or ACMD41(SD). */
	retryCount = 0x20; /* reset retry counter */
	while (retryCount > 0) {
		/* Send CMD2 command repeatedly until the response is back correctly */
		MCI_SendCmd( ALL_SEND_CID, 0, EXPECT_LONG_RESP, 0);
		respStatus = MCI_GetCmdResp( ALL_SEND_CID, EXPECT_LONG_RESP, (DWORD *)&respValue[0]);
		/* bit 0 and bit 2 must be zero, or it's timeout or CRC error */
		if ( !(respStatus & MCI_CMD_TIMEOUT )) {
			return ( TRUE ); /* response is back and correct. */
		}
		for (i = 0; i < 0x20; i++) {
			;
		}
		retryCount--;
	}
	return ( FALSE );
}

/******************************************************************************
 ** Function name:		MCI_Set_Address
 **
 ** Descriptions:		Send CMD3, STE_RELATIVE_ADDR, should after CMD2		
 **
 ** parameters:			None
 ** Returned value:		TRUE if response is back before timeout.
 ** 
 ** modified by mthomas
 ******************************************************************************/
DWORD MCI_Set_Address(void) {
	volatile DWORD i; // mthomas - volatile
	DWORD retryCount;
	DWORD respStatus;
	DWORD respValue[4];
	DWORD CmdArgument;

	/* If it's a SD card, SET_RELATIVE_ADDR is to get the address
	 from the card and use this value in RCA, if it's a MMC, set default
	 RCA addr. 0x00010000. */
#if 1
	// mthomas:
	if (MCI_CardType & MMC_CARD) {
		/* If it's unknown or MMC_CARD, fix the RCA address */
		CmdArgument = 0x00010000;
	} else {
		CmdArgument = 0;
	}
#else
	// NXP original:
	if ( MCI_CardType == SD_CARD )
	{
		CmdArgument = 0;
	}
	else /* If it's unknown or MMC_CARD, fix the RCA address */
	{
		CmdArgument = 0x00010000;
	}
#endif

	retryCount = 0x20; /* reset retry counter */
	while (retryCount > 0) {
		/* Send CMD3 command repeatedly until the response is back correctly */
		MCI_SendCmd( SET_RELATIVE_ADDR, CmdArgument, EXPECT_SHORT_RESP, 0);
		respStatus = MCI_GetCmdResp( SET_RELATIVE_ADDR, EXPECT_SHORT_RESP, (DWORD *)&respValue[0]);
		/* bit 0 and bit 2 must be zero, or it's timeout or CRC error */
		/* It should go to IDEN state and bit 8 should be 1 */
		if ( !(respStatus & MCI_CMD_TIMEOUT) && ((respValue[0] & (0x0F << 8))
				== 0x0500)) {
			CardRCA = respValue[0] & 0xFFFF0000; /* Save the RCA value from SD card */
			return ( TRUE ); /* response is back and correct. */
		}
		for (i = 0; i < 0x20; i++) {
			;
		}
		retryCount--;
	}
	return ( FALSE );
}

/******************************************************************************
 ** Function name:		MCI_Send_CSD
 **
 ** Descriptions:		CMD9, SEND_CSD cmd, it should be sent only at
 **						STBY state and after CMD3. See MMC and SD spec. state 
 **						diagram.
 **
 ** parameters:			None
 ** Returned value:		Response value
 ** 
 ******************************************************************************/
// DWORD MCI_Send_CSD( void )
DWORD MCI_Send_CSD(DWORD *csd) {
	volatile DWORD i; // mthomas - volatile
	DWORD retryCount;
	DWORD respStatus;
	DWORD respValue[4];
	DWORD CmdArgument;

#if 1
	// mthomas:
	if (MCI_CardType & MMC_CARD) {
		/* If it's unknown or MMC_CARD, fix the RCA address */
		CmdArgument = 0x00010000;
	} else {
		CmdArgument = CardRCA;
	}
#else
	// NXP original:
	if ( MCI_CardType == SD_CARD )
	{
		CmdArgument = CardRCA;
	}
	else /* if MMC or unknown card type, use default RCA addr. */
	{
		CmdArgument = 0x00010000;
	}
#endif

	retryCount = 0x20;
	while (retryCount > 0) {
		/* Send SET_BLOCK_LEN command before read and write */
		MCI_CLEAR |= (MCI_CMD_TIMEOUT | MCI_CMD_CRC_FAIL | MCI_CMD_RESP_END);
		MCI_SendCmd( SEND_CSD, CmdArgument, EXPECT_LONG_RESP, 0);
		respStatus = MCI_GetCmdResp( SEND_CSD, EXPECT_LONG_RESP, (DWORD *)&respValue[0]);
		if ( !respStatus) {
			mci_debug_printf("MCI_Send_CSD result: %08x %08x %08x %08x\n",
					respValue[0], respValue[1],
					respValue[2], respValue[3]
			);

			if (csd) {
				for (i = 0; i < 4; i++) {
					csd[i] = respValue[i];
				}
			}
			return ( TRUE );
		}
		for (i = 0; i < 0x20; i++) {
			;
		}
		retryCount--;
	}
	return ( FALSE );
}

/******************************************************************************
 ** Function name:		MCI_Select_Card
 **
 ** Descriptions:		CMD7, SELECT_CARD, should be after CMD9, the state
 **						will be inter-changed between STBY and TRANS after 
 **						this cmd.
 **
 ** parameters:			None
 ** Returned value:		return false if response times out.
 ** 
 ** modified by Martin Thomas for unselect (RCA 0)
 ******************************************************************************/
DWORD MCI_Select_Card(void) {
	volatile DWORD i; // mthomas - volatile
	DWORD retryCount;
	DWORD respStatus;
	DWORD respValue[4];
	DWORD CmdArgument;

#if 1
	// mthomas:
	if (MCI_CardType == CARD_UNKNOWN) {
		CmdArgument = 0; // RCA = 0 -> de-select -> trans to stby
	} else if (MCI_CardType & MMC_CARD) {
		/* If it's unknown or MMC_CARD, fix the RCA address */
		CmdArgument = 0x00010000;
	} else {
		CmdArgument = CardRCA;
	}
#else
	// NXP original:
	if ( MCI_CardType == SD_CARD )
	{
		CmdArgument = CardRCA;
	}
	else /* if MMC or unknown card type, use default RCA addr. */
	{
		CmdArgument = 0x00010000;
	}
#endif

	retryCount = 0x20;
	while (retryCount > 0) {
#if 1
		// mthomas:
		/* Send SELECT_CARD command before read and write */
		MCI_CLEAR |= (MCI_CMD_TIMEOUT | MCI_CMD_CRC_FAIL | MCI_CMD_RESP_END);
		MCI_SendCmd( SELECT_CARD, CmdArgument, EXPECT_SHORT_RESP, 0);
		respStatus = MCI_GetCmdResp( SELECT_CARD, EXPECT_SHORT_RESP, (DWORD *)&respValue[0]);

		if (CmdArgument) {
			// select card
			if ( !respStatus && ((respValue[0] & (0x0F << 8)) == 0x0700 )) { /* Should be in STANDBY state now and ready */
				/* mthomas: repsValue 0 is the card-status V2.2 p3-20
				 READ_FOR_DATA: ready, CURRENT_STATE=3->stby */
				return ( TRUE );
			}
		} else {
			// TODO: Timeouts - maybe because not card is selected
			return ( TRUE );
		}
#else
		// NXP original
		/* Send SELECT_CARD command before read and write */
		MCI_CLEAR |= (MCI_CMD_TIMEOUT | MCI_CMD_CRC_FAIL | MCI_CMD_RESP_END);
		MCI_SendCmd( SELECT_CARD, CmdArgument, EXPECT_SHORT_RESP, 0 );
		respStatus = MCI_GetCmdResp( SELECT_CARD, EXPECT_SHORT_RESP, (DWORD *)&respValue[0] );
		if ( !respStatus && ((respValue[0] & (0x0F << 8)) == 0x0700 ))
		{ /* Should be in STANDBY state now and ready */
			/* mthomas: READ_FOR_DATA: ready, CURRENT_STATE=3->stby */
			return ( TRUE );
		}
#endif
		for (i = 0; i < 0x20; i++) {
			;
		}mci_debug_printf(" Select_Card: arg:%08x respStatus: %08x resp0:%08x resp1:%08x\n",
				CmdArgument, respStatus, respValue[0], respValue[1]);
		retryCount--;
	} /* while retry */
	return ( FALSE );
}

/******************************************************************************
 ** Function name:		MCI_Send_Status
 **
 ** Descriptions:		CMD13, SEND_STATUS, the most important cmd to
 **						debug the state machine of the card.		
 **
 ** parameters:			None
 ** Returned value:		Response value(card status), true if the ready bit 
 **						is set in the card status register, if timeout, return 
 **						INVALID_RESPONSE 0xFFFFFFFF.
 ** 
 ** modified by mthomas
 ******************************************************************************/
DWORD MCI_Send_Status(void) {
	DWORD retryCount;
	DWORD respStatus;
	DWORD respValue[4];
	DWORD CmdArgument;

#if 1
	// mthomas:
	if (MCI_CardType & MMC_CARD) {
		/* If it's unknown or MMC_CARD, fix the RCA address */
		CmdArgument = 0x00010000;
	} else {
		CmdArgument = CardRCA;
	}
#else
	// NXP original:	
	if ( MCI_CardType == SD_CARD )
	{
		CmdArgument = CardRCA;
	}
	else /* if MMC or unknown card type, use default RCA addr. */
	{
		CmdArgument = 0x00010000;
	}
#endif

	/* Note that, since it's called after the block write and read, this timeout 
	 is important based on the clock you set for the data communication. */
	retryCount = 0x2000;
	while (retryCount > 0) {
		/* Send SELECT_CARD command before read and write */
		MCI_CLEAR |= (MCI_CMD_TIMEOUT | MCI_CMD_CRC_FAIL | MCI_CMD_RESP_END);
		MCI_SendCmd( SEND_STATUS, CmdArgument, EXPECT_SHORT_RESP, 0);
		respStatus = MCI_GetCmdResp( SEND_STATUS, EXPECT_SHORT_RESP, (DWORD *)&respValue[0]);
		if ( !respStatus && (respValue[0] & (1 << 8))) { /* The ready bit should be set, it should be in either TRAN or RCV state now */
			return (respValue[0] );
		}
		retryCount--;
	}
	return ( INVALID_RESPONSE );
}

/******************************************************************************
 ** Function name:		MCI_Set_BlockLen
 **
 ** Descriptions:		CMD16, SET_BLOCKLEN, called after CMD7(SELECT_CARD)
 **						called in the TRANS state.
 **
 ** parameters:			The length of the data block to be written or read.
 ** Returned value:		true or false, return TRUE if ready bit is set, and it's
 **						in TRANS state.
 ** 
 ******************************************************************************/
DWORD MCI_Set_BlockLen(DWORD blockLength) {
	volatile DWORD i; // mthomas - volatile
	DWORD retryCount;
	DWORD respStatus;
	DWORD respValue[4];

	retryCount = 0x20;
	while (retryCount > 0) {
		/* Send SET_BLOCK_LEN command before read and write */
		MCI_CLEAR |= (MCI_CMD_TIMEOUT | MCI_CMD_CRC_FAIL | MCI_CMD_RESP_END);
		MCI_SendCmd( SET_BLOCK_LEN, blockLength, EXPECT_SHORT_RESP, 0);
		respStatus = MCI_GetCmdResp( SET_BLOCK_LEN, EXPECT_SHORT_RESP, (DWORD *)&respValue[0]);
		/* bit 9 through 12 should be in transfer state now. bit 8 is ready. */
		if ( !respStatus && ((respValue[0] & (0x0F << 8)) == 0x0900)) {
			return ( TRUE );
		}
		for (i = 0; i < 0x20; i++) {
			;
		}
		retryCount--;
	}
	return ( FALSE );
}

/******************************************************************************
 ** Function name:		MCI_Send_ACMD_Bus_Width
 **
 ** Descriptions:		ACMD6, SET_BUS_WIDTH, if it's SD card, we can
 **						use the 4-bit bus instead of 1-bit. This cmd
 **						can only be called during TRANS state.
 **						Since it's a ACMD, CMD55 APP_CMD needs to be
 **						sent out first. 		
 **
 ** parameters:			Bus width value, 1-bit is 0, 4-bit is 10
 ** Returned value:		true or false, true if the card is still in the 
 **						TRANS state after the cmd.
 ** 
 ******************************************************************************/
DWORD MCI_Send_ACMD_Bus_Width(DWORD buswidth) {
	volatile DWORD i; // mthomas - volatile
	DWORD retryCount;
	DWORD respStatus;
	DWORD respValue[4];

	retryCount = 0x20; /* reset retry counter */
	while (retryCount > 0) {
		if (MCI_Send_ACMD() == FALSE) {
			continue;
		}
		/* Send ACMD6 command to set the bus width */
		MCI_SendCmd( SET_ACMD_BUS_WIDTH, buswidth, EXPECT_SHORT_RESP, 0);
		respStatus = MCI_GetCmdResp( SET_ACMD_BUS_WIDTH, EXPECT_SHORT_RESP, (DWORD *)&respValue[0]);
		if ( !respStatus && ((respValue[0] & (0x0F << 8)) == 0x0900)) {
			return ( TRUE ); /* response is back and correct. */
		}
		for (i = 0; i < 0x20; i++) {
			;
		}
		retryCount--;
	}
	return ( FALSE );
}

/******************************************************************************
 ** Function name:		MCI_Send_Stop
 **
 ** Descriptions:		CMD12, STOP_TRANSMISSION. if that happens, the card is 
 **						maybe in a unknown state that need a warm reset.		
 **
 ** parameters:			None
 ** Returned value:		true or false, true if, at least, the card status
 **						shows ready bit is set.
 ** 
 ******************************************************************************/
DWORD MCI_Send_Stop(void) {
	volatile DWORD i; // mthomas - volatile
	DWORD retryCount;
	DWORD respStatus;
	DWORD respValue[4];

	retryCount = 0x20;
	while (retryCount > 0) {
		MCI_CLEAR = 0x7FF;
		MCI_SendCmd( STOP_TRANSMISSION, 0x00000000, EXPECT_SHORT_RESP, 0);
		respStatus = MCI_GetCmdResp( STOP_TRANSMISSION, EXPECT_SHORT_RESP, (DWORD *)respValue);
		/* ready bit, bit 8, should be set in the card status register */
		if ( !respStatus && (respValue[0] & (1 << 8))) {
			return ( TRUE );
		}
		for (i = 0; i < 0x20; i++) {
			;
		}
		retryCount--;
	}
	return ( FALSE );
}

/******************************************************************************
 ** Function name:		MCI_Send_Write_Block
 **
 ** Descriptions:		CMD24, WRITE_BLOCK, send this cmd in the TRANS state
 **						to write a block of data to the card.
 **
 ** parameters:			block number
 ** Returned value:		Response value
 ** 
 ** modified by mthomas
 ******************************************************************************/
// mthomas - static
static DWORD MCI_Send_Write_Block(DWORD blockNum) {
	volatile DWORD i; // mthomas - volatile
	DWORD retryCount;
	DWORD respStatus;
	DWORD respValue[4];

	retryCount = 0x20;
	while (retryCount > 0) {
		MCI_CLEAR = 0x7FF;
#if 1
		// mthomas
		if (MCI_CardType == SDHC_CARD) {
			MCI_SendCmd( WRITE_BLOCK, blockNum, EXPECT_SHORT_RESP, 0);
		} else {
			MCI_SendCmd( WRITE_BLOCK, blockNum * BLOCK_LENGTH, EXPECT_SHORT_RESP, 0);
		}
#else
		// NXP original
		MCI_SendCmd( WRITE_BLOCK, blockNum * BLOCK_LENGTH, EXPECT_SHORT_RESP, 0 );
#endif
		respStatus = MCI_GetCmdResp( WRITE_BLOCK, EXPECT_SHORT_RESP, (DWORD *)&respValue[0]);
		/* it should be in the transfer state, bit 9~12 is 0x0100 and bit 8 is 1 */
		if ( !respStatus && ((respValue[0] & (0x0F << 8)) == 0x0900)) {
			return ( TRUE ); /* ready and in TRAN state */
		}

		for (i = 0; i < 0x20; i++) {
			;
		}
		retryCount--;
	}
	return ( FALSE ); /* Fatal error */
}

/******************************************************************************
 ** Function name:		MCI_Send_Write_Multiple_Block
 **
 ** Descriptions:		CMD25, WRITE_MULTIPLE_BLOCK , send this cmd in the 
 **						TRANS state to write several blocks of data to the card.
 **
 ** parameters:			block number
 ** Returned value:		Response value
 ** 
 ** Added by Lorenz Aebi
 ******************************************************************************/
// mthomas - static
static DWORD MCI_Send_Write_Multiple_Block(DWORD blockNum) {
	volatile DWORD i; // mthomas - volatile
	DWORD retryCount;
	DWORD respStatus;
	DWORD respValue[4];

	retryCount = 0x20;
	while (retryCount > 0) {
		MCI_CLEAR = 0x7FF;
#if 1
		// mthomas
		if (MCI_CardType == SDHC_CARD) {
			MCI_SendCmd( WRITE_MULTIPLE_BLOCK, blockNum, EXPECT_SHORT_RESP, 0);
		} else {
			MCI_SendCmd( WRITE_MULTIPLE_BLOCK, blockNum * BLOCK_LENGTH, EXPECT_SHORT_RESP, 0);
		}
#else
		// NXP original
		MCI_SendCmd( WRITE_MULTIPLE_BLOCK, blockNum * BLOCK_LENGTH, EXPECT_SHORT_RESP, 0 );
#endif
		respStatus = MCI_GetCmdResp( WRITE_MULTIPLE_BLOCK, EXPECT_SHORT_RESP, (DWORD *)&respValue[0]);
		/* it should be in the transfer state, bit 9~12 is 0x0100 and bit 8 is 1 */
		if ( !respStatus && ((respValue[0] & (0x0F << 8)) == 0x0900)) {
			return ( TRUE ); /* ready and in TRAN state */
		}

		for (i = 0; i < 0x20; i++) {
			;
		}
		retryCount--;
	}
	return ( FALSE ); /* Fatal error */
}

/******************************************************************************
 ** Function name:		MCI_Send_Read_Block
 **
 ** Descriptions:		CMD17, READ_SINGLE_BLOCK, send this cmd in the TRANS 
 **						state to read a block of data from the card.
 **
 ** parameters:			block number
 ** Returned value:		Response value
 **
 ** modified by mthomas
 ******************************************************************************/
// mthomas - static
static DWORD MCI_Send_Read_Block(DWORD blockNum) {
	volatile DWORD i; // mthomas - volatile
	DWORD retryCount;
	DWORD respStatus;
	DWORD respValue[4];

	retryCount = 0x20;
	while (retryCount > 0) {
		MCI_CLEAR = 0x7FF;
#if 1
		// mthomas
		if (MCI_CardType == SDHC_CARD) {
			MCI_SendCmd( READ_SINGLE_BLOCK, blockNum, EXPECT_SHORT_RESP, 0);
		} else {
			MCI_SendCmd( READ_SINGLE_BLOCK, blockNum * BLOCK_LENGTH, EXPECT_SHORT_RESP, 0);
		}
#else
		// NXP original
		MCI_SendCmd( READ_SINGLE_BLOCK, blockNum * BLOCK_LENGTH, EXPECT_SHORT_RESP, 0 );
#endif
		respStatus = MCI_GetCmdResp( READ_SINGLE_BLOCK, EXPECT_SHORT_RESP, (DWORD *)&respValue[0]);
		/* it should be in the transfer state, bit 9~12 is 0x0100 and bit 8 is 1 */
		if ( !respStatus && ((respValue[0] & (0x0F << 8)) == 0x0900)) {
			return ( TRUE ); /* ready and in TRAN state */
		}
		for (i = 0; i < 0x20; i++) {
			;
		}
		retryCount--;
	}
	return ( FALSE ); /* Fatal error */
}

/******************************************************************************
 ** Function name:		MCI_Write_Block
 **
 ** Descriptions:		Set MCI data control register, data length and data
 **						timeout, send WRITE_BLOCK cmd, finally, enable
 **						interrupt. On completion of WRITE_BLOCK cmd, TX_ACTIVE
 **						interrupt will occurs, data can be written continuously
 **						into the FIFO until the block data length is reached.		
 **
 ** parameters:			block number
 ** Returned value:		true or false, if cmd times out, return false and no 
 **						need to continue.
 ** 
 ******************************************************************************/
DWORD MCI_Write_Block(DWORD blockNum) {
	volatile DWORD i; // mthomas - volatile
	DWORD DataCtrl = 0;

	MCI_CLEAR = 0x7FF;
	MCI_DATA_CTRL = 0;
	for (i = 0; i < 0x10; i++) {
		;
	}

	/* Below status check is redundant, but ensure card is in TRANS state
	 before writing and reading to from the card. */
	if (MCI_CheckStatus() != TRUE) {
		mci_debug_printf("MCI_Wead_Block: not in trans\n");
		MCI_Send_Stop();
		return ( FALSE );
	}

	MCI_DATA_TMR = DATA_TIMER_VALUE;
	MCI_DATA_LEN = BLOCK_LENGTH;
	MCI_Block_End_Flag = 1;
	MCI_TXEnable();
	if (MCI_Send_Write_Block(blockNum) == FALSE) {
		return ( FALSE );
	}

#if MCI_DMA_ENABLED
	DMA_Move( 0, M2P);
	GPDMA_CH0_CFG |= 0x10001 | (0x00 << 1) | (0x04 << 6) | (0x05 << 11);
	/* Write, block transfer, DMA, and data length */
	DataCtrl |= ((1 << 0) | (1 << 3) | (DATA_BLOCK_LEN << 4));
#else
	/* Write, block transfer, and data length */
	DataCtrl |= ((1 << 0) | (DATA_BLOCK_LEN << 4));
#endif
	MCI_DATA_CTRL = DataCtrl;
	//laebi - not necessary
	for (i = 0; i < 0x10; i++) {
		;
	}

	return ( TRUE );
}

/******************************************************************************
 ** Function name:		MCI_Write_Multiple_Block
 **
 ** Descriptions:		Set MCI data control register, data length and data
 **						timeout, send WRITE_MULTIPLE_BLOCK cmd. Continuously
 **						writes blocks of data. At the end a STOP_TRANSMISSION
 **						marks the end of the datatransfer. 
 ** 							
 **
 ** parameters:			block number
 **						number of blocks
 **						first address to the data in the memory
 ** Returned value:		true or false, if cmd times out, return false and no 
 **						need to continue.
 **
 ** Added by Lorenz Aebi
 ******************************************************************************/
DWORD MCI_Write_Multiple_Block(DWORD blockNum, BYTE count, const BYTE *buff) {
	volatile DWORD i; // mthomas - volatile
	DWORD DataCtrl = 0;

	MCI_CLEAR = 0x7FF;
	MCI_DATA_CTRL = 0;
	//laebi - not necessary
	//for ( i = 0; i < 0x10; i++ ) { ; }

	/* Below status check is redundant, but ensure card is in TRANS state
	 before writing and reading to from the card. */
	if (MCI_CheckStatus() != TRUE) {
		mci_debug_printf("MCI_Wead_Block: not in trans\n");
		MCI_Send_Stop();
		return ( FALSE );
	}

	MCI_DATA_TMR = DATA_TIMER_VALUE;
	MCI_DATA_LEN = BLOCK_LENGTH;
	MCI_Block_End_Flag = 1;
	MCI_TXEnable();
	if (MCI_Send_Write_Multiple_Block(blockNum) == FALSE) {
		return ( FALSE );
	}

#if MCI_DMA_ENABLED
	do {
		// TODO: avoid copy
		memcpy((void *)DMA_SRC, buff, 512);
		buff += 512;
		GPDMA_INT_TCCLR = 0x01;
		GPDMA_INT_ERR_CLR = 0x01;
		GPDMA_CH0_SRC = DMA_SRC;
		GPDMA_CH0_DEST = DMA_MCIFIFO;
		GPDMA_CONFIG = 0x01;
		/* Enable DMA channels, little endian */
		while ( !(GPDMA_CONFIG & 0x01))
			;
		/* The burst size is set to 8, the size is 8 bit too. */
		/* Terminal Count Int enable */
		GPDMA_CH0_CTRL = (DMA_SIZE & 0x0FFF) | (0x04 << 12) | (0x02 << 15)
		| (0x02 << 18) | (0x02 << 21) | (1 << 26) | 0x80000000;
		//DMA_Move( 0, M2P );
		GPDMA_CH0_CFG |= 0x10001 | (0x00 << 1) | (0x04 << 6) | (0x05 << 11);
		/* Write, block transfer, DMA, and data length */
		DataCtrl |= ((1 << 0) | (1 << 3) | (DATA_BLOCK_LEN << 4));
#else
		/* Write, block transfer, and data length */
		DataCtrl |= ((1 << 0) | (DATA_BLOCK_LEN << 4));
#endif
		MCI_DATA_CTRL = DataCtrl;
		for (i = 0; i < 0x10; i++) {
			;
		}
	} while (count--);

	//for (i = 0; i < 0x10; i++) { ; }

	MCI_Send_Stop();

	return ( TRUE );
}

/******************************************************************************
 ** Function name:		MCI_Read_Block
 **
 ** Descriptions:		Set MCI data control register, data length and data
 **						timeout, send READ_SINGLE_BLOCK cmd, finally, enable
 **						interrupt. On completion of READ_SINGLE_BLOCK cmd, 
 **						RX_ACTIVE interrupt will occurs, data can be read 
 **						continuously into the FIFO until the block data 
 **						length is reached.		
 **
 ** parameters:			block number
 ** Returned value:		true or false, if cmd times out, return false and no 
 **						need to continue.
 **
 ******************************************************************************/
DWORD MCI_Read_Block(DWORD blockNum) {
	volatile DWORD i; // mthomas - volatile
	DWORD DataCtrl = 0;

	MCI_CLEAR = 0x7FF;
	MCI_DATA_CTRL = 0;
	for (i = 0; i < 0x10; i++) {
		;
	}

	/* Below status check is redundant, but ensure card is in TRANS state
	 before writing and reading to from the card. */
	if (MCI_CheckStatus() != TRUE) {
		mci_debug_printf("MCI_Read_Block: not in trans\n");
		MCI_Send_Stop();
		return ( FALSE );
	}
	MCI_RXEnable();

	MCI_DATA_TMR = DATA_TIMER_VALUE;
	MCI_DATA_LEN = BLOCK_LENGTH;
	MCI_Block_End_Flag = 1;
	if (MCI_Send_Read_Block(blockNum) == FALSE) {
		return ( FALSE );
	}

#if MCI_DMA_ENABLED
	DMA_Move( 1, P2M);
	GPDMA_CH1_CFG |= 0x10001 | (0x04 << 1) | (0x00 << 6) | (0x06 << 11);
	/* Write, block transfer, DMA, and data length */
	DataCtrl |= ((1 << 0) | (1 << 1) | (1 << 3) | (DATA_BLOCK_LEN << 4));
#else
	/* Read, enable, block transfer, and data length */
	DataCtrl |= ((1 << 0) | (1 << 1) | (DATA_BLOCK_LEN << 4));
#endif
	MCI_DATA_CTRL = DataCtrl;
	for (i = 0; i < 0x10; i++) {
		;
	}

	return ( TRUE );
}

/******************************************************************************
 ** Function name:		MCI_Sd_Status
 **
 ** Descriptions:		reads the 512bit SD_STATUS (3-22) using ACMD13 (4-37)
 **						Card must be in trans-state for this (4-7)
 **						( SD-Status in ReadBlock when MCI_Block_End_Flag==1 )
 **
 ** parameters:			none
 ** Returned value:		true or false 
 ** 
 ** Added by Martin Thomas
 ******************************************************************************/
DWORD MCI_Sd_Status(void) {
	volatile DWORD i;
	DWORD retryCount;
	DWORD respStatus;
	DWORD respValue[4];
	DWORD stuffbits = 0x00000000;
	DWORD DataCtrl = 0;

	if ( (MCI_CardType == CARD_UNKNOWN ) || (MCI_CardType == MMC_CARD )) {
		return ( FALSE ); // unsupported
	}

	MCI_CLEAR = 0x7FF;
	MCI_DATA_CTRL = 0;
	for (i = 0; i < 0x10; i++) {
		;
	}

	/* Below status check is redundant, but ensure card is in TRANS state
	 before writing and reading to from the card. */
	if (MCI_CheckStatus() != TRUE) {
		MCI_Send_Stop();mci_debug_printf("Sd_Status: Not in trans-state\n");
		return ( FALSE );
	}

	MCI_RXEnable();

	MCI_DATA_TMR = DATA_TIMER_VALUE;
	MCI_DATA_LEN = BLOCK_LENGTH;
	// MCI_DATA_LEN = SD_BLOCK_LENGTH;
	MCI_Block_End_Flag = 1;
	SDStatRead = 1;

	// "MCI_Send_SD_STATUS":
	retryCount = 0x20;
	while (retryCount > 0) {
		MCI_CLEAR = 0x7FF;
		// prepare for ACMD (with CMD55)
		if (MCI_Send_ACMD() == TRUE) {
			// Send ACMD13
			MCI_SendCmd( SD_STATUS, stuffbits, EXPECT_SHORT_RESP, 0);
			respStatus = MCI_GetCmdResp( SD_STATUS, EXPECT_SHORT_RESP, (DWORD *)&respValue[0]);
			/* it should be in the transfer state, bit 9~12 is 0x0100 and bit 8 is 1 */
			if ( !respStatus && ((respValue[0] & (0x0F << 8)) == 0x0900)) {
				break; /* ready and in TRAN state */
			}
		}
		for (i = 0; i < 0x20; i++) {
			;
		}
		retryCount--;
	}
	if (retryCount == 0) {
		mci_debug_printf("Sd_Status: ACMD13 failed\n");
		return ( FALSE ); // error sending ACMD13 or state not trans
	}

#if MCI_DMA_ENABLED
	DMA_Move( 1, P2M);
	GPDMA_CH1_CFG |= 0x10001 | (0x04 << 1) | (0x00 << 6) | (0x06 << 11);
	/* Write, block transfer, DMA, and data length */
	DataCtrl |= ((1 << 0) | (1 << 1) | (1 << 3) | (SD_STATUS_BLOCK_LEN << 4));
#else
	/* Read, enable, block transfer, and data length */
	DataCtrl |= ((1 << 0) | (1 << 1) | ( SD_STATUS_BLOCK_LEN << 4 ));
#endif
	MCI_DATA_CTRL = DataCtrl;
	// mci_debug_printf("MCI_SD_Status: DATA_CTRL set\n");
	for (i = 0; i < 0x10; i++) {
		;
	}

	return ( TRUE );
}

/******************************************************************************
 ** Function name:		MCI_Power_Off
 **
 ** Descriptions:		disable power to MCI (Power-Reg)
 **
 ** parameters:			none
 ** Returned value:		none
 ** 
 ** Added by Martin Thomas - but it does not work on MCB2300
 ******************************************************************************/
void MCI_Power_Off(void) {
	volatile DWORD i;

	// SCS |= (1<<8);
	MCI_POWER = 0;
	for (i = 0; i < 0x100; i++)
		;mci_debug_printf( "MCI_POWER 0x%08x\n", MCI_POWER );
}

/*****************************************************************************
 **                            End Of File
 ******************************************************************************/

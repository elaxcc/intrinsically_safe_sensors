/*****************************************************************************
 *   mcitest.c:  main C entry file for NXP LPC23xx/24xx Family Microprocessors
 *
 *   Copyright(C) 2006, NXP Semiconductor
 *   All rights reserved.
 *
 *   History
 *   2006.07.20  ver 1.00    Prelimnary version, first Release
 *
******************************************************************************/

/* 
	Modified and extended by Martin Thomas
	http://www.siwawi.arubi.uni-kl.de/avr_projects
*/

#include "LPC23xx.h"                       /* LPC23xx/24xx definitions */
#include "type.h"
#include "irq.h"
#include "mci.h"
#include "dma.h"
#include <stdio.h>

extern       void init_serial    (void);

extern volatile DWORD MCI_CardType;
extern volatile DWORD MCI_Block_End_Flag;

extern volatile BYTE *WriteBlock, *ReadBlock;


static volatile void dump_status( const char* info )
{
	DWORD st;
	DWORD currst;

	const char* states[] = {
		"idle", "ready", "ident", "stby", 
		"tran", "data", "rcv", "prg", "dis"
	};

	st = MCI_Send_Status();
	if ( st == INVALID_RESPONSE ) {
		printf("dump_status: Send_Status INVALID_RESPONSE\n");
		return;
	}

	printf("Status register %s\n", info);
	printf("AKE_SEQ_ERROR    : %s\n",  (st & (1 <<  3)) ? "error"     : "no error" );
	printf("APP_CMD          : %s\n",        (st & (1 <<  5)) ? "enabled"   : "disabled" );
	printf("READY_FOR_DATA   : %s\n", (st & (1 <<  8)) ? "not ready" : "ready"    );

	currst = (st >> 9) & 0x0f;
	if ( currst > 8 ) {
		printf("CURR_STATE       : reserved\n");
	}
	else {
		printf("CURR_STATE       : %s   <---\n", states[currst]);
	}
	// TODO bits 13-18
	printf("ERROR            : %s\n", (st & (1<<19)) ? "error"  : "no error" );
	printf("CC_ERROR         : %s\n", (st & (1<<20)) ? "error"  : "no error" );
	printf("CARD_ECC_FAILED  : %s\n", (st & (1<<21)) ? "failure": "success"  );
	printf("ILLEGAL_COMMAND  : %s\n", (st & (1<<22)) ? "error"  : "no error" );
	printf("COM_CRC_ERROR    : %s\n", (st & (1<<23)) ? "error"  : "no error" );
	printf("LOCK_UNLOCK_FAIL : %s\n", (st & (1<<24)) ? "error"  : "no error" );
	printf("CARD_IS_LOCKED   : %s\n", (st & (1<<25)) ? "locked" : "unlocked" );
	// TODO bits 26-31
}

static void dump_csd(void)
{
	BYTE cardtype_save;
	DWORD respValue[4];

	cardtype_save = MCI_CardType;
	// CMD7: trans to stby (RCA 0) / de-select
	MCI_CardType = 0;
	if ( MCI_Select_Card() == TRUE ) {
		dump_status("after de-select");
	}
	else {
		printf("dump_csd: de-select failed\n");
		return;
	}
	
	// CMD9: SEND_SID
	if ( MCI_Send_CSD( respValue ) == TRUE ) {
		// dump_status("after Send_CSD");
		printf(" CSD: %08x %08x %08x %08x\n", 
			respValue[0], respValue[1],
			respValue[2], respValue[3]
		);
	}
	else {
		printf("dump_cid: Send_CID failed\n");
	}

	// CMD7: stby to trans (RCA set) / select
	MCI_CardType = cardtype_save;
	if ( MCI_Select_Card() == TRUE ) {
		dump_status("after select");
	}
	else {
		printf("dump_cid: select failed\n");
	}
}

// Dumps the SD-Status (ACMD13) (this is not the status)
void dump_sdstat(void)
{
	DWORD i,j;

	// dump_status("before ACMD13");
	if ( MCI_Sd_Status() == FALSE ) {
		printf("dump_sdstat: Sd_Status failed\n");
		return;
	}
	// dump_status("after ACMD13");
	while ( MCI_Block_End_Flag == 1 ) ; // TODO: timeout
	// dump_status("after wait data");

	printf("SD-Status:\n");
	j=0;
	for (i=0; i<64; i++ ) {
		printf("%02i:%02x ", i, ReadBlock[i] );
		if ( j++ == 10 ) {
			printf("\n");
			j = 0;
		}
	}
	printf("\n");

	printf("DAT_BUS_WIDTH : %02x = ", ReadBlock[0]>>6 );
	switch ( ReadBlock[0]>>6 ) {
		case 0:
			printf("1 (default)");
			break;
		case 1:
			printf("reserved1");
			break;
		case 2:
			printf("4-bit width");
			break;
		case 3:
			printf("reserved2");
			break;
	}
	printf("\n");
}

/******************************************************************************
**   Main Function  main()
******************************************************************************/
int main (void)
{
	DWORD i, j, k;
	
	init_serial();
	printf("Hello from the extended NXP MCI example (extended by Martin Thomas)\n");

	for ( k = 0; k < 3; k++ ) {
		printf("---- Running Test %d ----\n", k+1);


	/* Fill block data pattern in write buffer and clear everything 
	in read buffer. */
	for ( i = 0; i < BLOCK_LENGTH; i++ )
	{
		WriteBlock[i] = i;
		ReadBlock[i] = 0;
	}
#if MCI_DMA_ENABLED
	/* on DMA channel 0, source is memory, destination is MCI FIFO. */
	/* On DMA channel 1, source is MCI FIFO, destination is memory. */
	DMA_Init();
	printf("DMA enabled\n");
#endif
	
	/* For the SD card I tested, the minimum required block length is 512 */
	/* For MMC, the restriction is loose, due to the variety of SD and MMC
	card support, ideally, the driver should read CSD register to find the 
	right speed and block length for the card, and set them accordingly.
	In this driver example, it will support both MMC and SD cards, and it
	does read the information by send SEND_CSD to poll the card status,
	however, to simplify the example, it doesn't configure them accordingly
	based on the CSD register value. This is not intended to support all 
	the SD and MMC cards. */
	if ( MCI_Init() != TRUE )
	{
		printf("MCI_Init failed\n");
		while( 1 );			/* fatal error */
	}
	
	MCI_CardType = MCI_CardInit();
	
	if ( MCI_CardType == CARD_UNKNOWN )
	{
		printf("unknown card\n");
		while ( 1 );		/* fatal error */
	}
	
	if (MCI_Check_CID() != TRUE)
	{
		printf("Check_CID failed\n");
		while ( 1 );		/* fatal error */
	}
	
	if ( MCI_Set_Address() != TRUE )
	{
		printf("Set_Address failed\n");
		while ( 1 );		/* fatal error */
	}
	
	if ( MCI_Send_CSD( NULL ) != TRUE )
	{
		printf("Send_CSD failed\n");
		while ( 1 );		/* fatal error */
	}
	
	if ( MCI_Select_Card() != TRUE )
	{
		printf("Select_Card failed\n");
		while ( 1 );		/* fatal error */
	}
	
	if ( MCI_CardType == SD_CARD )
	{
		MCI_Set_MCIClock( NORMAL_RATE );
#if 1
		if (SD_Set_BusWidth( SD_4_BIT ) != TRUE )
		{
			printf("set 4 bit mode failed\n");
			while ( 1 );	/* fatal error */
		}
#endif
	}
	
	if ( MCI_Set_BlockLen( BLOCK_LENGTH ) != TRUE )
	{
		printf("Set_BlockLen failed\n");
		while ( 1 );		/* fatal error */
	}

	// dump_status("after init");
	
	/***************************************************************/
	/* TEST - Write and Read number of blocks defined by BLOCK_NUM */
	/***************************************************************/

	printf("Test start\n");

	for ( i = 0; i < BLOCK_NUM; i++ )
	{
		printf("Block %d ...", i);

		if ( MCI_Write_Block( i ) != TRUE )
		{
			printf("Write_Block failed\n");
			while ( 1 );		/* Fatal error */
		}
		/* When MCI_Block_End_Flag is clear, it indicates
		   Write_Block is complete, next task, Read_Block to check write */
		while ( MCI_Block_End_Flag == 1 ) ;
	
		if ( MCI_Read_Block( i ) != TRUE )
		{
			printf("Read_Block failed\n");
			while ( 1 );		/* Fatal error */
		}

		/* When MCI_Block_End_Flag is clear, it indicates RX is done 
		   with Read_Block,  validation of RX and TX buffers next. */
		while ( MCI_Block_End_Flag == 1 ) ;
	
		for ( j = 0; j < (BLOCK_LENGTH); j++ )
		{
			if ( WriteBlock[j] != ReadBlock[j] )
			{
				printf("Compare failed\n");
				while ( 1 );	/* Data comparison failure, fatal error */
			}
		}
		printf("o.k.\n");
	
		/*  clear read buffer for next block comparison */
		for ( j = 0; j < (BLOCK_LENGTH); j++ )
		{
			ReadBlock[j] = 0;
		}

		if ( i == 2 ) {
			// dump_status("before dump_csd");
			dump_csd();
			dump_sdstat();
		}
	}

	printf("Success\n");

	} // test loops

	while (1);

	// return 0;
}

/******************************************************************************
**                            End Of File
******************************************************************************/

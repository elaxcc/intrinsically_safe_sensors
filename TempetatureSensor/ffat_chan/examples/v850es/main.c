/*---------------------------------------------------------------*/
/* FAT file system module test program R0.05      (C)ChaN, 2007  */
/*---------------------------------------------------------------*/


#include <string.h>
#include "comm.h"
#include "monitor.h"
#include "diskio.h"
#include "ff.h"


int _rcopy(unsigned long*, long);
extern unsigned long _S_romp;

DWORD acc_size;				/* Work register for fs command */
WORD acc_files, acc_dirs;
FILINFO finfo;

char linebuf[120];			/* Console input buffer */

FATFS fatfs[_DRIVES];		/* File system object for each logical drive */
BYTE Buff[4096];			/* Working buffer */


volatile UINT Timer;		/* 1kHz increment timer */

volatile BYTE rtcYear = 106, rtcMon = 10, rtcMday = 12, rtcHour, rtcMin, rtcSec;




/*---------------------------------------------------------*/
/* 1000Hz timer interrupt generated by OC2                 */
/*---------------------------------------------------------*/


#pragma interrupt INTTM0EQ0 ISR_tmm0
__interrupt void ISR_tmm0 (void)
{
	static const BYTE samurai[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	static UINT div;
	BYTE n;


	Timer++;			/* Performance counter for this module */
	disk_timerproc();	/* Drive timer procedure of low level disk I/O module */

	P9 = div;

	/* Real Time Clock */
	if (++div >= 1000) {
		div = 0;
		if (++rtcSec >= 60) {
			rtcSec = 0;
			if (++rtcMin >= 60) {
				rtcMin = 0;
				if (++rtcHour >= 24) {
					rtcHour = 0;
					n = samurai[rtcMon - 1];
					if ((n == 28) && !(rtcYear & 3)) n++;
					if (++rtcMday > n) {
						rtcMday = 1;
						if (++rtcMon > 12) {
							rtcMon = 1;
							rtcYear++;
						}
					}
				}
			}
		}
	}
}



/*---------------------------------------------------------*/
/* User Provided Timer Function for FatFs module           */
/*---------------------------------------------------------*/
/* This is a real time clock service to be called from     */
/* FatFs module. Any valid time must be returned even if   */
/* the system does not support a real time clock.          */


DWORD get_fattime (void)
{
	DWORD tmr;


	__DI();
	tmr =	  (((DWORD)rtcYear - 80) << 25)
			| ((DWORD)rtcMon << 21)
			| ((DWORD)rtcMday << 16)
			| (WORD)(rtcHour << 11)
			| (WORD)(rtcMin << 5)
			| (WORD)(rtcSec >> 1);
	__EI();

	return tmr;
}




/*--------------------------------------------------------------------------*/
/* Monitor                                                                  */


static
FRESULT scan_files (char* path)
{
	DIR dirs;
	FRESULT res;
	BYTE i;


	if ((res = f_opendir(&dirs, path)) == FR_OK) {
		i = strlen(path);
		while (((res = f_readdir(&dirs, &finfo)) == FR_OK) && finfo.fname[0]) {
			if (finfo.fattrib & AM_DIR) {
				acc_dirs++;
				*(path+i) = '/'; strcpy(path+i+1, &finfo.fname[0]);
				res = scan_files(path);
				*(path+i) = '\0';
				if (res != FR_OK) break;
			} else {
				acc_files++;
				acc_size += finfo.fsize;
			}
		}
	}

	return res;
}



static
void put_rc (FRESULT rc)
{
	const char *p;
	static const char str[] =
		"OK\0" "NOT_READY\0" "NO_FILE\0" "FR_NO_PATH\0" "INVALID_NAME\0" "INVALID_DRIVE\0"
		"DENIED\0" "EXIST\0" "RW_ERROR\0" "WRITE_PROTECTED\0" "NOT_ENABLED\0"
		"NO_FILESYSTEM\0" "INVALID_OBJECT\0" "MKFS_ABORTED\0";
	FRESULT i;

	for (p = str, i = 0; i != rc && *p; i++) {
		while(*p++);
	}
	xprintf("\nrc=%u FR_%s", (UINT)rc, p);
}



static
void IoInit ()
{
	PRCMD = 0; OCDM = 0;	/* Disable Debugging I/F */
	RCM = 1; WDTM2 = 0;		/* Disable WDT */

	VSWC = 1;				/* Set wait state for peripherals */
	do; while(LOCKR);		/* Select clock source: PLLx4 */
	PLLCTL = 3;
	PRCMD = 0; PCC = 0;

	_rcopy(&_S_romp, -1);	/* Initialize .data sections */


	/* Initialize GPIO ports */
	PM7L = 0x01;
	PM7H = 0x00;
	P0 =  0x40;
	PM0 = 0x10;

	PM9 = 0x0000;
	PMDL = 0x0000;
	PMDH = 0x00;
	PMCCM = 0x02;			/* Enable CLKOUT */


	/* Start TM0 in interval time of 1ms */
	TM0CMP0 = SYSCLK / 1000 - 1;
	TM0CTL0 = 0x80;
	TM0EQMK0 = 0;

	uart_init();		/* Initialize UART driver */

	__EI();

}



/*-----------------------------------------------------------------------*/
/* Main                                                                  */


int main ()
{
	char *ptr, *ptr2;
	long p1, p2, p3;
	BYTE res, b1;
	WORD w1;
	UINT s1, s2, cnt, blen = sizeof(Buff);
	DWORD ofs = 0, sect = 0;
	FATFS *fs;				/* Pointer to file system object */
	DIR dir;				/* Directory object */
	FIL file1, file2;		/* File objects */


	IoInit();


	xputs("\nFatFs module test monitor for V850ES");

	for (;;) {
		xputs("\n>");
		ptr = linebuf;
		get_line(ptr, sizeof(linebuf));

		switch (*ptr++) {

		case 'd' :
			switch (*ptr++) {
			case 'd' :	/* dd <phy_drv#> [<sector>] - Dump secrtor */
				if (!xatoi(&ptr, &p1)) break;
				if (!xatoi(&ptr, &p2)) p2 = sect;
				res = disk_read(p1, Buff, p2, 1);
				if (res) { xprintf("\nrc=%d", (WORD)res); break; }
				sect = p2 + 1;
				xprintf("\nSector:%lu", p2);
				for (ptr=(char*)Buff, ofs = 0; ofs < 0x200; ptr+=16, ofs+=16)
					put_dump((BYTE*)ptr, ofs, 16);
				break;

			case 'i' :	/* di <phy_drv#> - Initialize disk */
				if (!xatoi(&ptr, &p1)) break;
				xprintf("\nrc=%d", (WORD)disk_initialize((BYTE)p1));
				break;

			case 's' :	/* ds <phy_drv#> - Show disk status */
				if (!xatoi(&ptr, &p1)) break;
				if (disk_ioctl((BYTE)p1, GET_SECTOR_COUNT, &p2) == RES_OK)
					{ xprintf("\nDrive size: %lu sectors", p2); }
				if (disk_ioctl((BYTE)p1, GET_SECTOR_SIZE, &w1) == RES_OK)
					{ xprintf("\nSector size: %u", w1); }
				if (disk_ioctl((BYTE)p1, GET_BLOCK_SIZE, &p2) == RES_OK)
					{ xprintf("\nErase block size: %lu sectors", p2); }
				if (disk_ioctl((BYTE)p1, MMC_GET_TYPE, &b1) == RES_OK)
					{ xprintf("\nMMC/SDC type: %u", b1); }
				if (disk_ioctl((BYTE)p1, MMC_GET_CSD, Buff) == RES_OK)
					{ xputs("\nCSD:"); put_dump(Buff, 0, 16); }
				if (disk_ioctl((BYTE)p1, MMC_GET_CID, Buff) == RES_OK)
					{ xputs("\nCID:"); put_dump(Buff, 0, 16); }
				if (disk_ioctl((BYTE)p1, MMC_GET_OCR, Buff) == RES_OK)
					{ xputs("\nOCR:"); put_dump(Buff, 0, 4); }
				if (disk_ioctl((BYTE)p1, MMC_GET_SDSTAT, Buff) == RES_OK) {
					xputs("\nSD Status:");
					for (s1 = 0; s1 < 64; s1 += 16) put_dump(Buff+s1, s1, 16);
				}
				if (disk_ioctl((BYTE)p1, ATA_GET_MODEL, linebuf) == RES_OK)
					{ linebuf[40] = '\0'; xprintf("\nModel: %s", linebuf); }
				if (disk_ioctl((BYTE)p1, ATA_GET_SN, linebuf) == RES_OK)
					{ linebuf[20] = '\0'; xprintf("\nS/N: %s", linebuf); }
				break;
			}
			break;

		case 'b' :
			switch (*ptr++) {
			case 'd' :	/* bd <addr> - Dump R/W buffer */
				if (!xatoi(&ptr, &p1)) break;
				for (ptr=(char*)&Buff[p1], ofs = p1, cnt = 32; cnt; cnt--, ptr+=16, ofs+=16)
					put_dump((BYTE*)ptr, ofs, 16);
				break;

			case 'e' :	/* be <addr> [<data>] ... - Edit R/W buffer */
				if (!xatoi(&ptr, &p1)) break;
				if (xatoi(&ptr, &p2)) {
					do {
						Buff[p1++] = (BYTE)p2;
					} while (xatoi(&ptr, &p2));
					break;
				}
				for (;;) {
					xprintf("\n%04X %02X-", (WORD)(p1), (WORD)Buff[p1]);
					get_line(linebuf, sizeof(linebuf));
					ptr = linebuf;
					if (*ptr == '.') break;
					if (*ptr < ' ') { p1++; continue; }
					if (xatoi(&ptr, &p2))
						Buff[p1++] = (BYTE)p2;
					else
						xputs("\n???");
				}
				break;

			case 'r' :	/* br <phy_drv#> <sector> [<n>] - Read disk into R/W buffer */
				if (!xatoi(&ptr, &p1)) break;
				if (!xatoi(&ptr, &p2)) break;
				if (!xatoi(&ptr, &p3)) p3 = 1;
				xprintf("\nrc=%u", (WORD)disk_read((BYTE)p1, Buff, p2, p3));
				break;

			case 'w' :	/* bw <phy_drv#> <sector> [<n>] - Write R/W buffer into disk */
				if (!xatoi(&ptr, &p1)) break;
				if (!xatoi(&ptr, &p2)) break;
				if (!xatoi(&ptr, &p3)) p3 = 1;
				xprintf("\nrc=%u", (WORD)disk_write((BYTE)p1, Buff, p2, p3));
				break;

			case 'f' :	/* bf <n> - Fill working buffer */
				if (!xatoi(&ptr, &p1)) break;
				memset(Buff, (BYTE)p1, sizeof(Buff));
				break;

			}
			break;

		case 'f' :
			switch (*ptr++) {

			case 'i' :	/* fi <log drv#> - Force initialized the logical drive */
				if (!xatoi(&ptr, &p1)) break;
				put_rc(f_mount((BYTE)p1, &fatfs[p1]));
				break;

			case 's' :	/* fs [<path>] - Show logical drive status */
				res = f_getfree(ptr, (DWORD*)&p2, &fs);
				if (res) { put_rc(res); break; }
				xprintf("\nFAT type = %u\nBytes/Cluster = %lu\nNumber of FATs = %u\n"
						"Root DIR entries = %u\nSectors/FAT = %lu\nNumber of clusters = %lu\n"
						"FAT start (lba) = %lu\nDIR start (lba,clustor) = %lu\nData start (lba) = %lu\n",
						(WORD)fs->fs_type, (DWORD)fs->sects_clust * 512, (WORD)fs->n_fats,
						fs->n_rootdir, fs->sects_fat, (DWORD)fs->max_clust - 2,
						fs->fatbase, fs->dirbase, fs->database
				);
				acc_size = acc_files = acc_dirs = 0;
				res = scan_files(ptr);
				if (res) { put_rc(res); break; }
				xprintf("\n%u files, %lu bytes.\n%u folders.\n"
						"%lu KB total disk space.\n%lu KB available.",
						acc_files, acc_size, acc_dirs,
						(fs->max_clust - 2) * (fs->sects_clust / 2), p2 * (fs->sects_clust / 2)
				);
				break;

			case 'l' :	/* fl [<path>] - Directory listing */
				res = f_opendir(&dir, ptr);
				if (res) { put_rc(res); break; }
				p1 = s1 = s2 = 0;
				for(;;) {
					res = f_readdir(&dir, &finfo);
					if ((res != FR_OK) || !finfo.fname[0]) break;
					if (finfo.fattrib & AM_DIR) {
						s2++;
					} else {
						s1++; p1 += finfo.fsize;
					}
					xprintf("\n%c%c%c%c%c %u/%02u/%02u %02u:%02u %9lu  %s", 
							(finfo.fattrib & AM_DIR) ? 'D' : '-',
							(finfo.fattrib & AM_RDO) ? 'R' : '-',
							(finfo.fattrib & AM_HID) ? 'H' : '-',
							(finfo.fattrib & AM_SYS) ? 'S' : '-',
							(finfo.fattrib & AM_ARC) ? 'A' : '-',
							(finfo.fdate >> 9) + 1980, (finfo.fdate >> 5) & 15, finfo.fdate & 31,
							(finfo.ftime >> 11), (finfo.ftime >> 5) & 63,
							finfo.fsize, &(finfo.fname[0]));
				}
				xprintf("\n%4u File(s),%10lu bytes total\n%4u Dir(s)", s1, p1, s2);
				if (f_getfree(ptr, (DWORD*)&p1, &fs) == FR_OK)
					xprintf(", %10lu bytes free", p1 * fs->sects_clust * 512);
				break;

			case 'o' :	/* fo <mode> <file> - Open a file */
				if (!xatoi(&ptr, &p1)) break;
				put_rc(f_open(&file1, ptr, (BYTE)p1));
				break;

			case 'c' :	/* fc - Close a file */
				put_rc(f_close(&file1));
				break;

			case 'e' :	/* fe - Seek file pointer */
				if (!xatoi(&ptr, &p1)) break;
				res = f_lseek(&file1, p1);
				put_rc(res);
				if (res == FR_OK)
					xprintf("\nfptr = %lu(0x%lX)", file1.fptr, file1.fptr);
				break;

			case 'r' :	/* fr <len> - read file */
				if (!xatoi(&ptr, &p1)) break;
				p2 = 0;
				Timer = 0;
				while (p1) {
					if ((UINT)p1 >= blen)	{ cnt = blen; p1 -= blen; }
					else 					{ cnt = p1; p1 = 0; }
					res = f_read(&file1, Buff, cnt, &s2);
					if (res != FR_OK) { put_rc(res); break; }
					p2 += s2;
					if (cnt != s2) break;
				}
				xprintf("\n%lu bytes read with %lu kB/sec.", p2, p2 / Timer);
				break;

			case 'd' :	/* fd <len> - read and dump file from current fp */
				if (!xatoi(&ptr, &p1)) break;
				ofs = file1.fptr;
				while (p1) {
					if ((UINT)p1 >= 16) { cnt = 16; p1 -= 16; }
					else 				{ cnt = p1; p1 = 0; }
					res = f_read(&file1, Buff, cnt, &cnt);
					if (res != FR_OK) { put_rc(res); break; }
					if (!cnt) break;
					put_dump(Buff, ofs, cnt);
					ofs += 16;
				}
				break;

			case 'w' :	/* fw <len> <val> - write file */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2)) break;
				memset(Buff, (BYTE)p2, blen);
				p2 = 0;
				Timer = 0;
				while (p1) {
					if ((UINT)p1 >= blen) { cnt = blen; p1 -= blen; }
					else 				  { cnt = p1; p1 = 0; }
					res = f_write(&file1, Buff, cnt, &s2);
					if (res != FR_OK) { put_rc(res); break; }
					p2 += s2;
					if (cnt != s2) break;
				}
				xprintf("\n%lu bytes written with %lu kB/sec.", p2, p2 / Timer);
				break;

			case 'n' :	/* fn <old_name> <new_name> - Change file/dir name */
				while (*ptr == ' ') ptr++;
				ptr2 = strchr(ptr, ' ');
				if (!ptr2) break;
				*ptr2++ = 0;
				while (*ptr2 == ' ') ptr2++;
				put_rc(f_rename(ptr, ptr2));
				break;

			case 'u' :	/* fu <name> - Unlink a file or dir */
				put_rc(f_unlink(ptr));
				break;

			case 'k' :	/* fk <name> - Create a directory */
				put_rc(f_mkdir(ptr));
				break;

			case 'a' :	/* fa <atrr> <mask> <name> - Change file/dir attribute */
				if (!xatoi(&ptr, &p1) || !xatoi(&ptr, &p2)) break;
				put_rc(f_chmod(ptr, p1, p2));
				break;

			case 'x' : /* fx <src_name> <dst_name> - Copy file */
				while (*ptr == ' ') ptr++;
				ptr2 = strchr(ptr, ' ');
				if (!ptr2) break;
				*ptr2++ = 0;
				while (*ptr2 == ' ') ptr2++;
				xprintf("\nOpening \"%s\"", ptr);
				res = f_open(&file1, ptr, FA_OPEN_EXISTING | FA_READ);
				if (res) {
					put_rc(res);
					break;
				}
				xprintf("\nCreating \"%s\"", ptr2);
				res = f_open(&file2, ptr2, FA_CREATE_ALWAYS | FA_WRITE);
				if (res) {
					put_rc(res);
					f_close(&file1);
					break;
				}
				xprintf("\nCopying...");
				p1 = 0;
				for (;;) {
					res = f_read(&file1, Buff, sizeof(Buff), &s1);
					if (res || s1 == 0) break;   /* error or eof */
					res = f_write(&file2, Buff, s1, &s2);
					p1 += s2;
					if (res || s2 < s1) break;   /* error or disk full */
				}
				xprintf("\n%lu bytes copied.", p1);
				f_close(&file1);
				f_close(&file2);
				break;
#if _USE_MKFS != 0
			case 'm' :	/* fm <log drv#> <partition rule> <cluster size> - Create file system */
				if (!xatoi(&ptr, &p1)) break;
				if (!xatoi(&ptr, &p2)) break;
				if (!xatoi(&ptr, &p3)) break;
				xprintf("\nThe drive %u will be formatted. Are you sure? (Y/n)=", p1);
				get_line(ptr, sizeof(linebuf));
				if (*ptr != 'Y') break;
				put_rc(f_mkfs((BYTE)p1, (BYTE)p2, (WORD)p3));
				break;
#endif
			case 'z' :	/* fz <len> - set transfer unit for fr/fw commands */
				if (xatoi(&ptr, &p1) && (p1 > 0) && (p1 <= sizeof(Buff)))
					blen = p1;
				xprintf("\nlen=%u", blen);
				break;
			}
			break;

		case 't' :	/* t [<year> <mon> <mday> <hour> <min> <sec>] */
			if (xatoi(&ptr, &p1)) {
				rtcYear = p1-1900;
				xatoi(&ptr, &p1); rtcMon = p1-1;
				xatoi(&ptr, &p1); rtcMday = p1;
				xatoi(&ptr, &p1); rtcHour = p1;
				xatoi(&ptr, &p1); rtcMin = p1;
				if(!xatoi(&ptr, &p1)) break;
				rtcSec = p1;
			}
			xprintf("\n%u/%u/%u %02u:%02u:%02u", rtcYear+1900, rtcMon+1, rtcMday, rtcHour, rtcMin, rtcSec);
			break;
		}
	}

}


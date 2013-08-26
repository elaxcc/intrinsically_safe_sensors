FatFs/Tiny-FatFs Module Sample Projects                    (C)ChaN, 2007


DIRECTORIES

  <avr>    The master sample project for FatFs/Tiny-FatFs with:
           ATMEL AVR (ATmega64,8-bit,RISC) with MMC, CompactFlash and ATA HDD.

  <h8>     A sample project for Tiny-FatFs with:
           Renesas H8/300H (HD64F3694,16-bit,CISC) with MMC.

  <tlcs>   A sample project for Tiny-FatFs with:
           TOSHIBA TLCS-870/C (TMP86FM29,8-bit,CISC) with MMC.

  <v850>   A sample project for FatFs with:
           NEC V850ES (UPD70F3716,32-bit,RISC) with MMC.

  These are sample projects for function/compatibility test of FatFs/Tiny-
  FatFs module with low level disk I/O codes. You will able to find various
  implementations on the web other than these samples. The existing projects
  are for SH2, LPC2k, MSP430, PIC and Z8 at least, so far as I know.



AGREEMENTS

  These sample projects for FatFs/Tiny-FatFs module are free software and
  there is no warranty. You can use, modify and/or redistribute it for
  personal, non-profit or profit use without any restriction under your
  responsibility.



REVISION HISTORY

  Apr 29, 2006  First release.
  Aug 19, 2006  MMC module: Fixed a bug that disk_initialize() never time-out
                when card does not go ready.
  Oct 12, 2006  CF module: Fixed a bug that disk_initialize() can fail at 3.3V.
  Oct 22, 2006  Added a sample project for V850ES.
  Feb 04, 2007  All modules: Modified for FatFs module R0.04.
                MMC module: Fixed a bug that disk_ioctl() returns incorrect disk size.
  Apr 03, 2007  All modules: Modified for FatFs module R0.04a.
                MMC module: Supported high capacity SD memory cards.
  May 05, 2007  MMC modules: Fixed a bug that GET_SECTOR_COUNT via disk_ioctl() failes on MMC.
  Aug 26, 2007  Added some ioctl sub-functions.
 
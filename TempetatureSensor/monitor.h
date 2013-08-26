#ifndef MONITOR_H_
#define MONITOR_H_

#include "integer.h"
#include "comm.h"

#define xputc(chr)	uart_put((BYTE)(chr))
#define xgetc()		(char)uart_get()

int xatoi (/*mth const*/ char**, long*);
void xputs (const char*);
void xitoa (signed long, signed char, signed char);
void xprintf (const char*, ...);
void put_dump (const BYTE*, DWORD ofs, int cnt);
void get_line (char*, int len);

#endif

/* 
	gettime for Chan's fat from LPC RTC
	Martin Thomas 9/2007
*/

#include "type.h"
#include "rtc.h"

DWORD get_fattime (void); /* avoid warning */
DWORD get_fattime (void)
{
	RTCTime t;
	DWORD res;

	RTCGetTime( &t );
	res = (
		(( t.RTC_Sec/2 )       << 0UL  ) |
		(( t.RTC_Min )         << 5UL  ) |
		(( t.RTC_Hour )        << 11UL ) |
		(( t.RTC_Mday )        << 16UL ) |
		(( t.RTC_Mon )         << 21UL ) |
		(( t.RTC_Year-1980 )   << 25UL )
	);

	return res;
}

/*
from Chan's documentation:

get_fattime
The get_fattime function gets current time.

DWORD get_fattime (void);

Return Value
Currnet time is returned with packed into a DWORD value. The bit field is as follows:

bit31:25 
Year from 1980 (0..127) 

bit24:21 
Month (1..12) 

bit20:16 
Date (1..31) 

bit15:11 
Hour (0..23) 

bit10:5 
Minute (0..59) 

bit4:0 
Second/2 (0..29) 

Description
The get_fattime function must return any valid time even if the system does not support a real time clock. This fucntion is not required in read only configuration.

*/

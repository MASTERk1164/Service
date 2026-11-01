"""Exercise production local-time parsing, including midnight and fractions."""
from pathlib import Path
import subprocess,tempfile
source=Path('src/main.c').read_text()
logic=source[source.index('static int readTime('):source.index('static void defaultPreTimes(')]
stub=r'''
#define _GNU_SOURCE
#include <time.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef int BOOL;typedef unsigned UINT;typedef unsigned short WORD;typedef uint32_t DWORD;typedef uint64_t ULONGLONG;typedef int HWND;
#define FALSE 0
#define TRUE 1
#define GDT_VALID 0
typedef struct{WORD wYear,wMonth,wDayOfWeek,wDay,wHour,wMinute,wSecond,wMilliseconds;} SYSTEMTIME;
typedef struct{DWORD dwLowDateTime,dwHighDateTime;} FILETIME;
static int win,timebox=1;static SYSTEMTIME values[4];static UINT fractions[4];static BOOL valid=1;
static int GetDlgCtrlID(HWND h){return h;}
static UINT GetDlgItemInt(HWND h,int id,BOOL *ok,BOOL sign){(void)h;(void)sign;if(ok)*ok=valid;return fractions[id==109?1:id];}
static int DateTime_GetSystemtime(HWND h,SYSTEMTIME *t){*t=values[h];return GDT_VALID;}
static int SystemTimeToFileTime(const SYSTEMTIME *s,FILETIME *f){struct tm t={0};t.tm_year=s->wYear-1900;t.tm_mon=s->wMonth-1;t.tm_mday=s->wDay;t.tm_hour=s->wHour;t.tm_min=s->wMinute;t.tm_sec=s->wSecond;ULONGLONG v=((ULONGLONG)timegm(&t)+11644473600ULL)*10000000ULL+s->wMilliseconds*10000ULL;f->dwLowDateTime=(DWORD)v;f->dwHighDateTime=(DWORD)(v>>32);return 1;}
static void FileTimeToSystemTime(const FILETIME *f,SYSTEMTIME *s){ULONGLONG v=((ULONGLONG)f->dwHighDateTime<<32)|f->dwLowDateTime;time_t sec=(time_t)(v/10000000ULL-11644473600ULL);struct tm *t=gmtime(&sec);s->wYear=t->tm_year+1900;s->wMonth=t->tm_mon+1;s->wDay=t->tm_mday;s->wHour=t->tm_hour;s->wMinute=t->tm_min;s->wSecond=t->tm_sec;s->wMilliseconds=(WORD)((v%10000000ULL)/10000ULL);}
static int TzSpecificLocalTimeToSystemTime(void *zone,const SYSTEMTIME *s,SYSTEMTIME *out){(void)zone;*out=*s;return 1;}
'''
tests=r'''
int main(void){
 SYSTEMTIME day={2026,11,0,1,0,0,0,0};ULONGLONG main,pre;
 values[1].wHour=20;values[2].wHour=19;values[2].wMinute=59;values[2].wSecond=59;fractions[2]=50;
 assert(readTime(1,1,day,&main,FALSE)&&readTime(2,2,day,&pre,TRUE));assert(main-pre==5000000);
 fractions[2]=0;assert(readTime(2,2,day,&pre,TRUE)&&main-pre==10000000);
 values[1].wHour=0;values[2].wHour=23;day.wMonth=1;day.wDay=1;
 assert(readTime(1,1,day,&main,FALSE)&&readTime(2,2,day,&pre,TRUE)&&main-pre==10000000);
 values[2].wHour=0;values[2].wMinute=0;values[2].wSecond=0;
 assert(readTime(2,2,day,&pre,TRUE)&&pre==main);
 fractions[2]=100;assert(!readTime(2,2,day,&pre,TRUE));fractions[2]=0;valid=0;assert(!readTime(2,2,day,&pre,TRUE));
 puts("PASS: 59:59.00 / 59:59.50, midnight across year boundary, equal time, invalid fraction");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'input.c';p.write_text(stub+logic+tests)
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror',str(p),'-o',str(Path(d)/'test')],check=True)
 subprocess.run([str(Path(d)/'test')],check=True)

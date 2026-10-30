"""Run the production scheduler against a deterministic Windows API stub."""
from pathlib import Path
import subprocess
import tempfile
source = Path('src/main.c').read_text()
logic = source[source.index('static int waitUntil('):source.index('static HWND control(')]
stub = r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint64_t ULONGLONG; typedef unsigned DWORD; typedef void *LPVOID;
typedef struct {int x,y;} POINT;
typedef struct {int type;struct {int dwFlags;} mi;} INPUT;
#define WINAPI
#define WAIT_OBJECT_0 0
#define INPUT_MOUSE 0
#define MOUSEEVENTF_LEFTDOWN 1
#define MOUSEEVENTF_LEFTUP 2
#define DONE 1
static int win,cancelEvent,earlyClick,repeatCount,delayMs,stopAfterFirst,cancelled,done;
static ULONGLONG target,now,clicks[30];static int count;
static ULONGLONG ticks(void){return now;}
static int WaitForSingleObject(int event,DWORD ms){(void)event;if(cancelled)return 0;now+=(ULONGLONG)ms*10000;return 258;}
static void SetCursorPos(int x,int y){assert(x==10&&y==20);}
static void GetCursorPos(POINT *p){p->x=10;p->y=20;}
static unsigned SendInput(unsigned n,INPUT *input,int size){(void)input;(void)size;clicks[count++]=now;if(stopAfterFirst)cancelled=1;return n;}
static void PostMessage(int w,int m,int result,int l){(void)w;(void)m;(void)l;done=result;}
'''
tests = r'''
static void reset(void){now=0;target=10000000;count=0;cancelled=0;done=-1;stopAfterFirst=0;delayMs=50;repeatCount=3;}
int main(void){
 reset();earlyClick=1;run(NULL);assert(done==0&&count==5);assert(clicks[0]==9500000);for(int i=1;i<5;i++)assert(clicks[i]==target+(i-1)*500000);
 reset();earlyClick=0;run(NULL);assert(done==0&&count==4&&clicks[0]==target);
 reset();earlyClick=1;repeatCount=0;run(NULL);assert(count==2&&clicks[1]==target);
 reset();earlyClick=1;stopAfterFirst=1;run(NULL);assert(done==1&&count==1);
 reset();earlyClick=1;now=target;run(NULL);assert(count==4&&clicks[0]==target);
 reset();earlyClick=1;now=target+30000000;run(NULL);assert(done==3&&count==0);
 reset();earlyClick=1;target=103700000;run(NULL);assert(done==0&&count==5&&clicks[0]==103200000&&clicks[1]==103700000);
 reset();earlyClick=0;repeatCount=0;target=100100000;run(NULL);assert(count==1&&clicks[0]==target);
 reset();earlyClick=0;repeatCount=0;target=109900000;run(NULL);assert(count==1&&clicks[0]==target);
 puts("PASS: early/main timing, counts, cancellation, missed early slot, stale deadline");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'timing.c';p.write_text(stub+logic+tests)
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror',str(p),'-o',str(Path(d)/'test')],check=True)
 subprocess.run([str(Path(d)/'test')],check=True)

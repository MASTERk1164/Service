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
typedef int BOOL;
#define TRUE 1
#define FALSE 0
typedef uint64_t ULONGLONG; typedef unsigned DWORD; typedef void *LPVOID;
typedef struct {int x,y;} POINT;
typedef struct {int type;struct {int dwFlags;} mi;} INPUT;
#define WINAPI
#define WAIT_OBJECT_0 0
#define INPUT_MOUSE 0
#define MOUSEEVENTF_LEFTDOWN 1
#define MOUSEEVENTF_LEFTUP 2
#define DONE 1
static int win,cancelEvent,repeatCount,delayMs,stopAfterFirst,cancelled,done;
static ULONGLONG target,preTarget[2],now,clicks[64];static int count;static BOOL preEnabled[2];
static ULONGLONG ticks(void){return now;}
static int WaitForSingleObject(int event,DWORD ms){(void)event;if(cancelled)return 0;now+=(ULONGLONG)ms*10000;return 258;}
static void SetCursorPos(int x,int y){assert(x==10&&y==20);}
static int GetCursorPos(POINT *p){p->x=10;p->y=20;return 1;}
static unsigned SendInput(unsigned n,INPUT *input,int size){(void)input;(void)size;clicks[count++]=now;if(stopAfterFirst)cancelled=1;return n;}
static void PostMessage(int w,int m,int result,int l){(void)w;(void)m;(void)l;done=result;}
'''
tests = r'''
static void reset(void){now=0;target=20000000;preTarget[0]=10000000;preTarget[1]=15000000;preEnabled[0]=preEnabled[1]=0;count=0;cancelled=0;done=-1;stopAfterFirst=0;delayMs=50;repeatCount=3;}
int main(void){
 reset();run(NULL);assert(done==0&&count==4&&clicks[0]==target);
 reset();preEnabled[0]=preEnabled[1]=1;run(NULL);assert(done==0&&count==6&&clicks[0]==10000000&&clicks[1]==15000000);for(int i=2;i<6;i++)assert(clicks[i]==target+(i-2)*500000);
 reset();preEnabled[1]=1;repeatCount=0;run(NULL);assert(done==0&&count==2&&clicks[0]==preTarget[1]&&clicks[1]==target);
 reset();preEnabled[0]=preEnabled[1]=1;preTarget[0]=17000000;preTarget[1]=12000000;run(NULL);assert(count==6&&clicks[0]==12000000&&clicks[1]==17000000);
 reset();preEnabled[0]=preEnabled[1]=1;preTarget[1]=preTarget[0];run(NULL);assert(count==6&&clicks[0]==clicks[1]);
 reset();preEnabled[0]=preEnabled[1]=1;stopAfterFirst=1;run(NULL);assert(done==1&&count==1);
 reset();cancelled=1;run(NULL);assert(done==1&&count==0);
 reset();preEnabled[0]=preEnabled[1]=1;repeatCount=20;run(NULL);assert(done==0&&count==23);
 reset();preEnabled[0]=1;preTarget[0]=1;now=30000000;target=50000000;run(NULL);assert(done==3&&count==4&&clicks[0]==target);
 reset();target=103700000;repeatCount=0;run(NULL);assert(count==1&&clicks[0]==target);
 puts("PASS: two single pre-clicks, main repeats unchanged, disabled slots, reordered/equal times, Esc, stale slot, hundredths");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'timing.c';p.write_text(stub+logic+tests)
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror',str(p),'-o',str(Path(d)/'test')],check=True)
 subprocess.run([str(Path(d)/'test')],check=True)

"""Run production pre-time autofill against deterministic UI and calendar stubs."""
from pathlib import Path
import ast,subprocess,tempfile
source=Path('src/main.c').read_text()
logic=source[source.index('static void defaultPreTimes('):source.index('static void arm(')]
module=ast.parse(Path('tests/test-input-time.py').read_text())
stub=next(ast.literal_eval(n.value) for n in module.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='stub' for t in n.targets))
# Keep only the APIs used by autofill, with unused stub APIs marked for this harness.
stub=stub.replace('static int GetDlgCtrlID','static __attribute__((unused)) int GetDlgCtrlID').replace('static int TzSpecificLocalTimeToSystemTime','static __attribute__((unused)) int TzSpecificLocalTimeToSystemTime')
stub+=r'''
#include <wchar.h>
#include <stdlib.h>
typedef wchar_t WCHAR;
static HWND date=3,pretime[2]={2,0},prefraction[2]={2,0};
static BOOL preEdited[2],updating;static WCHAR rendered[4][3];
static void DateTime_SetSystemtime(HWND h,int flag,const SYSTEMTIME *t){(void)flag;values[h]=*t;}
static void SetWindowText(HWND h,const WCHAR *s){wcscpy(rendered[h],s);fractions[h]=(UINT)wcstoul(s,NULL,10);}
'''
tests=r'''
int main(void){
 values[3]=(SYSTEMTIME){2026,11,0,1,0,0,0,0};values[1].wHour=20;
 defaultPreTimes();assert(values[2].wHour==19&&values[2].wMinute==59&&values[2].wSecond==59);assert(wcscmp(rendered[2],L"00")==0&&wcscmp(rendered[0],L"50")==0);
 preEdited[0]=1;values[2].wSecond=58;values[1].wHour=21;defaultPreTimes();assert(values[2].wSecond==58&&values[0].wHour==20);
 preEdited[0]=0;defaultPreTimes();assert(values[2].wHour==20&&values[2].wSecond==59);
 fractions[1]=37;defaultPreTimes();assert(fractions[2]==37&&fractions[0]==87);
 fractions[1]=0;values[1].wHour=0;values[3]=(SYSTEMTIME){2027,1,0,1,0,0,0,0};defaultPreTimes();assert(values[2].wYear==2026&&values[2].wMonth==12&&values[2].wDay==31&&values[2].wHour==23);assert(fractions[2]==0&&fractions[0]==50&&!updating);
 puts("PASS: production autofill .00/.50, manual edits preserved, refresh, fractional main, midnight rollover");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'auto.c';p.write_text(stub+logic+tests)
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror',str(p),'-o',str(Path(d)/'test')],check=True)
 subprocess.run([str(Path(d)/'test')],check=True)

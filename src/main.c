#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#define DONE (WM_APP+1)
static HWND win,date,timebox,retries,interval,start,stop,status;
static HANDLE cancelEvent,worker;
static HFONT font;
static ULONGLONG target;
static int repeatCount,delayMs;
static BOOL hotkey;
static ULONGLONG ticks(void){ FILETIME f; GetSystemTimeAsFileTime(&f); return ((ULONGLONG)f.dwHighDateTime<<32)|f.dwLowDateTime; }
static DWORD WINAPI run(LPVOID unused){
    (void)unused;
    for(;;){
        ULONGLONG now=ticks();
        if(now>=target){ if(now-target>20000000ULL){ PostMessage(win,DONE,3,0); return 0; } break; }
        DWORD wait=(DWORD)((target-now)/10000ULL);
        if(wait>25)wait=25;
        if(wait<1)wait=1;
        if(WaitForSingleObject(cancelEvent,wait)==WAIT_OBJECT_0){PostMessage(win,DONE,1,0);return 0;}
    }
    POINT p; GetCursorPos(&p);
    for(int i=0;i<=repeatCount;i++){
        if(WaitForSingleObject(cancelEvent,0)==WAIT_OBJECT_0){PostMessage(win,DONE,1,0);return 0;}
        SetCursorPos(p.x,p.y);
        INPUT in[2]={0}; in[0].type=in[1].type=INPUT_MOUSE;
        in[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN; in[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
        if(SendInput(2,in,sizeof(INPUT))!=2){
            INPUT up={0};up.type=INPUT_MOUSE;up.mi.dwFlags=MOUSEEVENTF_LEFTUP;SendInput(1,&up,sizeof(INPUT));
            PostMessage(win,DONE,2,0);return 0;
        }
        if(i<repeatCount && WaitForSingleObject(cancelEvent,(DWORD)delayMs)==WAIT_OBJECT_0){PostMessage(win,DONE,1,0);return 0;}
    }
    PostMessage(win,DONE,0,0); return 0;
}
static HWND control(LPCWSTR cls,LPCWSTR txt,DWORD style,int x,int y,int w,int h,int id){
    HWND c=CreateWindowEx(0,cls,txt,WS_CHILD|WS_VISIBLE|style,x,y,w,h,win,(HMENU)(INT_PTR)id,GetModuleHandle(NULL),NULL);
    SendMessage(c,WM_SETFONT,(WPARAM)font,TRUE);return c;
}
static void enabled(BOOL ready){EnableWindow(date,ready);EnableWindow(timebox,ready);EnableWindow(retries,ready);EnableWindow(interval,ready);EnableWindow(start,ready);EnableWindow(stop,!ready);}
static void arm(void){
    SYSTEMTIME d,t,utc;FILETIME f;
    DateTime_GetSystemtime(date,&d);DateTime_GetSystemtime(timebox,&t);
    d.wHour=t.wHour;d.wMinute=t.wMinute;d.wSecond=t.wSecond;d.wMilliseconds=0;
    if(!TzSpecificLocalTimeToSystemTime(NULL,&d,&utc)||!SystemTimeToFileTime(&utc,&f)){MessageBox(win,L"날짜와 시간을 확인하세요.",L"입력 확인",MB_OK);return;}
    target=((ULONGLONG)f.dwHighDateTime<<32)|f.dwLowDateTime;
    BOOL ok1,ok2;repeatCount=(int)GetDlgItemInt(win,103,&ok1,FALSE);delayMs=(int)GetDlgItemInt(win,104,&ok2,FALSE);
    if(!ok1||!ok2||repeatCount>20||delayMs<10||delayMs>1000){MessageBox(win,L"추가 클릭은 0~20회, 간격은 10~1000ms로 입력하세요.",L"입력 확인",MB_OK);return;}
    if(target<=ticks()){MessageBox(win,L"미래의 날짜와 시간을 선택하세요.",L"입력 확인",MB_OK);return;}
    hotkey=RegisterHotKey(win,1,MOD_NOREPEAT,VK_ESCAPE);
    if(!hotkey){MessageBox(win,L"Esc 취소 키를 등록하지 못했습니다. 다른 프로그램이 Esc 단축키를 사용 중인지 확인하세요.",L"예약 실패",MB_OK|MB_ICONERROR);return;}
    ResetEvent(cancelEvent);enabled(FALSE);SetWindowText(status,L"예약 대기 중 · 클릭할 곳에 마우스를 놓으세요.\r\nEsc 키를 누르면 즉시 취소합니다.");
    worker=CreateThread(NULL,0,run,NULL,0,NULL);
    if(!worker){UnregisterHotKey(win,1);hotkey=FALSE;enabled(TRUE);SetWindowText(status,L"실행 작업을 만들지 못했습니다. 다시 시도하세요.");}
}
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_CREATE:{
        win=h;font=CreateFont(18,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Malgun Gothic");
        control(L"STATIC",L"예약 클릭",0,24,18,450,30,0);
        control(L"STATIC",L"이 PC의 날짜·시간 기준 (초 단위)",0,24,60,450,24,0);
        date=control(DATETIMEPICK_CLASS,L"",DTS_SHORTDATEFORMAT,24,92,220,32,101);
        timebox=control(DATETIMEPICK_CLASS,L"",DTS_TIMEFORMAT|DTS_UPDOWN,260,92,220,32,102);
        DateTime_SetFormat(timebox,L"HH':'mm':'ss");
        SYSTEMTIME now;GetLocalTime(&now);DateTime_SetSystemtime(date,GDT_VALID,&now);DateTime_SetSystemtime(timebox,GDT_VALID,&now);
        control(L"STATIC",L"추가 클릭 (0~20회)",0,24,146,220,24,0);
        retries=control(L"EDIT",L"3",WS_BORDER|ES_NUMBER|WS_TABSTOP,24,175,220,30,103);
        control(L"STATIC",L"클릭 간격 (10~1000ms)",0,260,146,230,24,0);
        interval=control(L"EDIT",L"50",WS_BORDER|ES_NUMBER|WS_TABSTOP,260,175,220,30,104);
        control(L"STATIC",L"예약 시각의 마우스 위치를 고정해 왼쪽 클릭합니다.\r\n추가 클릭은 첫 클릭의 성공 여부와 관계없이 실행됩니다.\r\n중복 클릭을 원하지 않으면 추가 클릭을 0으로 설정하세요.",0,24,225,460,82,0);
        start=control(L"BUTTON",L"예약 시작",WS_TABSTOP|BS_DEFPUSHBUTTON,24,325,220,42,105);
        stop=control(L"BUTTON",L"취소 (Esc)",WS_TABSTOP,260,325,220,42,106);EnableWindow(stop,FALSE);
        status=control(L"STATIC",L"날짜·시간을 선택하고 예약 시작을 누르세요.\r\n대기 중에는 PC를 켜 두고 잠금·절전 상태를 해제하세요.",0,24,390,460,65,0);
        cancelEvent=CreateEvent(NULL,TRUE,FALSE,NULL);return 0;}
    case WM_COMMAND:if(LOWORD(w)==105)arm();if(LOWORD(w)==106)SetEvent(cancelEvent);return 0;
    case WM_HOTKEY:if(w==1)SetEvent(cancelEvent);return 0;
    case DONE:
        if(worker){WaitForSingleObject(worker,INFINITE);CloseHandle(worker);worker=NULL;}
        if(hotkey){UnregisterHotKey(h,1);hotkey=FALSE;}enabled(TRUE);
        SetWindowText(status,w==0?L"예약 클릭을 완료했습니다.":w==1?L"예약을 취소했습니다.":w==3?L"예약 시각을 2초 이상 지나 클릭하지 않았습니다.\r\n절전 상태와 PC 시간을 확인하고 다시 예약하세요.":L"Windows가 클릭 입력을 허용하지 않았습니다.\r\n대상 프로그램의 권한과 잠금 상태를 확인하세요.");return 0;
    case WM_CLOSE:SetEvent(cancelEvent);if(worker){WaitForSingleObject(worker,INFINITE);CloseHandle(worker);worker=NULL;}DestroyWindow(h);return 0;
    case WM_DESTROY:if(hotkey)UnregisterHotKey(h,1);CloseHandle(cancelEvent);DeleteObject(font);PostQuitMessage(0);return 0;
    }return DefWindowProc(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE prev,LPWSTR cmd,int show){
    (void)prev;(void)cmd;SetProcessDPIAware();INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_DATE_CLASSES};InitCommonControlsEx(&ic);
    WNDCLASS wc={0};wc.lpfnWndProc=proc;wc.hInstance=inst;wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hIcon=LoadIcon(NULL,IDI_APPLICATION);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);wc.lpszClassName=L"ScheduledClickWindow";RegisterClass(&wc);
    HWND h=CreateWindow(wc.lpszClassName,L"예약 클릭",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,525,510,NULL,NULL,inst,NULL);
    if(!h)return 1;
    ShowWindow(h,show);MSG msg;while(GetMessage(&msg,NULL,0,0)>0){if(!IsDialogMessage(h,&msg)){TranslateMessage(&msg);DispatchMessage(&msg);}}return 0;
}

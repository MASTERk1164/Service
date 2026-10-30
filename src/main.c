#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#define DONE (WM_APP+1)
static HWND win,date,timebox,retries,interval,start,stop,status,clockbox,earlybox;
static HANDLE cancelEvent,worker;
static HFONT font,clockFont;
static void updateClock(void){
    SYSTEMTIME now; WCHAR text[96]; GetLocalTime(&now);
    swprintf(text,96,L"%04u-%02u-%02u  %02u:%02u:%02u",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond);
    SetWindowText(clockbox,text);
}
static ULONGLONG target;
static int repeatCount,delayMs;
static BOOL hotkey,earlyClick;
static ULONGLONG ticks(void){ FILETIME f; GetSystemTimeAsFileTime(&f); return ((ULONGLONG)f.dwHighDateTime<<32)|f.dwLowDateTime; }
static int waitUntil(ULONGLONG deadline){
    for(;;){
        if(WaitForSingleObject(cancelEvent,0)==WAIT_OBJECT_0)return 1;
        ULONGLONG now=ticks();
        if(now>=deadline)return now-deadline>20000000ULL?3:0;
        DWORD wait=(DWORD)((deadline-now)/10000ULL);
        if(wait>25)wait=25;
        if(wait<1)wait=1;
        if(WaitForSingleObject(cancelEvent,wait)==WAIT_OBJECT_0)return 1;
    }
}
static int clickAt(POINT p){
    if(WaitForSingleObject(cancelEvent,0)==WAIT_OBJECT_0)return 1;
    SetCursorPos(p.x,p.y);
    INPUT in[2]={0};in[0].type=in[1].type=INPUT_MOUSE;
    in[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;in[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
    if(SendInput(2,in,sizeof(INPUT))!=2){
        INPUT up={0};up.type=INPUT_MOUSE;up.mi.dwFlags=MOUSEEVENTF_LEFTUP;SendInput(1,&up,sizeof(INPUT));return 2;
    }
    return 0;
}
static DWORD WINAPI run(LPVOID unused){
    (void)unused;
    ULONGLONG first=target-(earlyClick?(ULONGLONG)delayMs*10000ULL:0);
    int result=waitUntil(first);
    POINT p;
    if(result){PostMessage(win,DONE,result,0);return 0;}
    GetCursorPos(&p);
    if(earlyClick){
        /* Skip a missed early slot; keep the main click at the requested time. */
        if(ticks()<target)result=clickAt(p);
        if(result){PostMessage(win,DONE,result,0);return 0;}
    }
    for(int i=0;i<=repeatCount;i++){
        result=waitUntil(target+(ULONGLONG)i*(ULONGLONG)delayMs*10000ULL);
        if(!result)result=clickAt(p);
        if(result)break;
    }
    PostMessage(win,DONE,result,0);return 0;
}
static HWND control(LPCWSTR cls,LPCWSTR txt,DWORD style,int x,int y,int w,int h,int id){
    HWND c=CreateWindowEx(0,cls,txt,WS_CHILD|WS_VISIBLE|style,x,y,w,h,win,(HMENU)(INT_PTR)id,GetModuleHandle(NULL),NULL);
    SendMessage(c,WM_SETFONT,(WPARAM)font,TRUE);return c;
}
static void enabled(BOOL ready){EnableWindow(date,ready);EnableWindow(timebox,ready);EnableWindow(retries,ready);EnableWindow(interval,ready);EnableWindow(earlybox,ready);EnableWindow(start,ready);EnableWindow(stop,!ready);}
static void arm(void){
    SYSTEMTIME d,t,utc;FILETIME f;
    DateTime_GetSystemtime(date,&d);DateTime_GetSystemtime(timebox,&t);
    d.wHour=t.wHour;d.wMinute=t.wMinute;d.wSecond=t.wSecond;d.wMilliseconds=0;
    if(!TzSpecificLocalTimeToSystemTime(NULL,&d,&utc)||!SystemTimeToFileTime(&utc,&f)){MessageBox(win,L"날짜와 시간을 확인하세요.",L"입력 확인",MB_OK);return;}
    target=((ULONGLONG)f.dwHighDateTime<<32)|f.dwLowDateTime;
    BOOL ok1,ok2;repeatCount=(int)GetDlgItemInt(win,103,&ok1,FALSE);delayMs=(int)GetDlgItemInt(win,104,&ok2,FALSE);
    if(!ok1||!ok2||repeatCount>20||delayMs<10||delayMs>1000){MessageBox(win,L"추가 클릭은 0~20회, 간격은 10~1000ms로 입력하세요.",L"입력 확인",MB_OK);return;}
    earlyClick=SendMessage(earlybox,BM_GETCHECK,0,0)==BST_CHECKED;
    if(target-(earlyClick?(ULONGLONG)delayMs*10000ULL:0)<=ticks()){MessageBox(win,L"사전 클릭 시각을 포함해 미래의 날짜와 시간을 선택하세요.",L"입력 확인",MB_OK);return;}
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
        control(L"STATIC",L"현재 PC 시간",0,24,12,450,24,0);
        clockbox=control(L"STATIC",L"",SS_CENTER,24,38,456,42,107);
        clockFont=CreateFont(30,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Consolas");
        SendMessage(clockbox,WM_SETFONT,(WPARAM)clockFont,TRUE);
        updateClock();SetTimer(h,1,100,NULL);
        control(L"STATIC",L"이 PC의 날짜·시간 기준 (초 단위)",0,24,94,450,24,0);
        date=control(DATETIMEPICK_CLASS,L"",DTS_SHORTDATEFORMAT,24,126,220,32,101);
        timebox=control(DATETIMEPICK_CLASS,L"",DTS_TIMEFORMAT|DTS_UPDOWN,260,126,220,32,102);
        DateTime_SetFormat(timebox,L"HH':'mm':'ss");
        SYSTEMTIME now;GetLocalTime(&now);DateTime_SetSystemtime(date,GDT_VALID,&now);DateTime_SetSystemtime(timebox,GDT_VALID,&now);
        control(L"STATIC",L"추가 클릭 (0~20회)",0,24,180,220,24,0);
        retries=control(L"EDIT",L"3",WS_BORDER|ES_NUMBER|WS_TABSTOP,24,209,220,30,103);
        control(L"STATIC",L"클릭 간격 (10~1000ms)",0,260,180,230,24,0);
        interval=control(L"EDIT",L"50",WS_BORDER|ES_NUMBER|WS_TABSTOP,260,209,220,30,104);
        earlybox=control(L"BUTTON",L"지정 시각보다 클릭 간격만큼 먼저 1회 클릭",WS_TABSTOP|BS_AUTOCHECKBOX,24,250,460,30,108);
        control(L"STATIC",L"첫 클릭 시각의 마우스 위치를 고정해 왼쪽 클릭합니다.\r\n추가 클릭은 첫 클릭의 성공 여부와 관계없이 실행됩니다.\r\n중복 클릭을 원하지 않으면 추가 클릭을 0으로 설정하세요.",0,24,292,460,82,0);
        start=control(L"BUTTON",L"예약 시작",WS_TABSTOP|BS_DEFPUSHBUTTON,24,392,220,42,105);
        stop=control(L"BUTTON",L"취소 (Esc)",WS_TABSTOP,260,392,220,42,106);EnableWindow(stop,FALSE);
        status=control(L"STATIC",L"날짜·시간을 선택하고 예약 시작을 누르세요.\r\n대기 중에는 PC를 켜 두고 잠금·절전 상태를 해제하세요.",0,24,457,460,65,0);
        cancelEvent=CreateEvent(NULL,TRUE,FALSE,NULL);return 0;}
    case WM_TIMER:if(w==1)updateClock();return 0;
    case WM_COMMAND:if(LOWORD(w)==105)arm();if(LOWORD(w)==106)SetEvent(cancelEvent);return 0;
    case WM_HOTKEY:if(w==1)SetEvent(cancelEvent);return 0;
    case DONE:
        if(worker){WaitForSingleObject(worker,INFINITE);CloseHandle(worker);worker=NULL;}
        if(hotkey){UnregisterHotKey(h,1);hotkey=FALSE;}enabled(TRUE);
        SetWindowText(status,w==0?L"예약 클릭을 완료했습니다.":w==1?L"예약을 취소했습니다.":w==3?L"예약 시각을 2초 이상 지나 클릭하지 않았습니다.\r\n절전 상태와 PC 시간을 확인하고 다시 예약하세요.":L"Windows가 클릭 입력을 허용하지 않았습니다.\r\n대상 프로그램의 권한과 잠금 상태를 확인하세요.");return 0;
    case WM_CLOSE:SetEvent(cancelEvent);if(worker){WaitForSingleObject(worker,INFINITE);CloseHandle(worker);worker=NULL;}DestroyWindow(h);return 0;
    case WM_DESTROY:KillTimer(h,1);DeleteObject(clockFont);if(hotkey)UnregisterHotKey(h,1);CloseHandle(cancelEvent);DeleteObject(font);PostQuitMessage(0);return 0;
    }return DefWindowProc(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE prev,LPWSTR cmd,int show){
    (void)prev;(void)cmd;SetProcessDPIAware();INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_DATE_CLASSES};InitCommonControlsEx(&ic);
    WNDCLASS wc={0};wc.lpfnWndProc=proc;wc.hInstance=inst;wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hIcon=LoadIcon(inst,MAKEINTRESOURCE(1));wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);wc.lpszClassName=L"ScheduledClickWindow";RegisterClass(&wc);
    HWND h=CreateWindow(wc.lpszClassName,L"예약 클릭",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,525,583,NULL,NULL,inst,NULL);
    if(!h)return 1;
    ShowWindow(h,show);MSG msg;while(GetMessage(&msg,NULL,0,0)>0){if(!IsDialogMessage(h,&msg)){TranslateMessage(&msg);DispatchMessage(&msg);}}return 0;
}

#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#define DONE (WM_APP+1)
#define SYNC_DONE (WM_APP+2)
static ULONGLONG ticks(void);
#include "navyism_sync.h"
static HWND win,date,timebox,retries,interval,start,stop,status,clockbox,earlybox,hundredths,sourcebox,syncbutton,syncstatus,clocklabel;
static HANDLE cancelEvent,worker,syncWorker;
static LONGLONG serverOffset,sampleOffset;
static DWORD sampleRtt;
static BOOL useServer,syncValid,sampleOk;
static ULONGLONG syncedAt;
static HFONT font,clockFont;
static void updateClock(void){
    SYSTEMTIME now; WCHAR text[96];
    ULONGLONG value=ticks()+(useServer&&syncValid?serverOffset:0);
    FILETIME utc={(DWORD)value,(DWORD)(value>>32)},local;
    FileTimeToLocalFileTime(&utc,&local);FileTimeToSystemTime(&local,&now);
    swprintf(text,96,L"%04u-%02u-%02u  %02u:%02u:%02u",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond);
    SetWindowText(clockbox,text);
}
static ULONGLONG target,pcTarget;
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
/* Each clock owns a complete click sequence. Merge deadlines so overlapping
   sequences never delay or replace each other. Equal deadlines keep both clicks. */
static DWORD WINAPI run(LPVOID unused){
    (void)unused;
    ULONGLONG bases[2]={target,pcTarget};
    int next[2]={earlyClick?-1:0,earlyClick?-1:0};
    int groups=useServer?2:1,result=0;
    POINT p={0}; int captured=0;
    for(;;){
        int group=-1; ULONGLONG deadline=0;
        for(int g=0;g<groups;g++){
            if(next[g]>repeatCount)continue;
            ULONGLONG slot=next[g]<0?bases[g]-(ULONGLONG)delayMs*10000ULL:
                bases[g]+(ULONGLONG)next[g]*(ULONGLONG)delayMs*10000ULL;
            if(group<0||slot<deadline){group=g;deadline=slot;}
        }
        if(group<0)break;
        int current=waitUntil(deadline);
        if(current==1){result=1;break;}
        if(current){result=current;next[group]=repeatCount+1;continue;}
        if(next[group]<0&&ticks()>=bases[group]){next[group]++;continue;}
        if(!captured){
            if(!GetCursorPos(&p)){result=2;break;}
            captured=1;
        }
        current=clickAt(p);
        if(current==1){result=1;break;}
        if(current){result=current;next[group]=repeatCount+1;continue;}
        next[group]++;
    }
    PostMessage(win,DONE,result,0);return 0;
}
static DWORD WINAPI synchronize(LPVOID unused){
    (void)unused;sampleOk=downloadNavy(&sampleOffset,&sampleRtt);
    PostMessage(win,SYNC_DONE,0,0);return 0;
}
static void syncNow(void){
    if(syncWorker||worker)return;
    syncValid=FALSE;SetWindowText(clocklabel,L"PC 시간 · NOL 동기화 중");EnableWindow(start,FALSE);EnableWindow(syncbutton,FALSE);
    SetWindowText(syncstatus,L"네이비즘 시간 확인 중...");
    syncWorker=CreateThread(NULL,0,synchronize,NULL,0,NULL);
    if(!syncWorker){SetWindowText(syncstatus,L"동기화 작업을 시작하지 못했습니다.");EnableWindow(syncbutton,TRUE);}
}
static HWND control(LPCWSTR cls,LPCWSTR txt,DWORD style,int x,int y,int w,int h,int id){
    HWND c=CreateWindowEx(0,cls,txt,WS_CHILD|WS_VISIBLE|style,x,y,w,h,win,(HMENU)(INT_PTR)id,GetModuleHandle(NULL),NULL);
    SendMessage(c,WM_SETFONT,(WPARAM)font,TRUE);return c;
}
static void enabled(BOOL ready){EnableWindow(date,ready);EnableWindow(timebox,ready);EnableWindow(hundredths,ready);EnableWindow(retries,ready);EnableWindow(interval,ready);EnableWindow(earlybox,ready);EnableWindow(sourcebox,ready&&!syncWorker);EnableWindow(syncbutton,ready&&useServer&&!syncWorker);EnableWindow(start,ready&&(!useServer||syncValid)&&!syncWorker);EnableWindow(stop,!ready);}
static void arm(void){
    if(useServer&&(!syncValid||ticks()-syncedAt>3000000000ULL)){syncValid=FALSE;SetWindowText(syncstatus,L"동기화가 필요합니다. 다시 동기화를 누르세요.");EnableWindow(start,FALSE);return;}
    SYSTEMTIME d,t,utc;FILETIME f;
    BOOL fractionOk;UINT fraction=GetDlgItemInt(win,109,&fractionOk,FALSE);
    if(!fractionOk||fraction>99){MessageBox(win,L"1/100초는 00~99로 입력하세요.",L"입력 확인",MB_OK);return;}
    DateTime_GetSystemtime(date,&d);DateTime_GetSystemtime(timebox,&t);
    d.wHour=t.wHour;d.wMinute=t.wMinute;d.wSecond=t.wSecond;d.wMilliseconds=(WORD)(fraction*10);
    if(!TzSpecificLocalTimeToSystemTime(NULL,&d,&utc)||!SystemTimeToFileTime(&utc,&f)){MessageBox(win,L"날짜와 시간을 확인하세요.",L"입력 확인",MB_OK);return;}
    target=((ULONGLONG)f.dwHighDateTime<<32)|f.dwLowDateTime;
    pcTarget=target;
    if(useServer)target=(ULONGLONG)((LONGLONG)target-serverOffset);
    BOOL ok1,ok2;repeatCount=(int)GetDlgItemInt(win,103,&ok1,FALSE);delayMs=(int)GetDlgItemInt(win,104,&ok2,FALSE);
    if(!ok1||!ok2||repeatCount>20||delayMs<10||delayMs>1000){MessageBox(win,L"추가 클릭은 0~20회, 간격은 10~1000ms로 입력하세요.",L"입력 확인",MB_OK);return;}
    earlyClick=SendMessage(earlybox,BM_GETCHECK,0,0)==BST_CHECKED;
    if((target-(earlyClick?(ULONGLONG)delayMs*10000ULL:0)<=ticks())||
       (useServer&&pcTarget-(earlyClick?(ULONGLONG)delayMs*10000ULL:0)<=ticks())){MessageBox(win,L"NOL·PC 예약 모두 사전 클릭을 포함해 미래 시각이어야 합니다.",L"입력 확인",MB_OK);return;}
    hotkey=RegisterHotKey(win,1,MOD_NOREPEAT,VK_ESCAPE);
    if(!hotkey){MessageBox(win,L"Esc 취소 키를 등록하지 못했습니다. 다른 프로그램이 Esc 단축키를 사용 중인지 확인하세요.",L"예약 실패",MB_OK|MB_ICONERROR);return;}
    ResetEvent(cancelEvent);enabled(FALSE);SetWindowText(status,useServer?L"NOL + PC 각각 예약 대기 중 · 같은 횟수로 실행합니다.\r\n첫 클릭 위치 고정 · Esc로 두 예약 모두 취소":L"PC 예약 대기 중 · 클릭할 곳에 마우스를 놓으세요.\r\nEsc 키를 누르면 즉시 취소합니다.");
    worker=CreateThread(NULL,0,run,NULL,0,NULL);
    if(!worker){UnregisterHotKey(win,1);hotkey=FALSE;enabled(TRUE);SetWindowText(status,L"실행 작업을 만들지 못했습니다. 다시 시도하세요.");}
}
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_CREATE:{
        win=h;font=CreateFont(18,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Malgun Gothic");
        clocklabel=control(L"STATIC",L"현재 PC 시간",0,24,12,450,24,0);
        clockbox=control(L"STATIC",L"",SS_CENTER,24,38,456,42,107);
        clockFont=CreateFont(30,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Consolas");
        SendMessage(clockbox,WM_SETFONT,(WPARAM)clockFont,TRUE);
        updateClock();SetTimer(h,1,100,NULL);
        sourcebox=control(L"BUTTON",L"NOL 시간 + PC 시간 모두 클릭",WS_TABSTOP|BS_AUTOCHECKBOX,24,90,330,28,110);
        syncbutton=control(L"BUTTON",L"다시 동기화",WS_TABSTOP,360,90,120,28,111);EnableWindow(syncbutton,FALSE);
        syncstatus=control(L"STATIC",L"PC 시간 기준 · NOL 선택 시 네이비즘과 동기화",0,24,122,456,36,0);
        control(L"STATIC",L"예약 시각 · NOL 선택 시 두 기준에 각각 적용",0,24,164,450,24,0);
        date=control(DATETIMEPICK_CLASS,L"",DTS_SHORTDATEFORMAT,24,196,220,32,101);
        timebox=control(DATETIMEPICK_CLASS,L"",DTS_TIMEFORMAT|DTS_UPDOWN,260,196,155,32,102);
        DateTime_SetFormat(timebox,L"HH':'mm':'ss");
        control(L"STATIC",L":",SS_CENTER,416,200,14,24,0);
        hundredths=control(L"EDIT",L"00",WS_BORDER|ES_NUMBER|ES_CENTER|WS_TABSTOP,432,196,48,32,109);
        SendMessage(hundredths,EM_SETLIMITTEXT,2,0);
        SYSTEMTIME now;GetLocalTime(&now);DateTime_SetSystemtime(date,GDT_VALID,&now);DateTime_SetSystemtime(timebox,GDT_VALID,&now);
        control(L"STATIC",L"추가 클릭 (0~20회)",0,24,250,220,24,0);
        retries=control(L"EDIT",L"3",WS_BORDER|ES_NUMBER|WS_TABSTOP,24,279,220,30,103);
        control(L"STATIC",L"클릭 간격 (10~1000ms)",0,260,250,230,24,0);
        interval=control(L"EDIT",L"50",WS_BORDER|ES_NUMBER|WS_TABSTOP,260,279,220,30,104);
        earlybox=control(L"BUTTON",L"지정 시각보다 클릭 간격만큼 먼저 1회 클릭",WS_TABSTOP|BS_AUTOCHECKBOX,24,320,460,30,108);
        control(L"STATIC",L"NOL 체크 시 NOL·PC 시간에 각각 최초 1회 + 추가 횟수.\r\n사전 클릭도 각 기준에 적용하며, 첫 클릭 위치를 유지합니다.\r\nNOL 클릭 후에도 PC 예약 유지 · Esc로 모두 취소합니다.",0,24,362,460,82,0);
        start=control(L"BUTTON",L"예약 시작",WS_TABSTOP|BS_DEFPUSHBUTTON,24,462,220,42,105);
        stop=control(L"BUTTON",L"취소 (Esc)",WS_TABSTOP,260,462,220,42,106);EnableWindow(stop,FALSE);
        status=control(L"STATIC",L"날짜·시간을 선택하고 예약 시작을 누르세요.\r\n대기 중에는 PC를 켜 두고 잠금·절전 상태를 해제하세요.",0,24,527,460,65,0);
        cancelEvent=CreateEvent(NULL,TRUE,FALSE,NULL);return 0;}
    case WM_TIMER:if(w==1)updateClock();return 0;
    case SYNC_DONE:
        if(syncWorker){WaitForSingleObject(syncWorker,INFINITE);CloseHandle(syncWorker);syncWorker=NULL;}
        if(useServer){
            syncValid=sampleOk;
            if(syncValid){
                serverOffset=sampleOffset;syncedAt=ticks();WCHAR text[160];
                swprintf(text,160,L"동기화됨 · PC 대비 %+.0fms · 통신 %lums\r\n네이비즘 추정 시간 / 네트워크 오차 가능",(double)serverOffset/10000.0,(unsigned long)sampleRtt);
                SetWindowText(syncstatus,text);SetWindowText(clocklabel,L"NOL · 네이비즘 기준 시간 (추정)");
            }else{SetWindowText(syncstatus,L"동기화 실패 · 다시 시도하거나 NOL 체크를 해제하세요.");SetWindowText(clocklabel,L"PC 시간 · NOL 동기화되지 않음");}
        }
        enabled(TRUE);updateClock();return 0;
    case WM_COMMAND:
        if(LOWORD(w)==105)arm();
        if(LOWORD(w)==106)SetEvent(cancelEvent);
        if(LOWORD(w)==110){
            useServer=SendMessage(sourcebox,BM_GETCHECK,0,0)==BST_CHECKED;
            syncValid=FALSE;
            if(useServer){SetWindowText(clocklabel,L"PC 시간 · NOL 동기화 중");syncNow();}
            else{SetWindowText(clocklabel,L"현재 PC 시간");SetWindowText(syncstatus,L"PC 시간 기준");}
            enabled(TRUE);updateClock();
        }
        if(LOWORD(w)==111)syncNow();
        return 0;
    case WM_HOTKEY:if(w==1)SetEvent(cancelEvent);return 0;
    case DONE:
        if(worker){WaitForSingleObject(worker,INFINITE);CloseHandle(worker);worker=NULL;}
        if(hotkey){UnregisterHotKey(h,1);hotkey=FALSE;}enabled(TRUE);
        SetWindowText(status,w==0?(useServer?L"NOL·PC 두 기준의 예약 클릭을 모두 완료했습니다.":L"PC 예약 클릭을 완료했습니다."):w==1?L"예약을 취소했습니다.":w==3?L"일부 예약 시각을 2초 이상 지나 해당 클릭을 건너뛰었습니다.\r\n절전 상태와 PC 시간을 확인하고 다시 예약하세요.":L"일부 클릭의 위치 확인 또는 입력 전송에 실패했습니다.\r\n대상 프로그램의 권한과 잠금 상태를 확인하세요.");return 0;
    case WM_CLOSE:if(syncWorker){WaitForSingleObject(syncWorker,INFINITE);CloseHandle(syncWorker);syncWorker=NULL;}SetEvent(cancelEvent);if(worker){WaitForSingleObject(worker,INFINITE);CloseHandle(worker);worker=NULL;}DestroyWindow(h);return 0;
    case WM_DESTROY:KillTimer(h,1);DeleteObject(clockFont);if(hotkey)UnregisterHotKey(h,1);CloseHandle(cancelEvent);DeleteObject(font);PostQuitMessage(0);return 0;
    }return DefWindowProc(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE prev,LPWSTR cmd,int show){
    (void)prev;(void)cmd;SetProcessDPIAware();INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_DATE_CLASSES};InitCommonControlsEx(&ic);
    WNDCLASS wc={0};wc.lpfnWndProc=proc;wc.hInstance=inst;wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hIcon=LoadIcon(inst,MAKEINTRESOURCE(1));wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);wc.lpszClassName=L"ScheduledClickWindow";RegisterClass(&wc);
    HWND h=CreateWindow(wc.lpszClassName,L"예약 클릭",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,525,653,NULL,NULL,inst,NULL);
    if(!h)return 1;
    ShowWindow(h,show);MSG msg;while(GetMessage(&msg,NULL,0,0)>0){if(!IsDialogMessage(h,&msg)){TranslateMessage(&msg);DispatchMessage(&msg);}}return 0;
}

#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#define DONE (WM_APP+1)
static HWND win,date,timebox,retries,interval,start,stop,status,clockbox,hundredths;
static HWND prebox[2],pretime[2],prefraction[2];
static HANDLE cancelEvent,worker;
static BOOL preEdited[2],updating;
static ULONGLONG ticks(void);
static HFONT font,clockFont;
static void updateClock(void){
    SYSTEMTIME now; WCHAR text[96];
    ULONGLONG value=ticks();
    FILETIME utc={(DWORD)value,(DWORD)(value>>32)},local;
    FileTimeToLocalFileTime(&utc,&local);FileTimeToSystemTime(&local,&now);
    swprintf(text,96,L"%04u-%02u-%02u  %02u:%02u:%02u",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond);
    SetWindowText(clockbox,text);
}
static ULONGLONG target,preTarget[2];
static BOOL preEnabled[2];
static int repeatCount,delayMs;
static BOOL hotkey;
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
    if(!SetCursorPos(p.x,p.y))return 2;
    INPUT in[2]={0};in[0].type=in[1].type=INPUT_MOUSE;
    in[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;in[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
    if(SendInput(2,in,sizeof(INPUT))!=2){
        INPUT up={0};up.type=INPUT_MOUSE;up.mi.dwFlags=MOUSEEVENTF_LEFTUP;SendInput(1,&up,sizeof(INPUT));return 2;
    }
    return 0;
}
/* Merge the two optional single clicks with the unchanged main sequence. */
static DWORD WINAPI run(LPVOID unused){
    (void)unused;
    int next=0,result=0; BOOL pending[2]={preEnabled[0],preEnabled[1]};
    POINT p={0}; int captured=0;
    for(;;){
        int slot=-1; ULONGLONG deadline=0;
        if(next<=repeatCount){slot=2;deadline=target+(ULONGLONG)next*delayMs*10000ULL;}
        for(int i=0;i<2;i++)if(pending[i]&&(slot<0||preTarget[i]<deadline)){slot=i;deadline=preTarget[i];}
        if(slot<0)break;
        int current=waitUntil(deadline);
        if(current==1){result=1;break;}
        if(slot<2)pending[slot]=FALSE;else next++;
        if(current){result=current;continue;}
        if(!captured){if(!GetCursorPos(&p)){result=2;break;}captured=1;}
        current=clickAt(p);
        if(current){result=current;break;}
    }
    PostMessage(win,DONE,result,0);return 0;
}
static HWND control(LPCWSTR cls,LPCWSTR txt,DWORD style,int x,int y,int w,int h,int id){
    HWND c=CreateWindowEx(0,cls,txt,WS_CHILD|WS_VISIBLE|style,x,y,w,h,win,(HMENU)(INT_PTR)id,GetModuleHandle(NULL),NULL);
    SendMessage(c,WM_SETFONT,(WPARAM)font,TRUE);return c;
}
static void enabled(BOOL ready){
    EnableWindow(date,ready);EnableWindow(timebox,ready);EnableWindow(hundredths,ready);
    EnableWindow(retries,ready);EnableWindow(interval,ready);EnableWindow(start,ready);EnableWindow(stop,!ready);
    for(int i=0;i<2;i++){EnableWindow(prebox[i],ready);EnableWindow(pretime[i],ready);EnableWindow(prefraction[i],ready);}
}
/* Local calendar arithmetic first, then convert each slot separately for DST. */
static int readTime(HWND time,HWND fractionBox,SYSTEMTIME day,ULONGLONG *value,BOOL prior){
    SYSTEMTIME t,utc;FILETIME f;BOOL ok;
    UINT fraction=GetDlgItemInt(win,GetDlgCtrlID(fractionBox),&ok,FALSE);
    if(!ok||fraction>99||DateTime_GetSystemtime(time,&t)!=GDT_VALID)return 0;
    day.wHour=t.wHour;day.wMinute=t.wMinute;day.wSecond=t.wSecond;day.wMilliseconds=(WORD)(fraction*10);
    if(prior){
        SYSTEMTIME mainTime;FILETIME mf;
        DateTime_GetSystemtime(timebox,&mainTime);
        mainTime.wYear=day.wYear;mainTime.wMonth=day.wMonth;mainTime.wDay=day.wDay;mainTime.wMilliseconds=0;
        if(!SystemTimeToFileTime(&day,&f)||!SystemTimeToFileTime(&mainTime,&mf))return 0;
        ULONGLONG local=((ULONGLONG)f.dwHighDateTime<<32)|f.dwLowDateTime;
        ULONGLONG mainLocal=(((ULONGLONG)mf.dwHighDateTime<<32)|mf.dwLowDateTime)+(ULONGLONG)GetDlgItemInt(win,109,NULL,FALSE)*100000ULL;
        if(local>mainLocal){local-=864000000000ULL;f.dwLowDateTime=(DWORD)local;f.dwHighDateTime=(DWORD)(local>>32);FileTimeToSystemTime(&f,&day);}
    }
    if(!TzSpecificLocalTimeToSystemTime(NULL,&day,&utc)||!SystemTimeToFileTime(&utc,&f))return 0;
    *value=((ULONGLONG)f.dwHighDateTime<<32)|f.dwLowDateTime;return 1;
}
static void defaultPreTimes(void){
    SYSTEMTIME d,t;FILETIME f;BOOL ok;UINT fraction=GetDlgItemInt(win,109,&ok,FALSE);
    if(!ok||fraction>99)return;
    DateTime_GetSystemtime(date,&d);DateTime_GetSystemtime(timebox,&t);
    d.wHour=t.wHour;d.wMinute=t.wMinute;d.wSecond=t.wSecond;d.wMilliseconds=(WORD)(fraction*10);
    if(!SystemTimeToFileTime(&d,&f))return;
    ULONGLONG base=((ULONGLONG)f.dwHighDateTime<<32)|f.dwLowDateTime;
    updating=TRUE;
    for(int i=0;i<2;i++)if(!preEdited[i]){
        ULONGLONG v=base-(i==0?10000000ULL:5000000ULL);
        f.dwLowDateTime=(DWORD)v;f.dwHighDateTime=(DWORD)(v>>32);FileTimeToSystemTime(&f,&t);
        DateTime_SetSystemtime(pretime[i],GDT_VALID,&t);SetDlgItemInt(win,121+i*3,t.wMilliseconds/10,FALSE);
    }
    updating=FALSE;
}
static void arm(void){
    SYSTEMTIME day;DateTime_GetSystemtime(date,&day);
    if(!readTime(timebox,hundredths,day,&target,FALSE)){MessageBox(win,L"본 예약 날짜·시간과 1/100초(00~99)를 확인하세요.",L"입력 확인",MB_OK);return;}
    BOOL ok1,ok2;repeatCount=(int)GetDlgItemInt(win,103,&ok1,FALSE);delayMs=(int)GetDlgItemInt(win,104,&ok2,FALSE);
    if(!ok1||!ok2||repeatCount>20||delayMs<10||delayMs>1000){MessageBox(win,L"추가 클릭은 0~20회, 간격은 10~1000ms로 입력하세요.",L"입력 확인",MB_OK);return;}
    for(int i=0;i<2;i++){
        preEnabled[i]=SendMessage(prebox[i],BM_GETCHECK,0,0)==BST_CHECKED;
        if(preEnabled[i]&&(!readTime(pretime[i],prefraction[i],day,&preTarget[i],TRUE)||preTarget[i]>=target)){
            MessageBox(win,L"사전 클릭은 본 예약보다 앞선 시각으로 입력하세요.\r\n1/100초는 00~99입니다. 자정 전 시각은 전날로 처리합니다.",L"입력 확인",MB_OK);return;
        }
    }
    ULONGLONG now=ticks();
    if(target<=now||(preEnabled[0]&&preTarget[0]<=now)||(preEnabled[1]&&preTarget[1]<=now)){
        MessageBox(win,L"선택한 모든 클릭 시각이 현재 PC 시간보다 뒤여야 합니다.",L"입력 확인",MB_OK);return;
    }
    hotkey=RegisterHotKey(win,1,MOD_NOREPEAT,VK_ESCAPE);
    if(!hotkey){MessageBox(win,L"Esc 취소 키를 등록하지 못했습니다. 다른 프로그램이 Esc 단축키를 사용 중인지 확인하세요.",L"예약 실패",MB_OK|MB_ICONERROR);return;}
    ResetEvent(cancelEvent);enabled(FALSE);SetWindowText(status,L"PC 시간 예약 대기 중 · 첫 클릭 위치를 유지합니다.\r\nEsc 키로 사전·본 예약을 모두 취소합니다.");
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
        control(L"STATIC",L"본 예약 날짜 · PC 시간 (시:분:초:1/100초)",0,24,94,460,24,0);
        date=control(DATETIMEPICK_CLASS,L"",DTS_SHORTDATEFORMAT,24,124,220,32,101);
        timebox=control(DATETIMEPICK_CLASS,L"",DTS_TIMEFORMAT|DTS_UPDOWN,260,124,155,32,102);
        DateTime_SetFormat(timebox,L"HH':'mm':'ss");
        hundredths=control(L"EDIT",L"00",WS_BORDER|ES_NUMBER|ES_CENTER|WS_TABSTOP,432,124,48,32,109);
        SendMessage(hundredths,EM_SETLIMITTEXT,2,0);
        updating=TRUE;
        for(int i=0;i<2;i++){
            prebox[i]=control(L"BUTTON",i==0?L"사전 클릭 1 (1회)":L"사전 클릭 2 (1회)",WS_TABSTOP|BS_AUTOCHECKBOX,24,174+i*40,225,32,119+i*3);
            pretime[i]=control(DATETIMEPICK_CLASS,L"",DTS_TIMEFORMAT|DTS_UPDOWN,260,174+i*40,155,32,120+i*3);
            DateTime_SetFormat(pretime[i],L"HH':'mm':'ss");
            prefraction[i]=control(L"EDIT",L"00",WS_BORDER|ES_NUMBER|ES_CENTER|WS_TABSTOP,432,174+i*40,48,32,121+i*3);
            SendMessage(prefraction[i],EM_SETLIMITTEXT,2,0);
        }
        SYSTEMTIME now;GetLocalTime(&now);DateTime_SetSystemtime(date,GDT_VALID,&now);DateTime_SetSystemtime(timebox,GDT_VALID,&now);
        preEdited[0]=preEdited[1]=FALSE;updating=FALSE;defaultPreTimes();
        control(L"STATIC",L"추가 클릭 (0~20회)",0,24,250,220,24,0);
        retries=control(L"EDIT",L"3",WS_BORDER|ES_NUMBER|WS_TABSTOP,24,279,220,30,103);
        control(L"STATIC",L"클릭 간격 (10~1000ms)",0,260,250,230,24,0);
        interval=control(L"EDIT",L"50",WS_BORDER|ES_NUMBER|WS_TABSTOP,260,279,220,30,104);
        control(L"STATIC",L"사전 클릭은 체크한 시각에 각각 1회 실행합니다.\r\n기본 제안: 본 예약 1초 전 / 0.5초 전 (직접 수정 가능).\r\n본 예약은 최초 1회 + 추가 횟수만큼 그대로 실행합니다.\r\n최초 클릭 위치 고정 · Esc로 모든 클릭 취소",0,24,326,460,110,0);
        start=control(L"BUTTON",L"예약 시작",WS_TABSTOP|BS_DEFPUSHBUTTON,24,462,220,42,105);
        stop=control(L"BUTTON",L"취소 (Esc)",WS_TABSTOP,260,462,220,42,106);EnableWindow(stop,FALSE);
        status=control(L"STATIC",L"날짜·시간을 선택하고 예약 시작을 누르세요.\r\n대기 중에는 PC를 켜 두고 잠금·절전 상태를 해제하세요.",0,24,527,460,65,0);
        cancelEvent=CreateEvent(NULL,TRUE,FALSE,NULL);return 0;}
    case WM_TIMER:if(w==1)updateClock();return 0;
    case WM_NOTIFY:
        if(((NMHDR*)l)->code==DTN_DATETIMECHANGE&&pretime[1]){
            HWND from=((NMHDR*)l)->hwndFrom;
            for(int i=0;i<2;i++)if(from==pretime[i]&&!updating)preEdited[i]=TRUE;
            if(from==date||from==timebox)defaultPreTimes();
        }
        return 0;
    case WM_COMMAND:
        if(LOWORD(w)==105&&!worker)arm();
        if(LOWORD(w)==106)SetEvent(cancelEvent);
        if(HIWORD(w)==EN_CHANGE&&pretime[1]&&!updating){
            for(int i=0;i<2;i++)if(LOWORD(w)==121+i*3)preEdited[i]=TRUE;
            if(LOWORD(w)==109)defaultPreTimes();
        }
        return 0;
    case WM_HOTKEY:if(w==1)SetEvent(cancelEvent);return 0;
    case DONE:
        if(worker){WaitForSingleObject(worker,INFINITE);CloseHandle(worker);worker=NULL;}
        if(hotkey){UnregisterHotKey(h,1);hotkey=FALSE;}enabled(TRUE);
        SetWindowText(status,w==0?L"사전·본 예약 클릭을 모두 완료했습니다.":w==1?L"예약을 취소했습니다.":w==3?L"일부 예약 시각을 2초 이상 지나 해당 클릭을 건너뛰었습니다.\r\n절전 상태와 PC 시간을 확인하고 다시 예약하세요.":L"일부 클릭의 위치 확인 또는 입력 전송에 실패했습니다.\r\n대상 프로그램의 권한과 잠금 상태를 확인하세요.");return 0;
    case WM_CLOSE:SetEvent(cancelEvent);if(worker){WaitForSingleObject(worker,INFINITE);CloseHandle(worker);worker=NULL;}DestroyWindow(h);return 0;
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

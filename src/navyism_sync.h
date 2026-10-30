#ifndef NAVYISM_SYNC_H
#define NAVYISM_SYNC_H
#include <winhttp.h>
#include "navyism_time.h"
static int downloadNavy(LONGLONG *offset,DWORD *rtt){
    int ok=0;char *body=NULL;DWORD used=0;
    HINTERNET session=WinHttpOpen(L"ScheduledClick/1.4.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!session)return 0;
    WinHttpSetTimeouts(session,4000,4000,4000,4000);
    HINTERNET connection=WinHttpConnect(session,L"time.navyism.com",INTERNET_DEFAULT_HTTPS_PORT,0);
    HINTERNET request=connection?WinHttpOpenRequest(connection,L"GET",L"/?host=ticket.interpark.com",NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE|WINHTTP_FLAG_REFRESH):NULL;
    ULONGLONG before=ticks(),after=before;
    if(request&&WinHttpSendRequest(request,L"Cache-Control: no-cache\r\n",(DWORD)-1L,WINHTTP_NO_REQUEST_DATA,0,0,0)&&WinHttpReceiveResponse(request,NULL)){
        DWORD code=0,size=sizeof(code);
        if(WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&code,&size,WINHTTP_NO_HEADER_INDEX)&&code==200){
            body=malloc(1024*1024+1);
            if(body){
                DWORD got=0;
                while(used<1024*1024&&WinHttpReadData(request,body+used,1024*1024-used,&got)){
                    if(!got){ok=1;break;}used+=got;
                }
                body[used]=0;after=ticks();
                long long remote=0;
                if(!ok||after<before||after-before>20000000ULL||!navyTime(body,&remote))ok=0;
                if(ok){
                    /* Midpoint estimate; not a precision time protocol. */
                    *offset=(LONGLONG)(116444736000000000ULL+(ULONGLONG)remote*10000ULL)-(LONGLONG)(before+(after-before)/2);
                    *rtt=(DWORD)((after-before)/10000ULL);
                }
            }
        }
    }
    free(body);if(request)WinHttpCloseHandle(request);if(connection)WinHttpCloseHandle(connection);WinHttpCloseHandle(session);return ok;
}
#endif

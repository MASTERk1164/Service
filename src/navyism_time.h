#ifndef NAVYISM_TIME_H
#define NAVYISM_TIME_H
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
/* Parse the currently observed public page format. Fail closed on changes. */
static int navyNumber(const char *text,const char *key,long long *value){
    const char *p=strstr(text,key);char *end;
    if(!p)return 0;
    p+=strlen(key);errno=0;*value=strtoll(p,&end,10);
    return end!=p&&!errno&&(*end==';'||*end==')');
}
static int navyTime(const char *html,long long *unixMs){
    size_t n=strlen(html);char *compact=malloc(n+1);size_t j=0;
    if(!compact)return 0;
    for(size_t i=0;i<n;i++)if(!isspace((unsigned char)html[i]))compact[j++]=html[i];
    compact[j]=0;
    long long seconds=0,millis=0,adjust=0,gap=0;
    const char *show=strstr(compact,"functionstartShow(){show(");
    int ok=strstr(compact,"host=\"ticket.interpark.com\"")&&show&&
        navyNumber(compact,"varthisTime2=",&seconds)&&
        navyNumber(compact,"varmgap=",&millis)&&
        navyNumber(compact,"mgval=mgval+",&adjust)&&
        navyNumber(show,"-time()+",&gap)&&
        seconds>=1700000000LL&&seconds<4102444800LL&&
        millis>=0&&millis<1000&&adjust>=0&&adjust<=5000&&gap>=-3600&&gap<=3600;
    if(ok)*unixMs=(seconds+gap)*1000+millis-adjust;
    free(compact);return ok;
}
#endif

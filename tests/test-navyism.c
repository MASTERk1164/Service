#include <assert.h>
#include <stdio.h>
#include "../src/navyism_time.h"
int main(void){
    long long time=0;
    const char *observed="var host=\"ticket.interpark.com\";\nvar mgap = 756;\nvar thisTime2 = 1793368768;\nfunction startShow()\n{\n show(1793368768 - time() + 0);\n}\nstartShow();\nmgval = mgval + 900;";
    assert(navyTime(observed,&time));assert(time==1793368767856LL); /* .756 - .900 */
    assert(!navyTime("<html>network error</html>",&time));
    assert(!navyTime("var host=\"nol.yanolja.com\";",&time));
    assert(!navyTime("var host=\"ticket.interpark.com\"; var mgap=500; var thisTime2=1793368768; function startShow(){show(1793368768-time()+0);}",&time));
    puts("PASS: observed Navyism format, correction, missing fields, wrong host");
}

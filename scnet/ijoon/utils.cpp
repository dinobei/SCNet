#include "utils.h"
#include <time.h>
#include <ijoon/coreutils.h>

void initRandomString() {
    srand(time(0));
}

std::string generateRandomString(unsigned int length) {
    std::string strList = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string arbiratyStr;
    while(length--) {
        arbiratyStr += strList.c_str()[rand()%strList.size()];
    }
    
    return arbiratyStr;
}


/* get system time */
void itimeofday(long *sec, long *nsec)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	if (sec) *sec = ts.tv_sec;
	if (nsec) *nsec = ts.tv_nsec;
}

/* get clock in millisecond 64 */
IINT64 iclock64(void)
{
	long s, n;
	IINT64 value;
	itimeofday(&s, &n);
	value = ((IINT64)s) * 1000 + (n / 1000000);
	return value;
}

IUINT32 iclock()
{
	return (IUINT32)(iclock64() & 0xfffffffful);
}

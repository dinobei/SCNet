#include "utils.h"
#include <time.h>

void initRandomString() {
    srand(time(0));
}

std::string generateRandomString(unsigned int length) {
    std::string strList = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string arbiratyStr;
    while(length--) {
        arbiratyStr += strList.c_str()[rand()%strList.size()];
    }
    
    return arbiratyStr;
}

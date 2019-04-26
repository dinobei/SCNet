#ifndef __UTILS_H__
#define __UTILS_H__
#include <iostream>
#include "ikcp.h"

void initRandomString();
std::string generateRandomString(unsigned int length);

IUINT32 iclock();

#endif

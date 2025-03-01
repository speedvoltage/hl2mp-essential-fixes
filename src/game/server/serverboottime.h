#ifndef SERVERBOOTINFO_H
#define SERVERBOOTINFO_H

#ifdef LINUX
#include <ctime>

const char *GetServerBootTime();

void InitializeServerBootTime();
#endif // LINUX
#endif // SERVERBOOTINFO_H
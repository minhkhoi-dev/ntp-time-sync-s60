/* Minimal stand-in for Symbian's e32def.h so ntppacket.cpp builds on a PC. */
#ifndef E32DEF_SHIM_H
#define E32DEF_SHIM_H
typedef int TInt;
typedef unsigned int TUint;
typedef unsigned char TUint8;
typedef unsigned int TUint32;
typedef long long TInt64;
typedef unsigned long long TUint64;
typedef int TBool;
#endif

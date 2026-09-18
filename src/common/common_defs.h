#ifndef __COMMON_DEFS_H__
#define __COMMON_DEFS_H__

// Include CPP libraries
#include <bits/stdc++.h>
/*
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
*/
using namespace std;

// Types
typedef char                s8;
typedef unsigned char       u8;
typedef short               s16;
typedef unsigned short      u16;
//typedef long                s32;
//typedef unsigned long       u32;
typedef int                 s32;
typedef unsigned int        u32;
typedef long long           s64;
typedef unsigned long long  u64;

// Constants
#define MAX_U8  255
#define MAX_U16 65535
#define MAX_U32 4294967295u

// Prefix for host-side (non-UART) emulator output, so it can be told apart
// from the guest's console stream.
#define EMU_TAG "[catnip_emulator] "

#endif

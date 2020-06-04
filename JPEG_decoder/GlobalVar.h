// 全局变量
#pragma once
#include <windows.h>

extern double PI;
extern DWORD t1, t2, t3, t4;

// CbCr拓扑表
extern int CrCb[64];
extern int MapCrCb(int i, int s);

// 反z型编码表
extern int FZig[64];

// IDCT
extern double fastIDCT[8][8];

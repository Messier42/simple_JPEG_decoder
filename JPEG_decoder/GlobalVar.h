// ȫ�ֱ���
#pragma once
#include <windows.h>

extern double PI;
extern DWORD t1, t2, t3, t4;

// CbCr���˱�
extern int CrCb[64];
extern int MapCrCb(int i, int s);

// ��z�ͱ����
extern int FZig[64];

// IDCT
extern double fastIDCT[8][8];

#pragma once
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <windows.h>
using namespace std;

class JPEG
{
public:
	ifstream myjpg;		// ͼƬ�ļ�����
	long int jpg_size;	// ͼƬ�ļ��ֽ���
	long data_start;	// ���ݿ�ʼ��
	bool result;		// ������

	// ͼ�ν�������Ŵ�
	int** rgb_r;    
	int** rgb_g;
	int** rgb_b;
	long mcu_w, mcu_h;	// ��ȫ�߽ǵĿ�͸�

	//SOFO  0XFFC0
	int data_precision;	// ��ɫ���ݾ��ȣ�8/12/16:  8/12/16��bit��
	int jpg_hight;		// ͼƬ�߶ȣ����أ�
	int jpg_width;		// ͼƬ��ȣ����أ�
	int colors;			// ��ɫ��������1�Ҷȣ�3YCrCb��4CMYK��
	int color_info[3][5];	// ����ɫ������Ϣ��[n][0]=ID,[n][1]=ˮƽ/��ֱ�������ӣ��ߵ�4λ��,[n][2]=��ǰ��ɫ����ʹ�õ�������,[n][3]/[4]=��ǰ��ɫ����ʹ�õ�ֱ��/����������������
	int YCbCr_ratio;	// ������
	int AC_correction[3];		// Y Cr Cb ����ֱ������������

	unsigned char* data_buf;	// ���ݻ�����
	long data_lp;				// ����ָ��
	long datasize;				// �����ܳ�

	// DQT 0XFFDB
	unsigned short QT[4][64];	// ������

	// DHT 0XFFC4
	int HT_bit[4][17];		// ��������ͬλ��������������16������������֮�ͣ�1��
	int* sp_HT_value[4];	// ������Ȩֵ��ָ��
	int* sp_HT_tree[4];		// ��������ָ��
	 
	// DRI 0XFFDD
	unsigned short DRI_val;	// ��ֱ����ۼƸ�λֵ
	 
	// Getbit()��������
	int s_bit;
	unsigned char bittemp;

	// Decode_Photo()��ز���
	int mcu_x, mcu_y;

public:
	JPEG(char* filename);
	bool ReadSOF0(void);
	bool ReadDQT(void);
	bool ReadDHT(void);
	bool ReadDRI(void);
	bool ReadSOS(void);
	bool BuildHTree(void);	// ������������

	void PrintInfo(void);	// ��ʾͼƬ��Ϣ

	unsigned GetBit(void);	// ��λ��ȡ��Ϣ
	
	int FindTree(unsigned int temp, int tree);	// ���������
	
	double C(int u)	// DCT�ú���
	{
		if (u == 0)
			return sqrt(2)/2;
		else
			return 1;
	}
		
	int FindTable(unsigned short temp, int s)	// �����
	{
		if (pow((double)2, s - 1) > temp)
		{
			return (int)(temp - pow((double)2, s) + 1);
		}
		else
			return temp;
	}

	bool ReadPart(double* OUT_buf, int YCbCr);	// 8*8���ݿ����YCrCb��1=Y,2=Cb,3=Cr��
	
	bool DecodePhoto(void);	// ͼƬ����
	
	bool ShowPhoto(int w_x, int w_y);

	unsigned char get(void)
	{
		if (data_lp < datasize)
			return data_buf[data_lp++];
	}
};


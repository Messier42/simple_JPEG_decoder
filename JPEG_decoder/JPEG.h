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
	ifstream myjpg;		// 图片文件对象
	long int jpg_size;	// 图片文件字节数
	long data_start;	// 数据开始点
	bool result;		// 解码结果

	// 图形解码结果存放处
	int** rgb_r;    
	int** rgb_g;
	int** rgb_b;
	long mcu_w, mcu_h;	// 补全边角的宽和高

	//SOFO  0XFFC0
	int data_precision;	// 颜色数据精度：8/12/16:  8/12/16（bit）
	int jpg_hight;		// 图片高度（像素）
	int jpg_width;		// 图片宽度（像素）
	int colors;			// 颜色分量数（1灰度，3YCrCb，4CMYK）
	int color_info[3][5];	// 各颜色分量信息：[n][0]=ID,[n][1]=水平/垂直采样因子（高低4位）,[n][2]=当前颜色分量使用的量化表,[n][3]/[4]=当前颜色分量使用的直流/交流哈夫曼树表编号
	int YCbCr_ratio;	// 采样比
	int AC_correction[3];		// Y Cr Cb 三个直流矫正变量；

	unsigned char* data_buf;	// 数据缓存区
	long data_lp;				// 数据指针
	long datasize;				// 数据总长

	// DQT 0XFFDB
	unsigned short QT[4][64];	// 量化表

	// DHT 0XFFC4
	int HT_bit[4][17];		// 哈夫曼表不同位数的码字数量（16），码字数量之和（1）
	int* sp_HT_value[4];	// 哈夫曼权值表指针
	int* sp_HT_tree[4];		// 哈夫曼树指针
	 
	// DRI 0XFFDD
	unsigned short DRI_val;	// 差分编码累计复位值
	 
	// Getbit()所需数据
	int s_bit;
	unsigned char bittemp;

	// Decode_Photo()相关参数
	int mcu_x, mcu_y;

public:
	JPEG(char* filename);
	bool ReadSOF0(void);
	bool ReadDQT(void);
	bool ReadDHT(void);
	bool ReadDRI(void);
	bool ReadSOS(void);
	bool BuildHTree(void);	// 建立哈夫曼树

	void PrintInfo(void);	// 显示图片信息

	unsigned GetBit(void);	// 按位读取信息
	
	int FindTree(unsigned int temp, int tree);	// 查哈夫曼树
	
	double C(int u)	// DCT用函数
	{
		if (u == 0)
			return sqrt(2)/2;
		else
			return 1;
	}
		
	int FindTable(unsigned short temp, int s)	// 译码表
	{
		if (pow((double)2, s - 1) > temp)
		{
			return (int)(temp - pow((double)2, s) + 1);
		}
		else
			return temp;
	}

	bool ReadPart(double* OUT_buf, int YCbCr);	// 8*8数据块解码YCrCb（1=Y,2=Cb,3=Cr）
	
	bool DecodePhoto(void);	// 图片解码
	
	bool ShowPhoto(int w_x, int w_y);

	unsigned char get(void)
	{
		if (data_lp < datasize)
			return data_buf[data_lp++];
	}
};


#include "JPEG.h"
#include "GlobalVar.h"

JPEG::JPEG(char* filename){
	result = true;

	//打开图像文件
	myjpg.open(filename, ios::binary | ios::in);
	if (myjpg.good() == false) {
		std::cout << "文件打开失败！" << endl;
		result = false;
	}
		
	//验证文件是否为JPEG图片
	unsigned short temp = 0;
	temp = myjpg.get();
	temp = (temp << 8) + myjpg.get();
	if (temp != 0xFFD8) {
		while (((temp = (temp << 8) | myjpg.get()) != 0xFFE0) && (temp != 0xFFD9));
		if (temp == 0xFFE0) {
			myjpg.seekg(2, ios::cur);	// 跳过数据长度
			unsigned long tag = myjpg.get() << 64 | myjpg.get() << 32 |
				myjpg.get() << 16 | myjpg.get() << 8 | myjpg.get();		// 标识符
			if (tag == 0x4A46494600)
				result = true;
			else {
				result = false;
				std::cout << "图片格式错误！" << endl;
			}
		}
	}

	// 读取文件头部信息
	if (result == true) {
		//获取文件大小
		myjpg.seekg(0, ios::end);
		this->jpg_size = (long)myjpg.tellg();

		// GetBit()数据初始化
		s_bit = 0;		// 已读取的位数（每次读8位）
		bittemp = 0;	// 当前位的值

		// 直流系数的差分编码校正
		AC_correction[0] = 0xabcdef;
		AC_correction[1] = 0xabcdef;
		AC_correction[2] = 0xabcdef;

		// DecodePhoto()数据初始化
		mcu_x = 0;		// MCU起始x
		mcu_y = 0;		// MCU起始y

		DRI_val = 0;	//差分编码累计复位的间隔

		//量化表初始化
		for (int n = 0; n < 4; n++) {
			for (int i = 0; i < 8; i++) {
				for (int j = 0; j < 8; j++) {
					QT[n][i * 8 + j] = 0;
				}
			}
		}

		//图片初始化：图片信息提取
		if (ReadSOF0() && ReadDQT() && ReadDHT() && !ReadDRI() && ReadSOS()) {
			result = true;
			std::cout << "图片打开成功！" << endl;
		}
		else {
			result = false;
			std::cout << "图片打开失败！" << endl;
		}
	}

	// 其他准备工作
	if(result == true){
		// 建立霍夫曼树
		BuildHTree();

		// 建立图片缓存区
		if (YCbCr_ratio == 0x111)
		{
			int w = 0, h = 0;	// 宽和高包含的8*8矩阵数
			w = jpg_width / 8;
			if ((jpg_width % 8) > 0)w++;	// 补全宽
			h = jpg_hight / 8;
			if ((jpg_hight % 8) > 0)h++;	// 补全高

			mcu_w = w * 8;		// 补全后的宽
			mcu_h = h * 8;		// 补全后的高
			rgb_r = new int*[mcu_h];
			rgb_g = new int*[mcu_h];
			rgb_b = new int*[mcu_h];
			for (long i = 0; i < mcu_h; i++)
			{
				rgb_r[i] = new int[mcu_w];
				rgb_g[i] = new int[mcu_w];
				rgb_b[i] = new int[mcu_w];
			}
		}
		else if (YCbCr_ratio == 0x411)
		{
			int w = 0, h = 0;	// 宽和高包含的16*16矩阵数
			w = jpg_width / 16;
			if ((jpg_width % 16) > 0)w++;	// 补全宽
			h = jpg_hight / 16;
			if ((jpg_hight % 16) > 0)h++;	// 补全高

			mcu_w = w * 16;		// 补全后的宽
			mcu_h = h * 16;		// 补全后的高
			rgb_r = new int*[mcu_h];
			rgb_g = new int*[mcu_h];
			rgb_b = new int*[mcu_h];
			for (long i = 0; i < mcu_h; i++)
			{
				rgb_r[i] = new int[mcu_w];
				rgb_g[i] = new int[mcu_w];
				rgb_b[i] = new int[mcu_w];
			}
		}
		//建立图片原始数据缓存区
		myjpg.seekg(data_start, ios::beg);
		datasize = jpg_size - data_start;
		data_buf = new unsigned char[jpg_size];
		myjpg.read((char*)data_buf, datasize);
		if (myjpg.gcount() == datasize)
			std::cout << "读取完整" << endl;
		else {
			std::cout << dec << myjpg.gcount() << '-' << datasize << endl;
		}
		data_lp = 0;//缓存区数据指针
		myjpg.close();
	}
}

bool JPEG::ReadSOF0() {
	unsigned short temp = 0;
	myjpg.seekg(0, ios::beg);

	while (((temp = (temp << 8) | myjpg.get()) != 0xFFC0) && (temp != 0xFFD9));
	if (temp == 0xFFC0)		// SOF0段标记代码
	{
		temp = myjpg.get();
		temp = (temp << 8) + myjpg.get();
		if (temp == 17)		// 数据长度 JFIF默认颜色分量为YCrCb，故该段长度为17字节
		{
			this->data_precision = myjpg.get();					// 获取精度		1字节
			this->jpg_hight = (myjpg.get() << 8) + myjpg.get();	// 获取图像高度	2字节
			this->jpg_width = (myjpg.get() << 8) + myjpg.get();	// 获取图像宽度	2字节
			this->colors = myjpg.get();							// 颜色分量数		1字节
			for (int i = 0; i < 3; i++)							// 3个颜色分量
				for (int j = 0; j < 3; j++)		// 0表示颜色分量ID，1表示水平（高4位）/垂直（低4位）采样因子，2表示量化表ID
					this->color_info[i][j] = myjpg.get();
			// YCbCr采样比
			YCbCr_ratio = ((color_info[0][1] & 0xf0) >> 4) * (color_info[0][1] & 0x0f);
			YCbCr_ratio = (YCbCr_ratio << 4) | (((color_info[1][1] & 0xf0) >> 4) * (color_info[1][1] & 0x0f));
			YCbCr_ratio = (YCbCr_ratio << 4) | (((color_info[2][1] & 0xf0) >> 4) * (color_info[2][1] & 0x0f));
		}
		else {
			std::cout << "本程序仅能读取颜色空间为YCrCb的图像，SOF0读取失败！" << endl;
			return false;
		}
	}
	else {
		std::cout << "未找到SOF0段标记，SOF0读取失败！" << endl;
		return false;
	}
		
	std::cout << "SOF0读取成功！" << endl;
	return true;
}

bool JPEG::ReadDQT() {
	unsigned short temp = 0, S = 0;
	myjpg.seekg(0, ios::beg);
	while (!myjpg.eof())
	{
		temp = 0;
		while (((temp = (temp << 8) + myjpg.get()) != 0xFFDB) && (temp != 0xFFC0));
		if (temp == 0xFFDB) {				// DCT段标记代码
			int size = (myjpg.get() << 8) | myjpg.get();	// 数据长度
			size -= 2;						// 减去数据长度本身占的2字节
			while (size > 0) {
				S = myjpg.get();
				size--;
				if ((S & 0xF0) == 0) {		// 高4位为0，即量化精度为8位
					S = S & 0x0F;			// 低4位表示量化表ID
					for (int i = 0; i < 64; i++) {
						this->QT[S][i] = myjpg.get();
						size--;
					}
				}
				else {						// 高4位为1，即量化精度为16位
					S = S & 0x0F;			// 低4位表示量化表ID
					for (int i = 0; i < 64; i++) {
						this->QT[S][i] = (myjpg.get() << 8) | (myjpg.get());
						size -= 2;
					}
				}
			}
		}
		else {
			std::cout << "DQT读取成功！" << endl;
			return true;
		}
	}
	std::cout << "DQT读取失败！" << endl;
	return false;
}

bool JPEG::ReadDHT() {
	unsigned short temp = 0, ID = 0, u = 0;;
	long ts = 0;
	myjpg.seekg(0, ios::beg);

	while (!myjpg.eof())			// 部分JPEG包含多个DHT段，每个段仅包含一个哈夫曼表
	{
		temp = 0;
		ID = 0;
		while (((temp = (temp << 8) + myjpg.get()) != 0xFFC4) && (temp != 0xFFDA));
		if (temp == 0xFFC4)		// DHT段标记代码
		{
			int size = (myjpg.get() << 8) | myjpg.get();	// 数据长度
			size -= 2;
			while (size > 0)	// 部分JPEG仅含一个DHT段，该段包含所有哈夫曼表
			{
				ID = myjpg.get();			// 表ID和表类型
				size--;
				ID = ((ID & 0x10) >> 3) | (ID & 0x01);	// 将表ID转换为0~3

				HT_bit[ID][16] = 0;	
				for (int i = 0; i < 16; i++)
				{
					this->HT_bit[ID][i] = myjpg.get();	// 不同位数的码字数量
					HT_bit[ID][16] += HT_bit[ID][i];	// 不同编码数量之和
					size--;
				}

				sp_HT_value[ID] = new int[(HT_bit[ID][16])];

				for (int i = 0; i < (HT_bit[ID][16]); i++)
				{
					this->sp_HT_value[ID][i] = myjpg.get();	// 每个表中每种编码的具体内容
					size--;
				}
			}
		}
		else {
			std::cout << "DHT读取成功！" << endl;
			return true;
		}
	}
	std::cout << "DHT读取失败！" << endl;
	return 0;
}

bool JPEG::ReadDRI() {
	unsigned short temp = 0;
	myjpg.seekg(0, ios::beg);

	while (1)
	{
		temp = 0;
		while (((temp = (temp << 8) + myjpg.get()) != 0xFFDD) && (temp != 0xFFDA));
		if (temp == 0xFFDD)
		{
			myjpg.seekg(2, ios::cur);
			temp = myjpg.get();
			temp = (temp << 8) + myjpg.get();
			DRI_val = temp;
			std::cout << "本程序暂不支持解码含DRI的图片！" << endl;
			return true;
		}
		else
			return false;
	}
}

bool JPEG::ReadSOS() {
	unsigned short temp = 0, s = 0;
	myjpg.seekg(0, ios::beg);

	while (((temp = (temp << 8) | myjpg.get()) != 0xFFDA) && (temp != 0xFFD9));
	if (temp == 0xFFDA)		// SOS段标记代码
	{
		myjpg.seekg(2, ios::cur);	// 跳过数据长度
		s = myjpg.get();			// 颜色分量数
		for (int i = 0; i < s; i++)
		{
			myjpg.seekg(1, ios::cur);	// 跳过颜色分量ID
			temp = myjpg.get();			
			this->color_info[i][3] = temp >> 4;		// 高4位：直流分量使用的哈夫曼树编号
			this->color_info[i][4] = temp & 0x0F;	// 低4位：交流分量使用的哈夫曼树编号
		}
		myjpg.seekg(3, ios::cur);		// 跳过压缩数据图像数据
		data_start = (long)myjpg.tellg();	// 图像信息开始点
	}
	else {
		std::cout << "SOS读取失败！" << endl;
		return false;
	}
	std::cout << "SOS读取成功！" << endl;
	return true;
}

bool JPEG::BuildHTree() {
	unsigned int temp = 0;
	int t1 = 0;	// 前一个码字的位数
	int lp = 0;

	// 计算每个码字的位数
	for (int i = 0; i < 4; i++) {
		sp_HT_tree[i] = new int[HT_bit[i][16]];	
		temp = 0;
		lp = 0;
		for (int ss = 0; ss < 16; ss++)
		{
			for (int ww = 0; ww < HT_bit[i][ss]; ww++)
			{
				sp_HT_tree[i][lp++] = ss + 1;	// 存放码字位数
			}
		}
	}

	// 计算每个码字的码值
	for (int i = 0; i < 4; i++){
		temp = 0;
		for (int j = 0; j < HT_bit[i][16]; j++){
			if (j == 0){						// 第一个码字为若干个0
				if (sp_HT_tree[i][0] != 1){		// 第一个码字长度大于1
					t1 = sp_HT_tree[i][0];		// 保存当前码字位数
					sp_HT_tree[i][0] <<= 16;	// 高位存放码字长度
					temp = 0;					// 码字为0
				}
				else {							// 第一个码字长度等于1
					t1 = sp_HT_tree[i][0];		// 保存当前码字位数
					temp = 0;
					sp_HT_tree[i][0] <<= 16;	// 高位存放码字长度
					temp = 0;					// 码字为0
				}
			}
			else {								// 后续码字
				if (sp_HT_tree[i][j] > t1){		// 当前码字位数比前面的码字位数大
					temp++;								// 码字 = 前面的码字 + 1 
					temp <<= (sp_HT_tree[i][j] - t1);	// 在码字后面添0直至满足码字位数
					t1 = sp_HT_tree[i][j];				// 保存当前码字位数
					sp_HT_tree[i][j] <<= 16;			// 高位存放码字长度
					sp_HT_tree[i][j] |= temp;			// 保存当前码字
				}
				else {							// 当前码字位数与前面的码字位相同
					temp++;						// 码字 = 前面的码字 + 1 
					sp_HT_tree[i][j] <<= 16;
					sp_HT_tree[i][j] |= temp;	// 保存当前码字
				}
			}
		}
	}
	return true;
}

// 显示图片信息
void JPEG::PrintInfo() {
	std::cout << "图片文件大小：" << (double)jpg_size / 1024 << "kb" << endl;
	std::cout << "高*宽：" << jpg_hight << "*" << jpg_width << endl;
	std::cout << "颜色精度：" << data_precision << endl;
	std::cout << "颜色分量数：" << colors << endl;
	std::cout << "YCbCr采样比：" << hex << YCbCr_ratio << endl << endl;

	// 量表
	for (int n = 0; n < 4; n++){
		std::cout << "量表" << n << ':' << endl;
		for (int i = 0; i < 8; i++){
			for (int j = 0; j < 8; j++){
				std::cout << dec << QT[n][i * 8 + j] << '\t';
			}
			std::cout << endl;
		}
	}
	std::cout << endl;

	// 哈夫曼树
	for (int n = 0; n < 4; n++){
		for (int ui = 0; ui < 16; ui++){
			std::cout << dec << HT_bit[n][ui] << ' ';
		}
		std::cout << endl;
		std::cout << "哈夫曼树" << dec << n + 1 << endl;
		int temp = 0;
		for (int i = 0; i < HT_bit[n][16]; i++){
			std::cout << hex << setw(2) << sp_HT_value[n][i] << "--->";
			temp = sp_HT_tree[n][i];
			std::cout << dec << setw(2) << ((temp & 0xf0000) >> 16) << "  ";
			for (int x = 0; x < 16; x++){
				if ((temp & 0x8000) == 0)
					std::cout << '0';
				else
					std::cout << '1';
				temp <<= 1;
			}
			std::cout << endl;
		}
		std::cout << endl;
	}

	// 颜色信息
	for (int i = 0; i < 3; i++){
		for (int j = 0; j < 5; j++){
			std::cout << hex << color_info[i][j] << '\t';
		}
		std::cout << endl;
	}

}

// 按位读取信息
unsigned JPEG::GetBit() {
	int x = 0, ee = 0;

	if (s_bit == 0){
		s_bit = 8;					// 每次读取8位数据
		if (data_lp >= (datasize - 2))return 0x0f;
		bittemp = data_buf[data_lp++];
		if (bittemp == 0xff){
			ee = data_buf[data_lp++];

			if (ee == 0xD9) {		// EOI
				data_lp -= 2;
				std::cout << "over! " << hex << 0xffd9 << endl;
				return 0x0f;
			}
			else if(ee != 0x00) {	// 其他错误
				std::cout << "error:" << hex << ee << endl;
				return 0x0f;
			}
		}
	}

	if (0 == (bittemp & 0x80))	// 当前位为0
		x = 0;
	else						// 当前位为1
		x = 1;
	bittemp <<= 1;				// 左移一位，每次读取最高位
	s_bit--;
	return x;
}

// 查霍夫曼树
int JPEG::FindTree(unsigned int temp, int tree) {
	for (int i = 0; i < HT_bit[tree][16]; i++){
		if (temp == sp_HT_tree[tree][i]) {	// 匹配码字
			return sp_HT_value[tree][i];	// 返回码值
		}
	}
	return 0xffffffff;
}

// 8*8数据块解码YCrCb（1=Y,2=Cb,3=Cr）
bool JPEG::ReadPart(double* OUT_buf, int YCbCr) {
	// 1.利用霍夫曼树解码
	// decode_val[i][0]表示第i个分量前面0的数量
	// decode_val[i][1]表示第i个分量的值
	int decode_val[64][2] = { 0 };
	unsigned short temp = 0;
	int c = 0, x = 0, sss = 0;
	int df = 0, ww = 1, sp = 0;


	t3 = GetTickCount();
	// 读取直流分量（1个）
	for (int x1 = 0; x1 < 16; x1++)	// 哈夫曼编码长度为1~16位
	{
		if (0x0f == (df = GetBit()))
			return false;
		temp = (temp << 1) + df;
		c = FindTree(temp | ((x1 + 1) << 16), color_info[YCbCr - 1][3]);	// 查找直流哈夫曼表
		if (c != 0xffffffff) {	// 当前码值表示该直流分量数值的位数
			temp = x = 0;		// 临时码字清零
			for (int i = 0; i < c; i++) {
				if (0x0f == (df = GetBit()))
					return false;
				temp = (temp << 1) + df;
				x++;
			}
			decode_val[sp++][1] = this->FindTable(temp, x);	// 查找直流分量对应的数值
			sss++;				// 分量数 += 1
			break;
		}
	}

	// 读取交流分量（63个）
	while (ww){
		temp = x = 0;
		// 哈夫曼编码长度为1~16位
		for (int x1 = 0; x1 < 16; x1++) {
			if (0x0f == (df = GetBit()))
				return false;
			temp = (temp << 1) + df;
			// 查找交流哈夫曼表（交流0号对应2号；交流1号对应3号）
			c = FindTree(temp | ((x1 + 1) << 16), color_info[YCbCr - 1][4] + 2);

			if ((c != 0xffffffff) && (c != 0)) {
				temp = 0;
				// 当前码值高4位表示当前数值前面有多少个连续的零
				decode_val[sp][0] = (c & 0xf0) >> 4;
				// 分量数 += 当前非0分量前面为0的分量数	
				sss += decode_val[sp][0];
				// 当前码值低4位表示该交流分量数值的二进制位数
				c = c & 0x0f;						
				if (c != 0) {	// 交流分量值不为0
					for (int i = 0; i < c; i++) {
						if (0x0f == (df = GetBit()))
							return false;
						temp = (temp << 1) + df;
						x++;
					}
					// 查找直流分量对应的数值
					decode_val[sp++][1] = this->FindTable(temp, x);
					sss++;	// 分量数 += 1
					// 当已经读入63个交流变量时停止读入交流变量
					if ((sss == 64) || (sp > 63))
						ww = 0;
					break;
				}
				else if (c == 0) {	// 交流分量值为0
					decode_val[sp++][1] = 0;			
					sss++;	// 分量数 += 1
					// 当已经读入63个交流变量时停止读入交流变量
					if ((sss == 64) || (sp > 63))
						ww = 0;
					break;
				}
			}
			// 当前码值为0说明往后交流变量全为0，无需再读入交流分量
			else if (c == 0) {
				ww = 0;
				break;
			}
			// 编码长度不可能大于16位
			if (x1 == 15)
				return false;
		}
	}

	// 2.直流系数的差分编码校正
	int a[64] = { 0 };

	if (AC_correction[YCbCr - 1] == 0xabcdef)
	{
		a[0] = decode_val[0][1];
		AC_correction[YCbCr - 1] = a[0];
	}
	else
	{
		a[0] = decode_val[0][1] + AC_correction[YCbCr - 1];
		AC_correction[YCbCr - 1] = a[0];
	}

	// 3.游程编码恢复成8*8矩阵
	int sum = 1;

	for (int i = 1; i < 64; i++){
		if ((decode_val[i][0] == 0) && (decode_val[i][1] == 0))
			break;

		for (int j = 0; j < decode_val[i][0]; j++) {	// 填充0
			a[sum++] = 0;
		}
		if (sum > 63)
			return false;

		a[sum++] = decode_val[i][1];					// 填充非0编码
		if (sum > 63)
			break;
	}

	// 4.反Z型编码+反量化
	int FZ[64] = { 0 };
	int qtx = 0;

	qtx = color_info[YCbCr - 1][2];	//	查量化表ID
	for (int s = 0; s < 64; s++) {
		FZ[s] = a[FZig[s]] * QT[qtx][FZig[s]];	// 先反量化，后反Z型编码
	}

	// 5.反余弦变换IDCT
	double Temp_buf[8][8] = { 0 };
	for (int j = 0; j < 8; j++) {
		for (int u = 0; u < 8; u++) {
			for (int v = 0; v < 8; v++) {
				Temp_buf[j][u] += fastIDCT[j][v] * FZ[v * 8 + u];
			}
		}
	}
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			OUT_buf[j * 8 + i] = 0;
			for (int u = 0; u < 8; u++) { {
					OUT_buf[j * 8 + i] += fastIDCT[i][u] * Temp_buf[j][u];
				}
			}
		}
	}

	t4 += GetTickCount() - t3;

	// 6.程序结束
	return true;
}

// 图片解码
bool JPEG::DecodePhoto() {
	if (DRI_val != 0){
		std::cout << "DRI值不为0，无法解码" << endl;
		return false;
	}

	if (result == false)	return false;

	if (YCbCr_ratio == 0x111){	// 采用比为1:1:1时
		double y[64] = { 0 };
		double cb[64] = { 0 };
		double cr[64] = { 0 };
		int r[64] = { 0 };
		int g[64] = { 0 };
		int b[64] = { 0 };
		data_lp = 0;

		while (data_lp < datasize){
			if (true == ReadPart(y, 1)&&true == ReadPart(cb, 2)&&true == ReadPart(cr, 3)){
				for (int i = 0; i < 64; i++){
					r[i] = y[i] + 1.402*(cr[i]) + 128;
					g[i] = y[i] - 0.34414*(cb[i]) - 0.71414*(cr[i]) + 128;
					b[i] = y[i] + 1.772*(cb[i]) + 128;
					if (r[i] > 255)r[i] = 255;
					if (g[i] > 255)g[i] = 255;
					if (b[i] > 255)b[i] = 255;
					if (r[i] < 0)r[i] = 0;
					if (g[i] < 0)g[i] = 0;
					if (b[i] < 0)b[i] = 0;
				}
				// 恢复成8*8矩阵
				for (int y = 0; y < 8; y++){
					for (int x = 0; x < 8; x++){
						this->rgb_r[mcu_y + y][mcu_x + x] = r[y * 8 + x];
						this->rgb_g[mcu_y + y][mcu_x + x] = g[y * 8 + x];
						this->rgb_b[mcu_y + y][mcu_x + x] = b[y * 8 + x];
					}
				}
				// 每次长、宽均移动8个像素
				mcu_x += 8;
				if (mcu_x >= (mcu_w - 1)){
					mcu_x = 0;
					mcu_y += 8;
					if (mcu_y >= (mcu_h - 1)){	// 读取结束
						result = true;
						return true;
					}
				}
			}		
			else
				return false;
		}
		return false;
	}
	else if (YCbCr_ratio == 0x411) {	// 采用比为4:1:1时
		double y[4][64] = { 0 };

		double cb[64] = { 0 };
		double cr[64] = { 0 };
		int r[4][64] = { 0 };
		int g[4][64] = { 0 };
		int b[4][64] = { 0 };
		int uu = 0;
		data_lp = 0;

		while (data_lp < datasize){
			if (true == ReadPart(y[0], 1)&&true == ReadPart(y[1], 1)&&true == ReadPart(y[2], 1)&&
				true == ReadPart(y[3], 1)&&true == ReadPart(cb, 2)&&true == ReadPart(cr, 3)) {	// r
				for (int n = 0; n < 4; n++){
					for (int i = 0; i < 64; i++){
						uu = MapCrCb(i, n);
						r[n][i] = y[n][i] + 1.402*(cr[uu]) + 128;
						g[n][i] = y[n][i] - 0.34414*(cb[uu]) - 0.71414*(cr[uu]) + 128;
						b[n][i] = y[n][i] + 1.772*(cb[uu]) + 128;
						if (r[n][i] > 255)r[n][i] = 255;
						if (g[n][i] > 255)g[n][i] = 255;
						if (b[n][i] > 255)b[n][i] = 255;
						if (r[n][i] < 0)r[n][i] = 0;
						if (g[n][i] < 0)g[n][i] = 0;
						if (b[n][i] < 0)b[n][i] = 0;
					}
				}
				// 恢复成8*8矩阵
				for (int y = 0; y < 8; y++){
					for (int x = 0; x < 8; x++){
						this->rgb_r[mcu_y + y][mcu_x + x] = r[0][y * 8 + x];
						this->rgb_g[mcu_y + y][mcu_x + x] = g[0][y * 8 + x];
						this->rgb_b[mcu_y + y][mcu_x + x] = b[0][y * 8 + x];
						this->rgb_r[mcu_y + y][mcu_x + 8 + x] = r[1][y * 8 + x];
						this->rgb_g[mcu_y + y][mcu_x + 8 + x] = g[1][y * 8 + x];
						this->rgb_b[mcu_y + y][mcu_x + 8 + x] = b[1][y * 8 + x];
						this->rgb_r[mcu_y + 8 + y][mcu_x + x] = r[2][y * 8 + x];
						this->rgb_g[mcu_y + 8 + y][mcu_x + x] = g[2][y * 8 + x];
						this->rgb_b[mcu_y + 8 + y][mcu_x + x] = b[2][y * 8 + x];
						this->rgb_r[mcu_y + 8 + y][mcu_x + 8 + x] = r[3][y * 8 + x];
						this->rgb_g[mcu_y + 8 + y][mcu_x + 8 + x] = g[3][y * 8 + x];
						this->rgb_b[mcu_y + 8 + y][mcu_x + 8 + x] = b[3][y * 8 + x];
					}
				}
				// 每次长、宽均移动16个像素
				mcu_x += 16;
				if (mcu_x >= (mcu_w - 1)){
					mcu_x = 0;
					mcu_y += 16;
					if (mcu_y >= (mcu_h - 1)){
						result = true;
						return true;
					}
				}
			}
			else
				return false;
		}
		return false;
	}
	else {
	std::cout << "本程序仅支持解码采样比为1:1:1和4:1:1的图片，解码失败！" << endl;
	}
}

// 显示图片
bool JPEG::ShowPhoto(int w_x, int w_y) {
	if (result == false) {
		std::cout << "解码未成功，图片显示失败！" << endl;
		return false;
	}
	HDC hdc = GetWindowDC(GetDesktopWindow());

	for (int y = 0; y < jpg_hight; y++){
		for (int x = 0; x < jpg_width; x++){
			SetPixel(hdc, w_x + x, w_y + y, RGB(rgb_r[y][x], rgb_g[y][x], rgb_b[y][x]));
		}
	}
	return true;
}


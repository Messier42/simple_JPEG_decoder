#include "JPEG.h"
#include "GlobalVar.h"

JPEG::JPEG(char* filename){
	result = true;

	//��ͼ���ļ�
	myjpg.open(filename, ios::binary | ios::in);
	if (myjpg.good() == false) {
		std::cout << "�ļ���ʧ�ܣ�" << endl;
		result = false;
	}
		
	//��֤�ļ��Ƿ�ΪJPEGͼƬ
	unsigned short temp = 0;
	temp = myjpg.get();
	temp = (temp << 8) + myjpg.get();
	if (temp != 0xFFD8) {
		while (((temp = (temp << 8) | myjpg.get()) != 0xFFE0) && (temp != 0xFFD9));
		if (temp == 0xFFE0) {
			myjpg.seekg(2, ios::cur);	// �������ݳ���
			unsigned long tag = myjpg.get() << 64 | myjpg.get() << 32 |
				myjpg.get() << 16 | myjpg.get() << 8 | myjpg.get();		// ��ʶ��
			if (tag == 0x4A46494600)
				result = true;
			else {
				result = false;
				std::cout << "ͼƬ��ʽ����" << endl;
			}
		}
	}

	// ��ȡ�ļ�ͷ����Ϣ
	if (result == true) {
		//��ȡ�ļ���С
		myjpg.seekg(0, ios::end);
		this->jpg_size = (long)myjpg.tellg();

		// GetBit()���ݳ�ʼ��
		s_bit = 0;		// �Ѷ�ȡ��λ����ÿ�ζ�8λ��
		bittemp = 0;	// ��ǰλ��ֵ

		// ֱ��ϵ���Ĳ�ֱ���У��
		AC_correction[0] = 0xabcdef;
		AC_correction[1] = 0xabcdef;
		AC_correction[2] = 0xabcdef;

		// DecodePhoto()���ݳ�ʼ��
		mcu_x = 0;		// MCU��ʼx
		mcu_y = 0;		// MCU��ʼy

		DRI_val = 0;	//��ֱ����ۼƸ�λ�ļ��

		//�������ʼ��
		for (int n = 0; n < 4; n++) {
			for (int i = 0; i < 8; i++) {
				for (int j = 0; j < 8; j++) {
					QT[n][i * 8 + j] = 0;
				}
			}
		}

		//ͼƬ��ʼ����ͼƬ��Ϣ��ȡ
		if (ReadSOF0() && ReadDQT() && ReadDHT() && !ReadDRI() && ReadSOS()) {
			result = true;
			std::cout << "ͼƬ�򿪳ɹ���" << endl;
		}
		else {
			result = false;
			std::cout << "ͼƬ��ʧ�ܣ�" << endl;
		}
	}

	// ����׼������
	if(result == true){
		// ������������
		BuildHTree();

		// ����ͼƬ������
		if (YCbCr_ratio == 0x111)
		{
			int w = 0, h = 0;	// ��͸߰�����8*8������
			w = jpg_width / 8;
			if ((jpg_width % 8) > 0)w++;	// ��ȫ��
			h = jpg_hight / 8;
			if ((jpg_hight % 8) > 0)h++;	// ��ȫ��

			mcu_w = w * 8;		// ��ȫ��Ŀ�
			mcu_h = h * 8;		// ��ȫ��ĸ�
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
			int w = 0, h = 0;	// ��͸߰�����16*16������
			w = jpg_width / 16;
			if ((jpg_width % 16) > 0)w++;	// ��ȫ��
			h = jpg_hight / 16;
			if ((jpg_hight % 16) > 0)h++;	// ��ȫ��

			mcu_w = w * 16;		// ��ȫ��Ŀ�
			mcu_h = h * 16;		// ��ȫ��ĸ�
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
		//����ͼƬԭʼ���ݻ�����
		myjpg.seekg(data_start, ios::beg);
		datasize = jpg_size - data_start;
		data_buf = new unsigned char[jpg_size];
		myjpg.read((char*)data_buf, datasize);
		if (myjpg.gcount() == datasize)
			std::cout << "��ȡ����" << endl;
		else {
			std::cout << dec << myjpg.gcount() << '-' << datasize << endl;
		}
		data_lp = 0;//����������ָ��
		myjpg.close();
	}
}

bool JPEG::ReadSOF0() {
	unsigned short temp = 0;
	myjpg.seekg(0, ios::beg);

	while (((temp = (temp << 8) | myjpg.get()) != 0xFFC0) && (temp != 0xFFD9));
	if (temp == 0xFFC0)		// SOF0�α�Ǵ���
	{
		temp = myjpg.get();
		temp = (temp << 8) + myjpg.get();
		if (temp == 17)		// ���ݳ��� JFIFĬ����ɫ����ΪYCrCb���ʸöγ���Ϊ17�ֽ�
		{
			this->data_precision = myjpg.get();					// ��ȡ����		1�ֽ�
			this->jpg_hight = (myjpg.get() << 8) + myjpg.get();	// ��ȡͼ��߶�	2�ֽ�
			this->jpg_width = (myjpg.get() << 8) + myjpg.get();	// ��ȡͼ����	2�ֽ�
			this->colors = myjpg.get();							// ��ɫ������		1�ֽ�
			for (int i = 0; i < 3; i++)							// 3����ɫ����
				for (int j = 0; j < 3; j++)		// 0��ʾ��ɫ����ID��1��ʾˮƽ����4λ��/��ֱ����4λ���������ӣ�2��ʾ������ID
					this->color_info[i][j] = myjpg.get();
			// YCbCr������
			YCbCr_ratio = ((color_info[0][1] & 0xf0) >> 4) * (color_info[0][1] & 0x0f);
			YCbCr_ratio = (YCbCr_ratio << 4) | (((color_info[1][1] & 0xf0) >> 4) * (color_info[1][1] & 0x0f));
			YCbCr_ratio = (YCbCr_ratio << 4) | (((color_info[2][1] & 0xf0) >> 4) * (color_info[2][1] & 0x0f));
		}
		else {
			std::cout << "��������ܶ�ȡ��ɫ�ռ�ΪYCrCb��ͼ��SOF0��ȡʧ�ܣ�" << endl;
			return false;
		}
	}
	else {
		std::cout << "δ�ҵ�SOF0�α�ǣ�SOF0��ȡʧ�ܣ�" << endl;
		return false;
	}
		
	std::cout << "SOF0��ȡ�ɹ���" << endl;
	return true;
}

bool JPEG::ReadDQT() {
	unsigned short temp = 0, S = 0;
	myjpg.seekg(0, ios::beg);
	while (!myjpg.eof())
	{
		temp = 0;
		while (((temp = (temp << 8) + myjpg.get()) != 0xFFDB) && (temp != 0xFFC0));
		if (temp == 0xFFDB) {				// DCT�α�Ǵ���
			int size = (myjpg.get() << 8) | myjpg.get();	// ���ݳ���
			size -= 2;						// ��ȥ���ݳ��ȱ���ռ��2�ֽ�
			while (size > 0) {
				S = myjpg.get();
				size--;
				if ((S & 0xF0) == 0) {		// ��4λΪ0������������Ϊ8λ
					S = S & 0x0F;			// ��4λ��ʾ������ID
					for (int i = 0; i < 64; i++) {
						this->QT[S][i] = myjpg.get();
						size--;
					}
				}
				else {						// ��4λΪ1������������Ϊ16λ
					S = S & 0x0F;			// ��4λ��ʾ������ID
					for (int i = 0; i < 64; i++) {
						this->QT[S][i] = (myjpg.get() << 8) | (myjpg.get());
						size -= 2;
					}
				}
			}
		}
		else {
			std::cout << "DQT��ȡ�ɹ���" << endl;
			return true;
		}
	}
	std::cout << "DQT��ȡʧ�ܣ�" << endl;
	return false;
}

bool JPEG::ReadDHT() {
	unsigned short temp = 0, ID = 0, u = 0;;
	long ts = 0;
	myjpg.seekg(0, ios::beg);

	while (!myjpg.eof())			// ����JPEG�������DHT�Σ�ÿ���ν�����һ����������
	{
		temp = 0;
		ID = 0;
		while (((temp = (temp << 8) + myjpg.get()) != 0xFFC4) && (temp != 0xFFDA));
		if (temp == 0xFFC4)		// DHT�α�Ǵ���
		{
			int size = (myjpg.get() << 8) | myjpg.get();	// ���ݳ���
			size -= 2;
			while (size > 0)	// ����JPEG����һ��DHT�Σ��öΰ������й�������
			{
				ID = myjpg.get();			// ��ID�ͱ�����
				size--;
				ID = ((ID & 0x10) >> 3) | (ID & 0x01);	// ����IDת��Ϊ0~3

				HT_bit[ID][16] = 0;	
				for (int i = 0; i < 16; i++)
				{
					this->HT_bit[ID][i] = myjpg.get();	// ��ͬλ������������
					HT_bit[ID][16] += HT_bit[ID][i];	// ��ͬ��������֮��
					size--;
				}

				sp_HT_value[ID] = new int[(HT_bit[ID][16])];

				for (int i = 0; i < (HT_bit[ID][16]); i++)
				{
					this->sp_HT_value[ID][i] = myjpg.get();	// ÿ������ÿ�ֱ���ľ�������
					size--;
				}
			}
		}
		else {
			std::cout << "DHT��ȡ�ɹ���" << endl;
			return true;
		}
	}
	std::cout << "DHT��ȡʧ�ܣ�" << endl;
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
			std::cout << "�������ݲ�֧�ֽ��뺬DRI��ͼƬ��" << endl;
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
	if (temp == 0xFFDA)		// SOS�α�Ǵ���
	{
		myjpg.seekg(2, ios::cur);	// �������ݳ���
		s = myjpg.get();			// ��ɫ������
		for (int i = 0; i < s; i++)
		{
			myjpg.seekg(1, ios::cur);	// ������ɫ����ID
			temp = myjpg.get();			
			this->color_info[i][3] = temp >> 4;		// ��4λ��ֱ������ʹ�õĹ����������
			this->color_info[i][4] = temp & 0x0F;	// ��4λ����������ʹ�õĹ����������
		}
		myjpg.seekg(3, ios::cur);		// ����ѹ������ͼ������
		data_start = (long)myjpg.tellg();	// ͼ����Ϣ��ʼ��
	}
	else {
		std::cout << "SOS��ȡʧ�ܣ�" << endl;
		return false;
	}
	std::cout << "SOS��ȡ�ɹ���" << endl;
	return true;
}

bool JPEG::BuildHTree() {
	unsigned int temp = 0;
	int t1 = 0;	// ǰһ�����ֵ�λ��
	int lp = 0;

	// ����ÿ�����ֵ�λ��
	for (int i = 0; i < 4; i++) {
		sp_HT_tree[i] = new int[HT_bit[i][16]];	
		temp = 0;
		lp = 0;
		for (int ss = 0; ss < 16; ss++)
		{
			for (int ww = 0; ww < HT_bit[i][ss]; ww++)
			{
				sp_HT_tree[i][lp++] = ss + 1;	// �������λ��
			}
		}
	}

	// ����ÿ�����ֵ���ֵ
	for (int i = 0; i < 4; i++){
		temp = 0;
		for (int j = 0; j < HT_bit[i][16]; j++){
			if (j == 0){						// ��һ������Ϊ���ɸ�0
				if (sp_HT_tree[i][0] != 1){		// ��һ�����ֳ��ȴ���1
					t1 = sp_HT_tree[i][0];		// ���浱ǰ����λ��
					sp_HT_tree[i][0] <<= 16;	// ��λ������ֳ���
					temp = 0;					// ����Ϊ0
				}
				else {							// ��һ�����ֳ��ȵ���1
					t1 = sp_HT_tree[i][0];		// ���浱ǰ����λ��
					temp = 0;
					sp_HT_tree[i][0] <<= 16;	// ��λ������ֳ���
					temp = 0;					// ����Ϊ0
				}
			}
			else {								// ��������
				if (sp_HT_tree[i][j] > t1){		// ��ǰ����λ����ǰ�������λ����
					temp++;								// ���� = ǰ������� + 1 
					temp <<= (sp_HT_tree[i][j] - t1);	// �����ֺ�����0ֱ����������λ��
					t1 = sp_HT_tree[i][j];				// ���浱ǰ����λ��
					sp_HT_tree[i][j] <<= 16;			// ��λ������ֳ���
					sp_HT_tree[i][j] |= temp;			// ���浱ǰ����
				}
				else {							// ��ǰ����λ����ǰ�������λ��ͬ
					temp++;						// ���� = ǰ������� + 1 
					sp_HT_tree[i][j] <<= 16;
					sp_HT_tree[i][j] |= temp;	// ���浱ǰ����
				}
			}
		}
	}
	return true;
}

// ��ʾͼƬ��Ϣ
void JPEG::PrintInfo() {
	std::cout << "ͼƬ�ļ���С��" << (double)jpg_size / 1024 << "kb" << endl;
	std::cout << "��*��" << jpg_hight << "*" << jpg_width << endl;
	std::cout << "��ɫ���ȣ�" << data_precision << endl;
	std::cout << "��ɫ��������" << colors << endl;
	std::cout << "YCbCr�����ȣ�" << hex << YCbCr_ratio << endl << endl;

	// ����
	for (int n = 0; n < 4; n++){
		std::cout << "����" << n << ':' << endl;
		for (int i = 0; i < 8; i++){
			for (int j = 0; j < 8; j++){
				std::cout << dec << QT[n][i * 8 + j] << '\t';
			}
			std::cout << endl;
		}
	}
	std::cout << endl;

	// ��������
	for (int n = 0; n < 4; n++){
		for (int ui = 0; ui < 16; ui++){
			std::cout << dec << HT_bit[n][ui] << ' ';
		}
		std::cout << endl;
		std::cout << "��������" << dec << n + 1 << endl;
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

	// ��ɫ��Ϣ
	for (int i = 0; i < 3; i++){
		for (int j = 0; j < 5; j++){
			std::cout << hex << color_info[i][j] << '\t';
		}
		std::cout << endl;
	}

}

// ��λ��ȡ��Ϣ
unsigned JPEG::GetBit() {
	int x = 0, ee = 0;

	if (s_bit == 0){
		s_bit = 8;					// ÿ�ζ�ȡ8λ����
		if (data_lp >= (datasize - 2))return 0x0f;
		bittemp = data_buf[data_lp++];
		if (bittemp == 0xff){
			ee = data_buf[data_lp++];

			if (ee == 0xD9) {		// EOI
				data_lp -= 2;
				std::cout << "over! " << hex << 0xffd9 << endl;
				return 0x0f;
			}
			else if(ee != 0x00) {	// ��������
				std::cout << "error:" << hex << ee << endl;
				return 0x0f;
			}
		}
	}

	if (0 == (bittemp & 0x80))	// ��ǰλΪ0
		x = 0;
	else						// ��ǰλΪ1
		x = 1;
	bittemp <<= 1;				// ����һλ��ÿ�ζ�ȡ���λ
	s_bit--;
	return x;
}

// ���������
int JPEG::FindTree(unsigned int temp, int tree) {
	for (int i = 0; i < HT_bit[tree][16]; i++){
		if (temp == sp_HT_tree[tree][i]) {	// ƥ������
			return sp_HT_value[tree][i];	// ������ֵ
		}
	}
	return 0xffffffff;
}

// 8*8���ݿ����YCrCb��1=Y,2=Cb,3=Cr��
bool JPEG::ReadPart(double* OUT_buf, int YCbCr) {
	// 1.���û�����������
	// decode_val[i][0]��ʾ��i������ǰ��0������
	// decode_val[i][1]��ʾ��i��������ֵ
	int decode_val[64][2] = { 0 };
	unsigned short temp = 0;
	int c = 0, x = 0, sss = 0;
	int df = 0, ww = 1, sp = 0;


	t3 = GetTickCount();
	// ��ȡֱ��������1����
	for (int x1 = 0; x1 < 16; x1++)	// ���������볤��Ϊ1~16λ
	{
		if (0x0f == (df = GetBit()))
			return false;
		temp = (temp << 1) + df;
		c = FindTree(temp | ((x1 + 1) << 16), color_info[YCbCr - 1][3]);	// ����ֱ����������
		if (c != 0xffffffff) {	// ��ǰ��ֵ��ʾ��ֱ��������ֵ��λ��
			temp = x = 0;		// ��ʱ��������
			for (int i = 0; i < c; i++) {
				if (0x0f == (df = GetBit()))
					return false;
				temp = (temp << 1) + df;
				x++;
			}
			decode_val[sp++][1] = this->FindTable(temp, x);	// ����ֱ��������Ӧ����ֵ
			sss++;				// ������ += 1
			break;
		}
	}

	// ��ȡ����������63����
	while (ww){
		temp = x = 0;
		// ���������볤��Ϊ1~16λ
		for (int x1 = 0; x1 < 16; x1++) {
			if (0x0f == (df = GetBit()))
				return false;
			temp = (temp << 1) + df;
			// ���ҽ���������������0�Ŷ�Ӧ2�ţ�����1�Ŷ�Ӧ3�ţ�
			c = FindTree(temp | ((x1 + 1) << 16), color_info[YCbCr - 1][4] + 2);

			if ((c != 0xffffffff) && (c != 0)) {
				temp = 0;
				// ��ǰ��ֵ��4λ��ʾ��ǰ��ֵǰ���ж��ٸ���������
				decode_val[sp][0] = (c & 0xf0) >> 4;
				// ������ += ��ǰ��0����ǰ��Ϊ0�ķ�����	
				sss += decode_val[sp][0];
				// ��ǰ��ֵ��4λ��ʾ�ý���������ֵ�Ķ�����λ��
				c = c & 0x0f;						
				if (c != 0) {	// ��������ֵ��Ϊ0
					for (int i = 0; i < c; i++) {
						if (0x0f == (df = GetBit()))
							return false;
						temp = (temp << 1) + df;
						x++;
					}
					// ����ֱ��������Ӧ����ֵ
					decode_val[sp++][1] = this->FindTable(temp, x);
					sss++;	// ������ += 1
					// ���Ѿ�����63����������ʱֹͣ���뽻������
					if ((sss == 64) || (sp > 63))
						ww = 0;
					break;
				}
				else if (c == 0) {	// ��������ֵΪ0
					decode_val[sp++][1] = 0;			
					sss++;	// ������ += 1
					// ���Ѿ�����63����������ʱֹͣ���뽻������
					if ((sss == 64) || (sp > 63))
						ww = 0;
					break;
				}
			}
			// ��ǰ��ֵΪ0˵������������ȫΪ0�������ٶ��뽻������
			else if (c == 0) {
				ww = 0;
				break;
			}
			// ���볤�Ȳ����ܴ���16λ
			if (x1 == 15)
				return false;
		}
	}

	// 2.ֱ��ϵ���Ĳ�ֱ���У��
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

	// 3.�γ̱���ָ���8*8����
	int sum = 1;

	for (int i = 1; i < 64; i++){
		if ((decode_val[i][0] == 0) && (decode_val[i][1] == 0))
			break;

		for (int j = 0; j < decode_val[i][0]; j++) {	// ���0
			a[sum++] = 0;
		}
		if (sum > 63)
			return false;

		a[sum++] = decode_val[i][1];					// ����0����
		if (sum > 63)
			break;
	}

	// 4.��Z�ͱ���+������
	int FZ[64] = { 0 };
	int qtx = 0;

	qtx = color_info[YCbCr - 1][2];	//	��������ID
	for (int s = 0; s < 64; s++) {
		FZ[s] = a[FZig[s]] * QT[qtx][FZig[s]];	// �ȷ���������Z�ͱ���
	}

	// 5.�����ұ任IDCT
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

	// 6.�������
	return true;
}

// ͼƬ����
bool JPEG::DecodePhoto() {
	if (DRI_val != 0){
		std::cout << "DRIֵ��Ϊ0���޷�����" << endl;
		return false;
	}

	if (result == false)	return false;

	if (YCbCr_ratio == 0x111){	// ���ñ�Ϊ1:1:1ʱ
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
				// �ָ���8*8����
				for (int y = 0; y < 8; y++){
					for (int x = 0; x < 8; x++){
						this->rgb_r[mcu_y + y][mcu_x + x] = r[y * 8 + x];
						this->rgb_g[mcu_y + y][mcu_x + x] = g[y * 8 + x];
						this->rgb_b[mcu_y + y][mcu_x + x] = b[y * 8 + x];
					}
				}
				// ÿ�γ�������ƶ�8������
				mcu_x += 8;
				if (mcu_x >= (mcu_w - 1)){
					mcu_x = 0;
					mcu_y += 8;
					if (mcu_y >= (mcu_h - 1)){	// ��ȡ����
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
	else if (YCbCr_ratio == 0x411) {	// ���ñ�Ϊ4:1:1ʱ
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
				// �ָ���8*8����
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
				// ÿ�γ�������ƶ�16������
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
	std::cout << "�������֧�ֽ��������Ϊ1:1:1��4:1:1��ͼƬ������ʧ�ܣ�" << endl;
	}
}

// ��ʾͼƬ
bool JPEG::ShowPhoto(int w_x, int w_y) {
	if (result == false) {
		std::cout << "����δ�ɹ���ͼƬ��ʾʧ�ܣ�" << endl;
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


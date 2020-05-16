// JPEG_decoder.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "JPEG.h"
#include "GlobalVar.h"

int main()
{
	char jpgFilename[] = "TestImgs\\Messier42.jpg";
	JPEG jpg1 = JPEG(jpgFilename);

	//jpg1.PrintInfo(); //图片信息显示

	t1 = GetTickCount();//
	if (jpg1.DecodePhoto())
	{
		t2 = GetTickCount();
		jpg1.ShowPhoto(0, 0);
		cout << dec << "解码用时：" << t2 - t1 << " ms" << endl;
		cout << dec << "Read_part用时：" << t4 << " ms" << endl;
	}
	else
	{
		t2 = GetTickCount();
		cout << "解码失败！" << endl;
		cout << dec << (int)jpg1.myjpg.tellg() << endl;
		jpg1.ShowPhoto(0, 0);
	}

	system("PAUSE");
	return 0;
}

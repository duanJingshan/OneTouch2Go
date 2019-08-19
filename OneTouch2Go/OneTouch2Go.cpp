// OneTouch2Go.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <string>
#include <fstream>
#include <atlstr.h>
using namespace std;

//层次名与执行程序名的对应表,填写这个表以后，本软件就能根据配置文件中的参数自动打开相应的程序，并提供启动参数
//命令行的格式为：程序名 设备号 实体号
struct functionMap_t {
	string LayerName;  
	string funcitonName;
	int len; //层次名字字符串的长度
}aFunctionMap[7];
string getFunction(string strName)
{
	int i;
	for (i = 0; i < 7; i++)
	{
		if (aFunctionMap[i].LayerName.empty() || strName.empty()) {
			continue;
		}
		if (strName.compare(aFunctionMap[i].LayerName) == 0) {
			//找到
			return aFunctionMap[i].funcitonName;
		}
	}
	//没找到
	return NULL;
}
/*
string getFunction(char* cpName)
{
	int i;
	for (i = 0; i < 7; i++)		
	{
		if (aFunctionMap[i].LayerName.empty()) {
			continue;
		}
		if (strncmp(aFunctionMap[i].LayerName.c_str(), cpName, strlen(aFunctionMap[i].LayerName.c_str())) == 0) {
			//找到
			return aFunctionMap[i].funcitonName;
		}
	}
	//没找到
	return NULL;
}
*/
constexpr auto IDLE4DEV_STATE = 0;
constexpr auto IDLE4LAYER_STATE = 1;
constexpr auto IDLE4ENTITY_STATE = 2;
constexpr auto ENTITY_STATE = 4;

constexpr auto DEV_EVENT = 0;
constexpr auto LAYER_EVENT = 1;
constexpr auto ENTITY_EVENT = 2;
constexpr auto END_EVENT = 3;
constexpr auto UNKNOWN_EVENT = 4;

#define MAX_NUMBER 20
//从设备文件中需要读出：
//1、有多少个设备，每个设备的编号（以整数保留）
//2、每个设备有多少个层次，每个层次的名称，查映射表，以准备system命令
//3、每个层次有多少个实体，每个实体的编号（以整数保存）
//4、逐个执行各条命令，并给与设备号，实体号参数，（层次对应程序本身）
//注，程序打开后如果要调整位置，根据设备文件中的布局参数完成
int giDevNumber;
int gaDevID[MAX_NUMBER];  //a[i],记录dev i 的ID号
int gaLayerNumber[MAX_NUMBER]; //a[i],表示device i有多少层
char* gcpLayerName[MAX_NUMBER][MAX_NUMBER]; //a[i][j],记录device i,第j层的名字
int gaEntityNumber[MAX_NUMBER][MAX_NUMBER]; //a[i][j]，表示device i,第j层有多少个实体
int gaEntityID[MAX_NUMBER][MAX_NUMBER][MAX_NUMBER]; //a[i][j][k],表示device i 的第i层，第k个实体的ID号
void myStrcpy(char* dst, string& src) //只保留ASCII码大于32的字符，32为空格，以下的都是控制字符
{
	size_t i, j;
	j = 0;
	for (i = 0; i < strlen(src.c_str()); i++) {
		if (src[i] > 32 || src[i] < 0) {
			dst[j] = src.c_str()[i];
			j++;
		}
	}
	dst[j] = 0;
}

int getEvent(string& strpLine)
{
	int retval;
	retval = strpLine.find("deviceID");
	if (retval >= 0) {
		return DEV_EVENT;
	}
	retval = strpLine.find("layer");
	if (retval >= 0) {
		return LAYER_EVENT;
	}
	retval = strpLine.find("entityID");
	if (retval >= 0) {
		return ENTITY_EVENT;
	}
	retval = strpLine.find("-------------");
	if (retval >= 0) {
		return END_EVENT;
	}
	return UNKNOWN_EVENT;
}
int idle4DevFsm(int iE, string& strpLine)
{
	string strInt;
	int iS = IDLE4DEV_STATE;
	switch (iE) {
	case DEV_EVENT:
		//读取设备号，记录设备号
		strInt = strpLine.substr(strpLine.find("=") + 1, strpLine.length() - strpLine.find("="));
		gaDevID[giDevNumber] = atoi(strInt.c_str());

		giDevNumber++;

		//下一个应该是层次号
		iS = IDLE4LAYER_STATE;
		break;
	case LAYER_EVENT:
		//状态不变，因为突然出现的层次号，无法定位属于哪个设备
		break;
	case ENTITY_EVENT:
		//状态不变
		break;
	case END_EVENT:
		//状态不变
		break;
	case UNKNOWN_EVENT:
		//状态不变
		break;
	}
	return iS; 
}
int idle4LayerFsm(int iE, string& strpLine)
{
	string strTmp;
	char* cpTmp;
	int iLayerNumber;
	int iS = IDLE4LAYER_STATE;

	switch (iE) {
	case DEV_EVENT:
		//意外停止，当作是新的DEV起始
		iS = idle4LayerFsm(iE, strpLine);
		break;
	case LAYER_EVENT:
		//截断取得名字
		strTmp = strpLine.substr(strpLine.find("=") + 1, strpLine.length() - strpLine.find("="));
		//整理内容，只保留大于32的字符
		cpTmp = (char*)malloc(strlen(strTmp.c_str())+10);
		if (cpTmp == NULL) {
			return iS;
		}
		myStrcpy(cpTmp, strTmp);
		iLayerNumber = gaLayerNumber[giDevNumber - 1];//当前设备，当前的层次序号
		//将层次名称记录下来
		gcpLayerName[giDevNumber - 1][iLayerNumber] = cpTmp;

		gaLayerNumber[giDevNumber - 1]++;

		//下一个应该是实体号
		iS = IDLE4ENTITY_STATE;
		break;
	case ENTITY_EVENT:
		//本状态是从设备进入。本来应该寻找层次，但是却找到了实体，缺少层次说明，无法定位，意外的错误，状态不变，跳过

		break;
	case END_EVENT:
		//意外，但是明确表示了要结束，退到从找新设备开始，中间的内容全部放弃
		iS = IDLE4DEV_STATE;
		break;
	case UNKNOWN_EVENT:
		//跳过，保持状态，继续
		break;
	}
	return iS; //状态不变
}
int idle4EntityFsm(int iE, string& strpLine)
{
	string strTmp;
	int iLayerNumber;
	int iEntityNumber;

	int iS = IDLE4ENTITY_STATE;
	switch (iE) {
	case DEV_EVENT:
		//新的DEV开始
		iS = idle4DevFsm(iE, strpLine);
		break;
	case LAYER_EVENT:
		//新的LAYER
		iS = idle4LayerFsm(iE, strpLine);
		break;
	case ENTITY_EVENT:
		//取得实体号
		strTmp = strpLine.substr(strpLine.find("=") + 1, strpLine.length() - strpLine.find("="));

		iLayerNumber = gaLayerNumber[giDevNumber - 1];//当前设备，当前的层次编号
		iEntityNumber = gaEntityNumber[giDevNumber - 1][iLayerNumber - 1];//当前设备，当前层次，当前实体序号
		//把当前设备，当前层次，当前实体编号记录下来，
		gaEntityID[giDevNumber - 1][iLayerNumber - 1][iEntityNumber] = atoi(strTmp.c_str());
		gaEntityNumber[giDevNumber - 1][iLayerNumber - 1]++;

		iS = ENTITY_STATE;
		break;
	case END_EVENT:
		iS = IDLE4ENTITY_STATE;
		break;
	case UNKNOWN_EVENT:
		//状态不变
		break;
	}
	return iS; 
}
int EntityFsm(int iE, string& strpLine)
{
	int iS = ENTITY_STATE;
	switch (iE) {
	case DEV_EVENT:
		//新dev
		iS = idle4DevFsm(iE, strpLine);
		break;
	case LAYER_EVENT:
		//新layer
		iS = idle4LayerFsm(iE, strpLine);
		break;
	case ENTITY_EVENT:
		iS = idle4EntityFsm(iE, strpLine);
		break;
	case END_EVENT:
		iS = idle4EntityFsm(iE, strpLine);
		break;
	case UNKNOWN_EVENT:
		//保持状态不变
		break;
	}
	return iS; 
}
void readMapFile(ifstream& f)
{
	string strTmp;
	string csLayer;
	string csFunc;
	char* cpLayer;
	char* cpFunc;
	int i = 0;
	while (!f.eof()) {
		getline(f, strTmp);
		if (strTmp.empty()) {
			continue;
		}
		if (strTmp.find("=") < 0) {
			continue;
		}
		csLayer = strTmp.substr(0,strTmp.find("=") - 1);
		cpLayer = (char*)malloc(strlen(csLayer.c_str()) + 100);
		if (cpLayer == NULL) {
			return;
		}
		myStrcpy(cpLayer, csLayer);

		csFunc = strTmp.substr(strTmp.find("=") + 1, strTmp.length() - strTmp.find("="));
		cpFunc = (char*)malloc(strlen(csFunc.c_str())+100);
		if (cpFunc == NULL) {
			free(cpLayer);
			return;
		}
		myStrcpy(cpFunc, csFunc);

		aFunctionMap[i].LayerName = cpLayer;
		free(cpLayer); //？？？
		aFunctionMap[i].funcitonName = cpFunc;
		free(cpFunc);
		i++;
	}
}
int main()
{
	int iState,iEvent;
	int i,j,k;
	CString csCmd;
	CString csTmp;
	string strTmp;

	//打开映射文件
	ifstream mapFile("map.txt");
	if (!mapFile.is_open())
	{
		cout << "没有找到map.txt文件";
		return 0;
	}
	readMapFile(mapFile);
	mapFile.close();
	//打开配置文件
	ifstream cfgFile("ne.txt");
	if (!cfgFile.is_open())
	{
		return 0;
	}
	giDevNumber = 0;
	for (i = 0; i < MAX_NUMBER; i++) {
		gaLayerNumber[i] = 0; //a[i],表示device i有多少层
		gaDevID[i] = 0;
		for (j = 0; j < MAX_NUMBER; j++) {
			gcpLayerName[i][j] = NULL; //a[i][j],是device i,第j层的名字
			gaEntityNumber[i][j] = 0;//a[i][j]，表示device i,第j层有多少个实体
			for (k = 0; k < MAX_NUMBER; k++) {
				gaEntityID[i][j][k] = 0;
			}
		}
	}

	//遍历配置文件
	//通过设备号，层次名，和实体号，得到四个参数组:basic, lower , upper，peer
	iState = IDLE4DEV_STATE;
	while (!cfgFile.eof()) {
		getline(cfgFile, strTmp);
		iEvent = getEvent(strTmp);
		switch (iState) {
		case IDLE4DEV_STATE:
			iState = idle4DevFsm(iEvent, strTmp);
			break;
		case IDLE4LAYER_STATE:
			iState = idle4LayerFsm(iEvent, strTmp);
			break;
		case IDLE4ENTITY_STATE:
			iState = idle4EntityFsm(iEvent, strTmp);
			break;
		case ENTITY_STATE:
			iState = EntityFsm(iEvent, strTmp);
			break;
		default:
			iState = IDLE4DEV_STATE;

		}

	}
	cfgFile.close();
	//逐个运行程序
	cout << "读出网元 "<<giDevNumber<<" 个"<<endl;
	for (i = 0; i < giDevNumber; i++) {
		for (j = 0; j < gaLayerNumber[i]; j++) {
			//csCmd = "./";
			csCmd = getFunction(gcpLayerName[i][j]).c_str();
			if (csCmd.IsEmpty())
				continue;
			for (k = 0; k < gaEntityNumber[i][j]; k++) {
				//设备号
				//csTmp.Format("%d",gaDevID[i]);
				//csCmd += " ";
				//csCmd += csTmp;
				//取得实体号
				//csTmp.Format("%d",gaEntityID[i][j][k]);
				//csCmd += " ";
				//csCmd += csTmp;
				csTmp.Format("%d %d", gaDevID[i], gaEntityID[i][j][k]);
				ShellExecute(NULL, _T("open"), csCmd, csTmp, NULL, SW_SHOWNORMAL);
				//ShellExecute(NULL,_T("open"),csCmd,NULL,NULL,SW_SHOWNORMAL);
				cout << "启动网元 " << gaDevID[i] << " 的" << csCmd << "层 实体 " << gaEntityID[i][j][k] << endl;
			}
		}
	}
	/*
	for (i = 0; i < MAX_NUMBER; i++) {
		gaLayerNumber[i] = 0; //a[i],表示device i有多少层
		gaDevID[i] = 0;
		for (j = 0; j < MAX_NUMBER; j++) {
			if (gcpLayerName[i][j] != NULL) { //a[i][j],是device i,第j层的名字
				free(gcpLayerName[i][j]);
			}
		}
	}
	*/
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件

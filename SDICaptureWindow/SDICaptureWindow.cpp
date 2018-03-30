// SDICaptureWindow.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <Windows.h>
#include <vector>
#include <TlHelp32.h>
#include <fstream>

#define STRING_MAX_LENGTH 256
#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))

struct WinAttribution
{
	HWND hwnd;
	HWND parent;
	RECT rect;
	TCHAR className[STRING_MAX_LENGTH];
	TCHAR name[STRING_MAX_LENGTH];
};

struct ProcessWinInfo
{
	unsigned int pid;
	unsigned int threadCount;
	TCHAR name[STRING_MAX_LENGTH];
	struct WinInfo
	{
		HWND hwnd;
		unsigned int threadId;
		WinAttribution attri;
	};
	std::vector<WinInfo> winInfos;
};

size_t GetWindowsList(std::vector<HWND>& winList);
bool GetWindowAttribution(HWND hwnd, WinAttribution& outAttribution);

bool GetWindowAttribution(HWND hwnd, WinAttribution& outAttribution)
{
	if (hwnd == NULL || !IsWindow(hwnd))
		return false;
	RECT rect = { 0 };
	if (!GetWindowRect(hwnd, &rect))
		return false;
	memset(&outAttribution, 0, sizeof(outAttribution));
	HWND parent = GetParent(hwnd);
	int classNameLen = GetClassName(hwnd, outAttribution.className, (ARRAY_COUNT(outAttribution.className)-1));
	if (classNameLen >= 0) outAttribution.className[classNameLen] = 0;
	int nameLen = GetWindowText(hwnd, outAttribution.name, (ARRAY_COUNT(outAttribution.name) - 1));
	if (nameLen >= 0) outAttribution.name[nameLen] = 0;
	outAttribution.hwnd = hwnd;
	outAttribution.parent = parent;
	outAttribution.rect = rect;
	return true;
}

//根据进程名获取进程ID
BOOL GetPidByProcessName(TCHAR *pProcess, DWORD*dwPid)
{
	HANDLE hSnapshot;
	PROCESSENTRY32 lppe;
	//创建系统快照 
	hSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);


	if (hSnapshot == NULL)
		return FALSE;


	//初始化 lppe 的大小 
	lppe.dwSize = sizeof(lppe);


	//查找第一个进程 
	if (!::Process32First(hSnapshot, &lppe))
		return FALSE;
	do
	{
		if (_tcscmp(lppe.szExeFile, pProcess) == 0)
		{
			*dwPid = lppe.th32ProcessID;
		}
	} while (::Process32Next(hSnapshot, &lppe)); //查找下一个进程  


	return TRUE;
}

bool GetHwndByPid(DWORD dwProcessID, const std::vector<HWND>& allWinList, std::vector<ProcessWinInfo::WinInfo>& winInfoList)
{
	//std::vector<HWND> winList;
	//size_t winCount = GetWindowsList(allWinList);
	for (size_t i = 0; i < allWinList.size(); i++)
	{
		HWND hWnd = allWinList[i];
		DWORD pid = 0;
		//根据窗口句柄获取进程ID
		DWORD dwTheardId = ::GetWindowThreadProcessId(hWnd, &pid);

		if (dwTheardId != 0)
		{
			if (pid == dwProcessID)
			{
				ProcessWinInfo::WinInfo winfo;
				winfo.hwnd = hWnd;
				winfo.threadId = dwTheardId;
				GetWindowAttribution(hWnd, winfo.attri);
				winInfoList.push_back(winfo);
				//return hWnd;
			}
		}
	}
	return true;
}

//根据进程ID获取窗口句柄
bool GetHwndByPid(DWORD dwProcessID, std::vector<ProcessWinInfo::WinInfo>& winInfoList)
{
	//返回Z序顶部的窗口句柄
	HWND hWnd = ::GetTopWindow(0);

	if(winInfoList.size()>0) winInfoList.clear();
	while (hWnd)
	{
		DWORD pid = 0;
		//根据窗口句柄获取进程ID
		DWORD dwTheardId = ::GetWindowThreadProcessId(hWnd, &pid);


		if (dwTheardId != 0)
		{
			if (pid == dwProcessID)
			{
				ProcessWinInfo::WinInfo winfo;
				winfo.hwnd = hWnd;
				winfo.threadId = dwTheardId;
				winInfoList.push_back(winfo);
				//return hWnd;
			}
		}
		//返回z序中的前一个或后一个窗口的句柄
		hWnd = ::GetNextWindow(hWnd, GW_HWNDNEXT);
	}
	return true;
}

bool GetAllProcessWinInfos(std::vector<ProcessWinInfo>& allProcessWinInfo)
{
	HANDLE hSnapshot;
	PROCESSENTRY32 lppe;
	//创建系统快照 
	hSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);


	if (hSnapshot == NULL)
		return FALSE;

	std::vector<HWND> winList;
	size_t winCount = GetWindowsList(winList);
	if (winCount <= 0)
		return false;

	//初始化 lppe 的大小 
	lppe.dwSize = sizeof(lppe);


	//查找第一个进程 
	if (!::Process32First(hSnapshot, &lppe))
		return FALSE;
	allProcessWinInfo.clear();
	do
	{
		ProcessWinInfo winInfo;
		winInfo.pid = lppe.th32ProcessID;
		_tcscpy_s(winInfo.name, sizeof(winInfo.name)/sizeof(winInfo.name[0]), lppe.szExeFile);
		winInfo.threadCount = lppe.cntThreads;
		GetHwndByPid(winInfo.pid, winList, winInfo.winInfos);
		allProcessWinInfo.push_back(winInfo);
	} while (::Process32Next(hSnapshot, &lppe)); //查找下一个进程  


	return TRUE;
}

BOOL CALLBACK enumWinProc(HWND hwnd, LPARAM lParam)
{
	std::vector<HWND>* winList = (std::vector<HWND>*)lParam;
	if (winList)
	{
		LONG exStyle = ::GetWindowLong(hwnd, GWL_EXSTYLE);
		if (((GetWindowLong(hwnd, GWL_STYLE) & WS_VISIBLE)
			 /*&& GetParent(hwnd) == NULL*/)  //只获取顶层可见窗口
			//|| (exStyle & WS_EX_LAYERED)	//如果是WS_EX_LAYERED也获取
			)
		{
			winList->push_back(hwnd);
		}
		//printf("Window %u 非可见或者parent不为NULL被跳过。\n", (unsigned int)hwnd);
	}
	return true;
}

size_t GetWindowsList(std::vector<HWND>& winList)
{
	if(!EnumWindows(enumWinProc, (LPARAM)&winList))
	{ 
		return 0;
	}
	return winList.size();
}

//根据进程ID获取窗口句柄
HWND GetHwndByPid(DWORD dwProcessID)
{
	//返回Z序顶部的窗口句柄
	HWND hWnd = ::GetTopWindow(0);


	while (hWnd)
	{
		DWORD pid = 0;
		//根据窗口句柄获取进程ID
		DWORD dwTheardId = ::GetWindowThreadProcessId(hWnd, &pid);


		if (dwTheardId != 0)
		{
			if (pid == dwProcessID)
			{
				return hWnd;
			}
		}
		//返回z序中的前一个或后一个窗口的句柄
		hWnd = ::GetNextWindow(hWnd, GW_HWNDNEXT);


	}
	return hWnd;
}

void startCaptureWindow(HWND hwnd)
{
	if (NULL == hwnd || !IsWindow(hwnd))
		return;
	RECT srcWinRect = { 0 };
	if (!GetWindowRect(hwnd, &srcWinRect))
		return;
	int width = srcWinRect.right - srcWinRect.left;
	int height = srcWinRect.bottom - srcWinRect.top;
	unsigned char* pdstData = NULL;
	HGDIOBJ old_bmp = NULL;
	HDC dsthdc = NULL;
	HDC srchdc = NULL;
	TCHAR fileName[256] = { 0 };
	swprintf_s(fileName, ARRAY_COUNT(fileName), _T("caputreed_%dx%d.rgb32"), width, height);
	std::ofstream capturedImg(fileName, std::ios::out | std::ios::binary);
	while (true)
	{
		if (pdstData == NULL)
		{
			BITMAPINFO bi = { 0 };
			BITMAPINFOHEADER *bih = &bi.bmiHeader;
			bih->biSize = sizeof(BITMAPINFOHEADER);
			bih->biBitCount = 32;
			bih->biWidth = width;
			bih->biHeight = height;
			bih->biPlanes = 1;
			dsthdc = CreateCompatibleDC(NULL);
			HBITMAP dstbmp = CreateDIBSection(dsthdc, &bi,
				DIB_RGB_COLORS, (void**)&pdstData,	NULL, 0);
			old_bmp = SelectObject(dsthdc, dstbmp);
		}
		if (srchdc == NULL)
			srchdc = GetDC(hwnd);

		BitBlt(dsthdc, 0, 0, width, height,
			srchdc, 0, 0, SRCCOPY);

		capturedImg.write((char*)pdstData, width * height * 4);

		Sleep(100);
	}
	ReleaseDC(NULL, srchdc);
	ReleaseDC(NULL, dsthdc);
}
	HMODULE get_system_module(const char *module)
	{
		char sys[MAX_PATH] = { 0 };
		GetSystemDirectoryA(sys, MAX_PATH);
		std::string base_path;

		base_path += sys;
		base_path += "\\";
		base_path += module;
		return GetModuleHandleA(base_path.c_str());
	}
int main()
{

	//bool hookLayeredWindowPaint()
	{
		HMODULE dxgi_module = get_system_module("User32.dll");
		if (!dxgi_module) {
			OutputDebugString(L"WinCapture: get module failed\n");
			return false;
		}
//		void* origin_func_address = GetProcAddress(dxgi_module, L"");
		//	origin_func_address = get_offset_addr(dxgi_module,
		//	m_rootLogic->m_offsets.dxgi.present);
//		return true;
	}
	printf("=============application start=========================\n");
	//std::vector<HWND> winList;
	//size_t winCount = GetWindowsList(winList);
	//printf("Windows count = %u from EnumWindows.\n", winCount);
	std::vector<ProcessWinInfo> allProcessWinInfo;
	if (GetAllProcessWinInfos(allProcessWinInfo))
	{
		for (size_t i = 0; i < allProcessWinInfo.size(); i++)
		{
			const ProcessWinInfo& proWinInfo = allProcessWinInfo[i];
			if (proWinInfo.winInfos.size() <= 0)
				continue;
			TCHAR outmsg[1024] = { 0 };
			swprintf_s(outmsg, sizeof(outmsg)/sizeof(outmsg[0]), _T("%-8d%-64s%4d\n"), proWinInfo.pid, proWinInfo.name, proWinInfo.winInfos.size());
			wprintf(outmsg);
		}
	}

	while (true)
	{
		printf("输入进程ID打印进程窗口的详细信息：");
		unsigned int inPID = -1;
		scanf_s("%u", &inPID);
		if (inPID != -1)
		{
			printf_s(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
			for (size_t i = 0; i < allProcessWinInfo.size(); i++)
			{
				const ProcessWinInfo& proWinInfo = allProcessWinInfo[i];
				if (proWinInfo.pid == inPID)
				{
					for (size_t j = 0; j < proWinInfo.winInfos.size(); j++)
					{
						const ProcessWinInfo::WinInfo& winInfo = proWinInfo.winInfos[j];
						HWND hwnd = winInfo.hwnd;
						LONG exStyle = ::GetWindowLong(hwnd, GWL_EXSTYLE);
						bool isLayeredWin = exStyle & WS_EX_LAYERED;
						int w = winInfo.attri.rect.right - winInfo.attri.rect.left;
						int h = winInfo.attri.rect.bottom - winInfo.attri.rect.top;
						wprintf_s(_T("\t%-8u x=%-5d y=%-5d w=%-5d h=%-5d pid=%-8u %s class=%s Layer=%d thread=%u\n"), hwnd, winInfo.attri.rect.left, winInfo.attri.rect.top, w, h,
							winInfo.attri.parent, winInfo.attri.name, winInfo.attri.className, isLayeredWin, winInfo.threadId);
					}
				}
			}
			printf_s("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
		}
		else
			break;
	}
	printf("输入需要捕屏的进程：");
	unsigned int inPid = -1;
	scanf_s("%u", &inPid);
	if (inPid != -1)
	{
		for (size_t i = 0; i < allProcessWinInfo.size(); i++)
		{
			const ProcessWinInfo& proWinInfo = allProcessWinInfo[i];
			if (proWinInfo.pid == inPid)
			{
				HWND hwnd = proWinInfo.winInfos[0].hwnd;
				startCaptureWindow(hwnd);
			}
		}
	}
	
	system("pause");
    return 0;
}


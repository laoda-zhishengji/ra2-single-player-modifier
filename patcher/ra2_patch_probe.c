#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

static const wchar_t *kPath=L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static DWORD find_target(void){HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(s==INVALID_HANDLE_VALUE)return 0;PROCESSENTRY32W e={0};e.dwSize=sizeof(e);DWORD r=0;if(Process32FirstW(s,&e))do{if(!_wcsicmp(e.szExeFile,L"game.exe")){HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);if(p){wchar_t q[MAX_PATH];DWORD n=MAX_PATH;if(QueryFullProcessImageNameW(p,0,q,&n)&&!_wcsicmp(q,kPath))r=e.th32ProcessID;CloseHandle(p);}}}while(!r&&Process32NextW(s,&e));CloseHandle(s);return r;}
int wmain(void){DWORD pid=find_target();if(!pid){wprintf(L"未找到目标 game.exe。\n");return 2;}HANDLE p=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,pid);if(!p)return 3;BYTE b[16],power[16];SIZE_T n=0;if(!ReadProcessMemory(p,(void*)0x00494670,b,sizeof(b),&n)||n!=sizeof(b)||!ReadProcessMemory(p,(void*)0x004F2D9B,power,sizeof(power),&n)||n!=sizeof(power)){wprintf(L"无法读取候选代码位置。\n");CloseHandle(p);return 4;}wprintf(L"pid=%lu address=0x00494670 bytes=",pid);for(int i=0;i<16;i++)wprintf(L"%02X",b[i]);wprintf(L"\npower_candidate=0x004F2D9B bytes=");for(int i=0;i<16;i++)wprintf(L"%02X",power[i]);wprintf(L"\n");CloseHandle(p);return 0;}

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>
static const wchar_t kPath[]=L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static DWORD pid(void){HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32W e={0};DWORD r=0;e.dwSize=sizeof(e);if(s!=INVALID_HANDLE_VALUE&&Process32FirstW(s,&e))do{if(!_wcsicmp(e.szExeFile,L"game.exe")){HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);wchar_t q[MAX_PATH];DWORD n=MAX_PATH;if(p&&QueryFullProcessImageNameW(p,0,q,&n)&&!_wcsicmp(q,kPath))r=e.th32ProcessID;if(p)CloseHandle(p);}}while(!r&&Process32NextW(s,&e));if(s!=INVALID_HANDLE_VALUE)CloseHandle(s);return r;}
int wmain(void){DWORD id=pid();if(!id)return 2;HANDLE p=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,id);if(!p)return 3;DWORD root=0,v=0;SIZE_T n=0;if(!ReadProcessMemory(p,(void*)0x00A35DB4,&root,4,&n)||n!=4){CloseHandle(p);return 4;}wprintf(L"global=0x%08lX\\n",(unsigned long)root);for(DWORD off=0x52B8;off<=0x52D4;off+=4){if(ReadProcessMemory(p,(void*)(uintptr_t)(root+off),&v,4,&n)&&n==4)wprintf(L"+0x%lX=%lu (0x%08lX)\\n",(unsigned long)off,(unsigned long)v,(unsigned long)v);}CloseHandle(p);return 0;}

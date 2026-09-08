#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static const wchar_t path[]=L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static DWORD pid(void){HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32W e={0};DWORD r=0;e.dwSize=sizeof(e);if(s==INVALID_HANDLE_VALUE)return 0;if(Process32FirstW(s,&e))do{if(!_wcsicmp(e.szExeFile,L"game.exe")){HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);wchar_t q[MAX_PATH];DWORD n=MAX_PATH;if(h&&QueryFullProcessImageNameW(h,0,q,&n)&&!_wcsicmp(q,path))r=e.th32ProcessID;if(h)CloseHandle(h);}}while(!r&&Process32NextW(s,&e));CloseHandle(s);return r;}
static void put(BYTE*p,DWORD v){memcpy(p,&v,4);}
int wmain(void){DWORD id=pid();if(!id)return 2;HANDLE p=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_CREATE_THREAD,FALSE,id);if(!p)return 3;BYTE stub[16];DWORD k=0;stub[k++]=0xB9;put(stub+k,0x008324E0);k+=4;stub[k++]=0x6A;stub[k++]=0x01;stub[k++]=0xE8;LPVOID mem=VirtualAllocEx(p,NULL,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);if(!mem){CloseHandle(p);return 4;}put(stub+k,(DWORD)(0x00633140-((DWORD)(uintptr_t)mem+k+4)));k+=4;stub[k++]=0xC3;SIZE_T n=0;BOOL ok=WriteProcessMemory(p,mem,stub,k,&n)&&n==k;HANDLE t=ok?CreateRemoteThread(p,NULL,0,(LPTHREAD_START_ROUTINE)mem,NULL,0,NULL):NULL;if(t){ok=WaitForSingleObject(t,1000)==WAIT_OBJECT_0;CloseHandle(t);}VirtualFreeEx(p,mem,0,MEM_RELEASE);CloseHandle(p);wprintf(L"RADAR_ON %ls\n",ok?L"completed":L"failed");return ok?0:5;}

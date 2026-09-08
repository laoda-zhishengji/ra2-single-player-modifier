#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>

static const wchar_t *kPath=L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static DWORD find_target(void){HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(s==INVALID_HANDLE_VALUE)return 0;PROCESSENTRY32W e={0};e.dwSize=sizeof(e);DWORD r=0;if(Process32FirstW(s,&e))do{if(!_wcsicmp(e.szExeFile,L"game.exe")){HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);if(p){wchar_t q[MAX_PATH];DWORD n=MAX_PATH;if(QueryFullProcessImageNameW(p,0,q,&n)&&!_wcsicmp(q,kPath))r=e.th32ProcessID;CloseHandle(p);}}}while(!r&&Process32NextW(s,&e));CloseHandle(s);return r;}
int wmain(int argc,wchar_t **argv){if(argc!=3){fwprintf(stderr,L"用法: ra2_money_rescan.exe <新资金值> <候选文件>\n");return 2;}long wanted=wcstol(argv[1],0,10);DWORD pid=find_target();if(!pid)return 3;HANDLE p=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,pid);if(!p)return 4;FILE *f=_wfopen(argv[2],L"rt, ccs=UNICODE");if(!f){CloseHandle(p);return 5;}wchar_t line[256];unsigned n=0;while(fgetws(line,256,f)){unsigned long a;if(swscanf(line,L"0x%lx",&a)==1){LONG v=0;SIZE_T got=0;if(ReadProcessMemory(p,(void *)(uintptr_t)a,&v,sizeof(v),&got)&&got==sizeof(v)&&v==wanted){wprintf(L"0x%08lX value=%ld\n",a,(long)v);n++;}}}fclose(f);CloseHandle(p);wprintf(L"matches=%u pid=%lu value=%ld\n",n,pid,wanted);return 0;}

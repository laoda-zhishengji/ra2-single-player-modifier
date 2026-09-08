#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>

static const wchar_t path[] = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static const BYTE original[] = {0x0F,0x84,0xC4,0x01,0x00,0x00};
static const BYTE patched[]  = {0x90,0x90,0x90,0x90,0x90,0x90};
static const BYTE original2[] = {0x0F,0x84,0xB6,0x01,0x00,0x00};
static const BYTE patched2[]  = {0x90,0x90,0x90,0x90,0x90,0x90};
static DWORD find_pid(void){HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32W e={sizeof(e)};DWORD id=0;if(s!=INVALID_HANDLE_VALUE&&Process32FirstW(s,&e))do{if(!_wcsicmp(e.szExeFile,L"game.exe")){HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);wchar_t q[MAX_PATH];DWORD n=MAX_PATH;if(h&&QueryFullProcessImageNameW(h,0,q,&n)&&!_wcsicmp(q,path))id=e.th32ProcessID;if(h)CloseHandle(h);if(id)break;}}while(Process32NextW(s,&e));if(s!=INVALID_HANDLE_VALUE)CloseHandle(s);return id;}
int wmain(int argc,wchar_t**argv){BOOL a1=argc==2&&!_wcsicmp(argv[1],L"--apply1"),r1=argc==2&&!_wcsicmp(argv[1],L"--restore1"),a2=argc==2&&!_wcsicmp(argv[1],L"--apply2"),r2=argc==2&&!_wcsicmp(argv[1],L"--restore2");if(!(a1||r1||a2||r2)){wprintf(L"Usage: --apply1|--restore1|--apply2|--restore2\n");return 2;}uintptr_t addr=(a1||r1)?0x0049BC1C:0x0049BC2A;const BYTE*originalx=(a1||r1)?original:original2;const BYTE*patchedx=(a1||r1)?patched:patched2;DWORD id=find_pid();if(!id)return 3;HANDLE p=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|PROCESS_VM_OPERATION|PROCESS_VM_WRITE,FALSE,id);if(!p)return 4;BYTE cur[6];SIZE_T n=0;if(!ReadProcessMemory(p,(void*)addr,cur,6,&n)||n!=6){CloseHandle(p);return 5;}const BYTE*want=(a1||a2)?originalx:patchedx;const BYTE*put=(a1||a2)?patchedx:originalx;if(memcmp(cur,want,6)){wprintf(L"PATCH_REFUSED unexpected bytes: %02X %02X %02X %02X %02X %02X\n",cur[0],cur[1],cur[2],cur[3],cur[4],cur[5]);CloseHandle(p);return 6;}SIZE_T w=0;BOOL ok=WriteProcessMemory(p,(void*)addr,put,6,&w)&&w==6&&FlushInstructionCache(p,(void*)addr,6);wprintf(L"DISTANCE_TEST_%ls %ls\n",(a1||r1)?(a1?L"APPLY1":L"RESTORE1"):(a2?L"APPLY2":L"RESTORE2"),ok?L"completed":L"failed");CloseHandle(p);return ok?0:7;}

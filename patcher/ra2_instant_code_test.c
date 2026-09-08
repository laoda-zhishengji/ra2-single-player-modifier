#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
static const wchar_t path[]=L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static const uintptr_t addr=0x004B9362;
static const BYTE original[5]={0x8B,0x56,0x24,0x03,0xD0};
static const BYTE patched[5]={0xBA,0x36,0x00,0x00,0x00};
static DWORD findpid(void){HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32W e={0};DWORD r=0;e.dwSize=sizeof(e);if(s!=INVALID_HANDLE_VALUE&&Process32FirstW(s,&e))do{if(!_wcsicmp(e.szExeFile,L"game.exe")){HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);wchar_t q[MAX_PATH];DWORD n=MAX_PATH;if(h&&QueryFullProcessImageNameW(h,0,q,&n)&&!_wcsicmp(q,path))r=e.th32ProcessID;if(h)CloseHandle(h);}}while(!r&&Process32NextW(s,&e));if(s!=INVALID_HANDLE_VALUE)CloseHandle(s);return r;}
int wmain(int argc,wchar_t**argv){BOOL apply=argc==2&&!_wcsicmp(argv[1],L"--apply"),restore=argc==2&&!_wcsicmp(argv[1],L"--restore");if(argc>1&&!apply&&!restore){wprintf(L"Usage: ra2_instant_code_test.exe [--apply|--restore]\n");return 2;}DWORD id=findpid();if(!id)return 3;HANDLE p=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|PROCESS_VM_OPERATION|PROCESS_VM_WRITE,FALSE,id);if(!p)return 4;BYTE cur[5];SIZE_T n=0;if(!ReadProcessMemory(p,(void*)addr,cur,5,&n)||n!=5){CloseHandle(p);return 5;}wprintf(L"module+0xB9362: %02X %02X %02X %02X %02X\n",cur[0],cur[1],cur[2],cur[3],cur[4]);if(!apply&&!restore){wprintf(L"PATCH_STATE=%ls\nNo memory was written.\n",memcmp(cur,patched,5)==0?L"APPLIED":L"UNAPPLIED");CloseHandle(p);return 0;}const BYTE*want=apply?original:patched;const BYTE*put=apply?patched:original;if(memcmp(cur,want,5)!=0){wprintf(L"PATCH_REFUSED: unexpected bytes.\n");CloseHandle(p);return 6;}SIZE_T w=0;if(!WriteProcessMemory(p,(void*)addr,put,5,&w)||w!=5||!FlushInstructionCache(p,(void*)addr,5)){CloseHandle(p);return 7;}wprintf(L"PATCH_STATE=%ls\n",apply?L"APPLIED":L"UNAPPLIED");CloseHandle(p);return 0;}

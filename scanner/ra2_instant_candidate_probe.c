#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>
static const wchar_t kPath[]=L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static DWORD findpid(void){HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32W e={0};DWORD r=0;e.dwSize=sizeof(e);if(s!=INVALID_HANDLE_VALUE&&Process32FirstW(s,&e))do{if(!_wcsicmp(e.szExeFile,L"game.exe")){HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);wchar_t q[MAX_PATH];DWORD n=MAX_PATH;if(h&&QueryFullProcessImageNameW(h,0,q,&n)&&!_wcsicmp(q,kPath))r=e.th32ProcessID;if(h)CloseHandle(h);}}while(!r&&Process32NextW(s,&e));if(s!=INVALID_HANDLE_VALUE)CloseHandle(s);return r;}
int wmain(void){DWORD id=findpid();if(!id)return 2;HANDLE p=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,id);if(!p)return 3;DWORD root=0,obj=0,cd=0;SIZE_T n=0;if(!ReadProcessMemory(p,(void*)0x00A35DB4,&root,4,&n)||n!=4)return 4;const DWORD po[]={0x52D8,0x52DC,0x52E0,0x52E4,0x52E8,0x52F8};const wchar_t*name[]={L"aircraft",L"infantry",L"vehicle",L"ship",L"building",L"superweapon"};wprintf(L"object=0x%08lX\\n",(unsigned long)root);for(int i=0;i<6;i++){obj=0;cd=0;ReadProcessMemory(p,(void*)(uintptr_t)(root+po[i]),&obj,4,&n);if(obj)ReadProcessMemory(p,(void*)(uintptr_t)(obj+0x24),&cd,4,&n);wprintf(L"%ls ptr[+0x%lX]=0x%08lX cd= %lu\\n",name[i],(unsigned long)po[i],(unsigned long)obj,(unsigned long)cd);}CloseHandle(p);return 0;}

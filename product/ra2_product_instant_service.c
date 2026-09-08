#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdint.h>
#include <stdio.h>
#include "ra2_product_manifest.h"

static const wchar_t kStopEvent[] = L"Local\\RA2ProductInstantProductionStop";
static const DWORD kQueueOffsets[] = { 0x52D8, 0x52DC, 0x52E0, 0x52E4, 0x52E8, 0x52F8 };
typedef struct { DWORD ptr, cd; BOOL valid; } QueueSnapshot;
static volatile BOOL g_stop = FALSE;
static BOOL WINAPI handler(DWORD t){if(t==CTRL_C_EVENT||t==CTRL_BREAK_EVENT||t==CTRL_CLOSE_EVENT){g_stop=TRUE;return TRUE;}return FALSE;}
static DWORD find_pid(void){HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32W e={sizeof(e)};DWORD id=0;if(s==INVALID_HANDLE_VALUE)return 0;if(Process32FirstW(s,&e))do{if(!_wcsicmp(e.szExeFile,L"game.exe")){HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);wchar_t q[MAX_PATH];DWORD n=MAX_PATH;if(p&&QueryFullProcessImageNameW(p,0,q,&n)&&!_wcsicmp(q,RA2_PRODUCT_GAME_PATH))id=e.th32ProcessID;if(p)CloseHandle(p);}}while(!id&&Process32NextW(s,&e));CloseHandle(s);return id;}
static BOOL read4(HANDLE p,DWORD a,DWORD*v){SIZE_T n=0;return ReadProcessMemory(p,(void*)(uintptr_t)a,v,4,&n)&&n==4;}
static BOOL write4(HANDLE p,DWORD a,DWORD v){SIZE_T n=0;return WriteProcessMemory(p,(void*)(uintptr_t)a,&v,4,&n)&&n==4;}

int wmain(int argc,wchar_t**argv){if(argc!=1){wprintf(L"Usage: ra2_product_instant_service.exe\n");return 2;}SetConsoleCtrlHandler(handler,TRUE);HANDLE stop=CreateEventW(NULL,TRUE,FALSE,kStopEvent);if(!stop)return 3;QueueSnapshot saved[6]={{0}};DWORD savedRoot=0,pid=0;HANDLE p=NULL;int missing=0;wprintf(L"INSTANT_PRODUCTION_SERVICE running\n");while(!g_stop&&WaitForSingleObject(stop,50)==WAIT_TIMEOUT){DWORD nowPid=find_pid();if(!nowPid){if(++missing>=20)break;continue;}missing=0;if(nowPid!=pid){if(p)CloseHandle(p);p=NULL;pid=nowPid;savedRoot=0;memset(saved,0,sizeof(saved));}if(!p)p=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|PROCESS_VM_OPERATION|PROCESS_VM_WRITE,FALSE,pid);if(!p)continue;DWORD root=0;if(!read4(p,RA2_PRODUCT_PLAYER_ROOT,&root)||!root)continue;if(root!=savedRoot){savedRoot=root;memset(saved,0,sizeof(saved));}for(int i=0;i<6;i++){DWORD q=0,cd=0;if(!read4(p,root+kQueueOffsets[i],&q)||!q||!read4(p,q+0x24,&cd))continue;if(!saved[i].valid||saved[i].ptr!=q){saved[i].ptr=q;saved[i].cd=cd;saved[i].valid=TRUE;}if(cd<53)write4(p,q+0x24,53);}}if(p&&savedRoot){DWORD root=0;if(read4(p,RA2_PRODUCT_PLAYER_ROOT,&root)&&root==savedRoot){for(int i=0;i<6;i++)if(saved[i].valid){DWORD cd=0;if(read4(p,saved[i].ptr+0x24,&cd)&&cd==53)write4(p,saved[i].ptr+0x24,saved[i].cd);}}CloseHandle(p);}CloseHandle(stop);wprintf(L"INSTANT_PRODUCTION_SERVICE stopped\n");return 0;}

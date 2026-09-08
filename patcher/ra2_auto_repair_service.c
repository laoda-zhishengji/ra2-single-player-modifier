#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "../product/ra2_product_manifest.h"

#pragma comment(lib, "bcrypt.lib")

static const wchar_t kStopEvent[] = L"Local\\RA2ProductAutoRepairStop";
static const uintptr_t kKnownRepairSell = (uintptr_t)0x1DCC2104;
static volatile BOOL g_stop = FALSE;
static BOOL WINAPI handler(DWORD type) { if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) { g_stop = TRUE; return TRUE; } return FALSE; }

static DWORD find_pid(void) { HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); PROCESSENTRY32W e={sizeof(e)}; DWORD id=0; if(s==INVALID_HANDLE_VALUE)return 0; if(Process32FirstW(s,&e))do{if(!_wcsicmp(e.szExeFile,L"game.exe")){HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);wchar_t q[MAX_PATH];DWORD n=MAX_PATH;if(p&&QueryFullProcessImageNameW(p,0,q,&n)&&!_wcsicmp(q,RA2_PRODUCT_GAME_PATH))id=e.th32ProcessID;if(p)CloseHandle(p);}}while(!id&&Process32NextW(s,&e));CloseHandle(s);return id; }
static BOOL readable(DWORD protect) { DWORD p=protect&0xff; return p==PAGE_READONLY||p==PAGE_READWRITE||p==PAGE_WRITECOPY||p==PAGE_EXECUTE_READ||p==PAGE_EXECUTE_READWRITE||p==PAGE_EXECUTE_WRITECOPY; }
static BOOL hash_ok(void) { HANDLE f=CreateFileW(RA2_PRODUCT_GAME_PATH,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);BCRYPT_ALG_HANDLE a=NULL;BCRYPT_HASH_HANDLE h=NULL;PUCHAR obj=NULL,dig=NULL;DWORD ol=0,dl=0,cb=0;BYTE b[8192];ULONG got=0;wchar_t out[65];BOOL ok=FALSE;if(f==INVALID_HANDLE_VALUE||BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,NULL,0)<0)goto done;if(BCryptGetProperty(a,BCRYPT_OBJECT_LENGTH,(PUCHAR)&ol,4,&cb,0)<0||BCryptGetProperty(a,BCRYPT_HASH_LENGTH,(PUCHAR)&dl,4,&cb,0)<0)goto done;obj=HeapAlloc(GetProcessHeap(),0,ol);dig=HeapAlloc(GetProcessHeap(),0,dl);if(!obj||!dig||BCryptCreateHash(a,&h,obj,ol,NULL,0,0)<0)goto done;for(;;){if(!ReadFile(f,b,sizeof(b),&got,NULL))goto done;if(!got)break;if(BCryptHashData(h,b,got,0)<0)goto done;}if(BCryptFinishHash(h,dig,dl,0)<0)goto done;for(DWORD i=0;i<dl;i++)swprintf_s(out+i*2,65-i*2,L"%02X",dig[i]);out[64]=0;ok=!_wcsicmp(out,RA2_PRODUCT_SHA256);done:if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);if(obj)HeapFree(GetProcessHeap(),0,obj);if(dig)HeapFree(GetProcessHeap(),0,dig);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);return ok; }

static uintptr_t find_repair_sell(HANDLE p, int *old) {
    MEMORY_BASIC_INFORMATION m; uintptr_t cur=0; BYTE buf[65536];
    *old=-1;
    while (VirtualQueryEx(p,(LPCVOID)cur,&m,sizeof(m))==sizeof(m)) {
        if (m.State==MEM_COMMIT&&readable(m.Protect)&&!(m.Protect&PAGE_GUARD)) {
            SIZE_T off=0;
            while (off<m.RegionSize) {
                SIZE_T want=m.RegionSize-off;if(want>sizeof(buf))want=sizeof(buf);SIZE_T got=0;
                if(ReadProcessMemory(p,(BYTE*)m.BaseAddress+off,buf,want,&got)&&got>=44) {
                    for(SIZE_T i=0;i+44<=got;i+=4) {
                        int v[11];for(int j=0;j<11;j++)memcpy(&v[j],buf+i+j*4,4);
                        if(v[0]==5&&v[1]==4&&v[2]==5&&v[3]==2&&v[4]>=0&&v[4]<=5&&v[5]==2&&v[6]==2&&v[7]==3&&v[8]==3&&v[9]==2&&v[10]==2){*old=v[4];return (uintptr_t)m.BaseAddress+off+i+16;}
                    }
                }
                if(want<44)break;off+=want-43;
            }
        }
        uintptr_t next=(uintptr_t)m.BaseAddress+m.RegionSize;if(next<=cur)break;cur=next;
    }
    return 0;
}

int wmain(int argc,wchar_t **argv) {
    if(argc!=1){wprintf(L"Usage: ra2_product_auto_repair.exe\n");return 2;}
    if(!hash_ok()){wprintf(L"VERSION_MISMATCH: service stopped.\n");return 2;}
    SetConsoleCtrlHandler(handler,TRUE);
    HANDLE stop=CreateEventW(NULL,TRUE,FALSE,kStopEvent);if(!stop)return 3;
    DWORD pid=0;uintptr_t addr=0;int original=1;BOOL wrote=FALSE;HANDLE p=NULL;
    wprintf(L"AUTO_REPAIR_IQ_SERVICE running\n");
    while(!g_stop&&WaitForSingleObject(stop,100)==WAIT_TIMEOUT){
        DWORD nowpid=find_pid();
        if(!nowpid){addr=0;if(p){CloseHandle(p);p=NULL;}Sleep(100);continue;}
        if(nowpid!=pid){if(p)CloseHandle(p);p=NULL;pid=nowpid;addr=0;wrote=FALSE;original=1;}
        if(!p)p=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|PROCESS_VM_OPERATION|PROCESS_VM_WRITE,FALSE,pid);
        if(!p)continue;
        if(!addr){
            int known=-1; SIZE_T n=0;
            if(ReadProcessMemory(p,(void*)kKnownRepairSell,&known,4,&n)&&n==4&&known>=0&&known<=5){addr=kKnownRepairSell;original=known;}
        }
        static DWORD scan_tick=0; DWORD tick=GetTickCount();
        if(!addr || tick-scan_tick>=300){
            scan_tick=tick;
            int found_old=-1;uintptr_t found=find_repair_sell(p,&found_old);
            if(found){
                if(!wrote||found!=addr)original=found_old;
                addr=found;
                int zero=0;SIZE_T w=0;
                if(WriteProcessMemory(p,(void*)addr,&zero,4,&w)&&w==4){wrote=TRUE;wprintf(L"AUTO_REPAIR_IQ_APPLIED addr=0x%08lX old=%d new=0\n",(unsigned long)addr,original);}
            }
        }
        if(addr){
            int current=-1; SIZE_T n=0;
            if(ReadProcessMemory(p,(void*)addr,&current,4,&n)&&n==4&&current!=0){
                int zero=0; SIZE_T w=0;
                if(WriteProcessMemory(p,(void*)addr,&zero,4,&w)&&w==4) wrote=TRUE;
            }
        }
    }
    if(p&&wrote&&addr){int current=-1;SIZE_T n=0;if(ReadProcessMemory(p,(void*)addr,&current,4,&n)&&n==4&&current==0){SIZE_T w=0;WriteProcessMemory(p,(void*)addr,&original,4,&w);}}
    if(p)CloseHandle(p);CloseHandle(stop);wprintf(L"AUTO_REPAIR_IQ_SERVICE stopped\n");return 0;
}

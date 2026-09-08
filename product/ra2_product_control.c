#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include "ra2_product_module.h"
#include "ra2_product_manifest.h"

#pragma comment(lib, "bcrypt.lib")


typedef struct { uintptr_t address; BYTE original[2]; BYTE patched[2]; Ra2ModuleId id; } Patch;
static const Patch kPatches[] = {
    {RA2_PRODUCT_MONEY_PATCH, {0x2B,0xC7}, {0x90,0x90}, RA2_MODULE_MONEY},
    {RA2_PRODUCT_POWER_PATCH, {0x03,0xC8}, {0x90,0x90}, RA2_MODULE_POWER}
};

static DWORD find_pid(void) {
    HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); PROCESSENTRY32W e={sizeof(e)}; DWORD id=0;
    if(s==INVALID_HANDLE_VALUE)return 0;
    if(Process32FirstW(s,&e)) do { if(!_wcsicmp(e.szExeFile,L"game.exe")) { HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID); wchar_t q[MAX_PATH]; DWORD n=MAX_PATH; if(p&&QueryFullProcessImageNameW(p,0,q,&n)&&!_wcsicmp(q,RA2_PRODUCT_GAME_PATH))id=e.th32ProcessID; if(p)CloseHandle(p); } } while(!id&&Process32NextW(s,&e));
    CloseHandle(s); return id;
}

static BOOL hash_file(wchar_t *out) {
    HANDLE f=CreateFileW(RA2_PRODUCT_GAME_PATH,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL); BCRYPT_ALG_HANDLE a=NULL; BCRYPT_HASH_HANDLE h=NULL; PUCHAR obj=NULL,dig=NULL; DWORD ol=0,dl=0,cb=0; BYTE buf[8192]; ULONG got; BOOL ok=FALSE;
    if(f==INVALID_HANDLE_VALUE||BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,NULL,0)<0)goto done;
    if(BCryptGetProperty(a,BCRYPT_OBJECT_LENGTH,(PUCHAR)&ol,4,&cb,0)<0||BCryptGetProperty(a,BCRYPT_HASH_LENGTH,(PUCHAR)&dl,4,&cb,0)<0)goto done;
    obj=HeapAlloc(GetProcessHeap(),0,ol); dig=HeapAlloc(GetProcessHeap(),0,dl); if(!obj||!dig||BCryptCreateHash(a,&h,obj,ol,NULL,0,0)<0)goto done;
    for(;;){if(!ReadFile(f,buf,sizeof(buf),&got,NULL))goto done;if(!got)break;if(BCryptHashData(h,buf,got,0)<0)goto done;} if(BCryptFinishHash(h,dig,dl,0)<0)goto done;
    for(DWORD i=0;i<dl;i++)swprintf_s(out+i*2,65-i*2,L"%02X",dig[i]); out[64]=0; ok=TRUE;
done: if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);if(obj)HeapFree(GetProcessHeap(),0,obj);if(dig)HeapFree(GetProcessHeap(),0,dig);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);return ok;
}

static BOOL read2(HANDLE p, uintptr_t a, BYTE b[2]) { SIZE_T n=0; return ReadProcessMemory(p,(void*)a,b,2,&n)&&n==2; }
static const wchar_t *state(const BYTE b[2],const Patch *x){if(!memcmp(b,x->original,2))return L"OFF";if(!memcmp(b,x->patched,2))return L"ON";return L"UNKNOWN";}

int wmain(int argc,wchar_t **argv) {
    BOOL status=argc==1||!_wcsicmp(argv[1],L"--status"); int target=-1; BOOL enable=FALSE,disable=FALSE;
    if (argc > 2) { wprintf(L"Usage: ra2_product_control.exe [--status|--money-on|--money-off|--power-on|--power-off]\n"); return 2; }
    if(argc==2){if(!_wcsicmp(argv[1],L"--money-on")){target=0;enable=TRUE;}else if(!_wcsicmp(argv[1],L"--money-off")){target=0;disable=TRUE;}else if(!_wcsicmp(argv[1],L"--power-on")){target=1;enable=TRUE;}else if(!_wcsicmp(argv[1],L"--power-off")){target=1;disable=TRUE;}else if(!status){wprintf(L"Usage: ra2_product_control.exe [--status|--money-on|--money-off|--power-on|--power-off]\n");return 2;}}
    DWORD pid=find_pid(); wchar_t sha[65]; if(!pid||!hash_file(sha)){wprintf(L"TARGET_UNAVAILABLE\n");return 3;} if(_wcsicmp(sha,RA2_PRODUCT_SHA256)){wprintf(L"VERSION_MISMATCH\nWRITES=REFUSED\n");return 4;}
    HANDLE p=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|((target>=0)?(PROCESS_VM_OPERATION|PROCESS_VM_WRITE):0),FALSE,pid); if(!p)return 5;
    Ra2ModuleRegistry registry; ra2_registry_init(&registry);
    for(int i=0;i<2;i++){BYTE b[2];if(!read2(p,kPatches[i].address,b)){CloseHandle(p);return 6;}wprintf(L"%ls=%ls bytes=%02X%02X\n",ra2_module_name(kPatches[i].id),state(b,&kPatches[i]),b[0],b[1]);if(target==i){const Patch *x=&kPatches[i];const BYTE *want=enable?x->original:x->patched;const BYTE *next=enable?x->patched:x->original;if(memcmp(b,want,2)){wprintf(L"REFUSED: unexpected current bytes\n");CloseHandle(p);return 7;}if(enable){if(ra2_begin_apply(&registry,x->id)!=RA2_RESULT_OK||ra2_commit_apply(&registry,x->id,x->original,2)!=RA2_RESULT_OK){CloseHandle(p);return 8;}}else{if(ra2_begin_apply(&registry,x->id)!=RA2_RESULT_OK||ra2_commit_apply(&registry,x->id,x->original,2)!=RA2_RESULT_OK||ra2_begin_restore(&registry,x->id)!=RA2_RESULT_OK){CloseHandle(p);return 8;}}SIZE_T n=0;if(!WriteProcessMemory(p,(void*)x->address,next,2,&n)||n!=2||!FlushInstructionCache(p,(void*)x->address,2)){CloseHandle(p);return 9;}if(!enable&&ra2_commit_restore(&registry,x->id,x->original,2)!=RA2_RESULT_OK){CloseHandle(p);return 10;}wprintf(L"%ls=%ls\n",ra2_module_name(x->id),enable?L"ON":L"OFF");}}
    wprintf(L"VERSION=MATCHED PID=%lu WRITES=%ls\n",pid,target>=0?L"ONE":L"NONE"); CloseHandle(p); return 0;
}

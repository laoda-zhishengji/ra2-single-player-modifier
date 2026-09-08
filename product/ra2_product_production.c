#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include "ra2_product_manifest.h"

#pragma comment(lib, "bcrypt.lib")

static const wchar_t kSnapshot[] = L"product-state\\production.snapshot";
static const DWORD kOffsets[] = {0x52B8,0x52BC,0x52C0,0x52C4,0x52C8};
static const DWORD kApplyValue = RA2_PRODUCT_PRODUCTION_APPLY_VALUE;

typedef struct { DWORD magic, version, pid, root, values[5]; wchar_t sha[65]; } Snapshot;

static DWORD find_pid(void) {
    HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); PROCESSENTRY32W e={sizeof(e)}; DWORD id=0;
    if(s==INVALID_HANDLE_VALUE)return 0;
    if(Process32FirstW(s,&e)) do { if(!_wcsicmp(e.szExeFile,L"game.exe")){HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);wchar_t q[MAX_PATH];DWORD n=MAX_PATH;if(p&&QueryFullProcessImageNameW(p,0,q,&n)&&!_wcsicmp(q,RA2_PRODUCT_GAME_PATH))id=e.th32ProcessID;if(p)CloseHandle(p);}} while(!id&&Process32NextW(s,&e));
    CloseHandle(s);return id;
}

static BOOL hash_file(wchar_t *out) {
    HANDLE f=CreateFileW(RA2_PRODUCT_GAME_PATH,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL); BCRYPT_ALG_HANDLE a=NULL;BCRYPT_HASH_HANDLE h=NULL;PUCHAR obj=NULL,dig=NULL;DWORD ol=0,dl=0,cb=0;BYTE buf[8192];ULONG got;BOOL ok=FALSE;
    if(f==INVALID_HANDLE_VALUE||BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,NULL,0)<0)goto done;if(BCryptGetProperty(a,BCRYPT_OBJECT_LENGTH,(PUCHAR)&ol,4,&cb,0)<0||BCryptGetProperty(a,BCRYPT_HASH_LENGTH,(PUCHAR)&dl,4,&cb,0)<0)goto done;obj=HeapAlloc(GetProcessHeap(),0,ol);dig=HeapAlloc(GetProcessHeap(),0,dl);if(!obj||!dig||BCryptCreateHash(a,&h,obj,ol,NULL,0,0)<0)goto done;for(;;){if(!ReadFile(f,buf,sizeof(buf),&got,NULL))goto done;if(!got)break;if(BCryptHashData(h,buf,got,0)<0)goto done;}if(BCryptFinishHash(h,dig,dl,0)<0)goto done;for(DWORD i=0;i<dl;i++)swprintf_s(out+i*2,65-i*2,L"%02X",dig[i]);out[64]=0;ok=TRUE;
done:if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);if(obj)HeapFree(GetProcessHeap(),0,obj);if(dig)HeapFree(GetProcessHeap(),0,dig);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);return ok;
}

static BOOL read_values(HANDLE p,DWORD root,DWORD values[5]) { SIZE_T n=0;for(int i=0;i<5;i++)if(!ReadProcessMemory(p,(void*)(uintptr_t)(root+kOffsets[i]),&values[i],4,&n)||n!=4)return FALSE;return TRUE; }
static BOOL write_values(HANDLE p,DWORD root,const DWORD values[5]) { SIZE_T n=0;for(int i=0;i<5;i++)if(!WriteProcessMemory(p,(void*)(uintptr_t)(root+kOffsets[i]),(void*)&values[i],4,&n)||n!=4)return FALSE;FlushInstructionCache(p,(void*)(uintptr_t)root,0x100);return TRUE; }
static BOOL save_snapshot(const Snapshot *s) { CreateDirectoryW(L"product-state",NULL);HANDLE f=CreateFileW(kSnapshot,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);DWORD n=0;BOOL ok=f!=INVALID_HANDLE_VALUE&&WriteFile(f,s,sizeof(*s),&n,NULL)&&n==sizeof(*s);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);return ok; }
static BOOL load_snapshot(Snapshot *s) { HANDLE f=CreateFileW(kSnapshot,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);DWORD n=0;BOOL ok=f!=INVALID_HANDLE_VALUE&&ReadFile(f,s,sizeof(*s),&n,NULL)&&n==sizeof(*s)&&s->magic==0x52325350&&s->version==1&&!_wcsicmp(s->sha,RA2_PRODUCT_SHA256);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);return ok; }
static BOOL instant_service_present(void) { HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); PROCESSENTRY32W e={sizeof(e)}; BOOL found=FALSE; if(s==INVALID_HANDLE_VALUE)return FALSE; if(Process32FirstW(s,&e)) do { if(!_wcsicmp(e.szExeFile,L"ra2_product_instant_service.exe")){found=TRUE;break;} } while(Process32NextW(s,&e)); CloseHandle(s); return found; }

int wmain(int argc,wchar_t **argv) {
    BOOL status=argc==1||!_wcsicmp(argv[1],L"--status"),capture=argc==2&&!_wcsicmp(argv[1],L"--capture"),apply=argc==2&&!_wcsicmp(argv[1],L"--apply"),restore=argc==2&&!_wcsicmp(argv[1],L"--restore");
    if(argc>2||(!status&&!capture&&!apply&&!restore)){wprintf(L"Usage: ra2_product_production.exe [--status|--capture|--apply|--restore]\n");return 2;}
    DWORD pid=find_pid();wchar_t sha[65];if(!pid||!hash_file(sha)){wprintf(L"TARGET_UNAVAILABLE\n");return 3;}if(_wcsicmp(sha,RA2_PRODUCT_SHA256)){wprintf(L"VERSION_MISMATCH\nWRITES=REFUSED\n");return 4;}
    HANDLE p=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|((apply||restore)?PROCESS_VM_OPERATION|PROCESS_VM_WRITE:0),FALSE,pid);if(!p)return 5;DWORD root=0,values[5];SIZE_T n=0;if(!ReadProcessMemory(p,(void*)RA2_PRODUCT_PLAYER_ROOT,&root,4,&n)||n!=4||!root||!read_values(p,root,values)){CloseHandle(p);return 6;}
    wprintf(L"VERSION=MATCHED PID=%lu ROOT=0x%08lX VALUES=%lu,%lu,%lu,%lu,%lu\n",pid,root,values[0],values[1],values[2],values[3],values[4]);
    if(status){wprintf(L"SNAPSHOT=%ls INSTANT_PRODUCTION_SERVICE=%ls WRITES=NONE\n",GetFileAttributesW(kSnapshot)==INVALID_FILE_ATTRIBUTES?L"ABSENT":L"PRESENT",instant_service_present()?L"ON":L"OFF");CloseHandle(p);return 0;}
    if(capture){Snapshot s={0};s.magic=0x52325350;s.version=1;s.pid=pid;s.root=root;memcpy(s.values,values,sizeof(values));wcsncpy_s(s.sha,65,RA2_PRODUCT_SHA256,_TRUNCATE);if(!save_snapshot(&s)){CloseHandle(p);return 7;}wprintf(L"SNAPSHOT_CAPTURED pid=%lu root=0x%08lX WRITES=NONE\n",pid,root);CloseHandle(p);return 0;}
    Snapshot s;if(!load_snapshot(&s)){wprintf(L"SNAPSHOT_INVALID\n");CloseHandle(p);return 8;}if(s.pid!=pid||s.root!=root){wprintf(L"REFUSED: snapshot belongs to another task state\n");CloseHandle(p);return 14;}
    if(apply){if(memcmp(values,s.values,sizeof(values))!=0){wprintf(L"REFUSED: current fields differ from snapshot\n");CloseHandle(p);return 9;}DWORD next[5]={kApplyValue,kApplyValue,kApplyValue,kApplyValue,kApplyValue};if(!write_values(p,root,next)){CloseHandle(p);return 10;}wprintf(L"PRODUCTION=ON\n");}
    else {DWORD expected[5]={kApplyValue,kApplyValue,kApplyValue,kApplyValue,kApplyValue};if(memcmp(values,expected,sizeof(values))!=0){wprintf(L"REFUSED: current fields are not the applied values\n");CloseHandle(p);return 11;}if(!write_values(p,root,s.values)){CloseHandle(p);return 12;}if(!DeleteFileW(kSnapshot)){wprintf(L"RESTORED_BUT_SNAPSHOT_CLEANUP_FAILED\n");CloseHandle(p);return 13;}wprintf(L"PRODUCTION=OFF RESTORED SNAPSHOT=CLEARED\n");}
    CloseHandle(p);return 0;
}

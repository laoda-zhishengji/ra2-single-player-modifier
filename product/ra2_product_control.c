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
static const Patch kMoneyPatch = {RA2_PRODUCT_MONEY_PATCH, {0x2B,0xC7}, {0x90,0x90}, RA2_MODULE_MONEY};

/* The two-byte add is shared by every house.  Replace the whole instruction
 * plus the beginning of the following store with a guarded trampoline so AI
 * houses keep their normal load calculation. */
static const BYTE kPowerOriginal[5] = {0x03,0xC8,0x89,0x8E,0xD4};
static const uintptr_t kPowerHook = RA2_PRODUCT_POWER_PATCH;
static const uintptr_t kPowerReturn = RA2_PRODUCT_POWER_PATCH + 8;

static void put32(BYTE *p, DWORD v) { memcpy(p, &v, sizeof(v)); }

static BOOL read_bytes(HANDLE p, uintptr_t a, BYTE *b, SIZE_T n) {
    SIZE_T got = 0;
    return ReadProcessMemory(p, (void *)a, b, n, &got) && got == n;
}

static BOOL power_is_on(HANDLE p) {
    BYTE b[5];
    return read_bytes(p, kPowerHook, b, sizeof(b)) && b[0] == 0xE9;
}

static int power_apply(HANDLE p) {
    BYTE current[5], patch[5] = {0xE9,0,0,0,0};
    if (!read_bytes(p, kPowerHook, current, sizeof(current))) return 6;
    if (!memcmp(current, kPowerOriginal, sizeof(current))) {
        BYTE stub[64]; DWORD k = 0; SIZE_T n = 0;
        LPVOID cave = VirtualAllocEx(p, NULL, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!cave) return 8;

        /* cmp esi,[player-root]; je skip add; otherwise execute add ecx,eax.
         * Both paths then execute the original store and return after it. */
        stub[k++] = 0x3B; stub[k++] = 0x35; put32(stub + k, RA2_PRODUCT_PLAYER_ROOT); k += 4;
        stub[k++] = 0x74; stub[k++] = 0x02;
        stub[k++] = 0x03; stub[k++] = 0xC8;
        stub[k++] = 0x89; stub[k++] = 0x8E; put32(stub + k, 0x52D4); k += 4;
        stub[k++] = 0xE9; put32(stub + k, (DWORD)(kPowerReturn - ((uintptr_t)cave + k + 4))); k += 4;

        put32(patch + 1, (DWORD)((uintptr_t)cave - (kPowerHook + sizeof(patch))));
        if (!WriteProcessMemory(p, cave, stub, k, &n) || n != k ||
            !WriteProcessMemory(p, (void *)kPowerHook, patch, sizeof(patch), &n) || n != sizeof(patch) ||
            !FlushInstructionCache(p, (void *)kPowerHook, sizeof(patch))) {
            VirtualFreeEx(p, cave, 0, MEM_RELEASE);
            return 9;
        }
        return 0;
    }
    return power_is_on(p) ? 0 : 7;
}

static int power_restore(HANDLE p) {
    BYTE current[5]; SIZE_T n = 0;
    if (!read_bytes(p, kPowerHook, current, sizeof(current))) return 6;
    if (!memcmp(current, kPowerOriginal, sizeof(current))) return 0;
    if (current[0] != 0xE9) return 7;
    if (!WriteProcessMemory(p, (void *)kPowerHook, kPowerOriginal, sizeof(kPowerOriginal), &n) ||
        n != sizeof(kPowerOriginal) || !FlushInstructionCache(p, (void *)kPowerHook, sizeof(kPowerOriginal))) return 9;
    return 0;
}

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
    BYTE money[2]; BYTE power[5];
    if (!read2(p, kMoneyPatch.address, money) || !read_bytes(p, kPowerHook, power, sizeof(power))) { CloseHandle(p); return 6; }
    wprintf(L"money=%ls bytes=%02X%02X\n", state(money, &kMoneyPatch), money[0], money[1]);
    wprintf(L"power=%ls bytes=%02X%02X\n", !memcmp(power, kPowerOriginal, sizeof(power)) ? L"OFF" : power[0] == 0xE9 ? L"ON" : L"UNKNOWN", power[0], power[1]);

    if (target == 0) {
        const BYTE *want = enable ? kMoneyPatch.original : kMoneyPatch.patched;
        const BYTE *next = enable ? kMoneyPatch.patched : kMoneyPatch.original;
        if (memcmp(money, want, 2)) { wprintf(L"REFUSED: unexpected money bytes\n"); CloseHandle(p); return 7; }
        if (enable) {
            if (ra2_begin_apply(&registry, kMoneyPatch.id) != RA2_RESULT_OK || ra2_commit_apply(&registry, kMoneyPatch.id, kMoneyPatch.original, 2) != RA2_RESULT_OK) { CloseHandle(p); return 8; }
        } else {
            if (ra2_begin_apply(&registry, kMoneyPatch.id) != RA2_RESULT_OK || ra2_commit_apply(&registry, kMoneyPatch.id, kMoneyPatch.original, 2) != RA2_RESULT_OK || ra2_begin_restore(&registry, kMoneyPatch.id) != RA2_RESULT_OK) { CloseHandle(p); return 8; }
        }
        SIZE_T n = 0;
        if (!WriteProcessMemory(p, (void *)kMoneyPatch.address, next, 2, &n) || n != 2 || !FlushInstructionCache(p, (void *)kMoneyPatch.address, 2)) { CloseHandle(p); return 9; }
        if (!enable && ra2_commit_restore(&registry, kMoneyPatch.id, kMoneyPatch.original, 2) != RA2_RESULT_OK) { CloseHandle(p); return 10; }
        wprintf(L"money=%ls\n", enable ? L"ON" : L"OFF");
    } else if (target == 1) {
        int rc = enable ? power_apply(p) : power_restore(p);
        if (rc) { wprintf(L"REFUSED: power guarded patch failed code=%d\n", rc); CloseHandle(p); return rc; }
        wprintf(L"power=%ls\n", enable ? L"ON_PLAYER_ONLY" : L"OFF");
    }
    wprintf(L"VERSION=MATCHED PID=%lu WRITES=%ls\n",pid,target>=0?L"ONE":L"NONE"); CloseHandle(p); return 0;
}

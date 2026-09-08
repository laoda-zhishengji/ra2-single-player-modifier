#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static const wchar_t path[] = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static DWORD find_pid(void) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); PROCESSENTRY32W e = { sizeof(e) }; DWORD id = 0;
    if (s != INVALID_HANDLE_VALUE && Process32FirstW(s, &e)) do {
        if (!_wcsicmp(e.szExeFile, L"game.exe")) { HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID); wchar_t q[MAX_PATH]; DWORD n=MAX_PATH;
            if (h && QueryFullProcessImageNameW(h, 0, q, &n) && !_wcsicmp(q, path)) id=e.th32ProcessID; if(h)CloseHandle(h); if(id)break; }
    } while (Process32NextW(s, &e)); if(s!=INVALID_HANDLE_VALUE)CloseHandle(s); return id;
}
static void put32(BYTE *p, DWORD v) { memcpy(p, &v, 4); }
int wmain(void) {
    DWORD id=find_pid(); if(!id)return 2;
    HANDLE p=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_CREATE_THREAD,FALSE,id); if(!p)return 3;
    DWORD obj=0, player=0; SIZE_T n=0;
    BOOL ok=ReadProcessMemory(p,(LPCVOID)0x00A3D290,&obj,4,&n)&&n==4&&obj;
    if(ok)ok=WriteProcessMemory(p,(LPVOID)(uintptr_t)(obj+0x2228),(BYTE[]){1},1,&n)&&n==1;
    if(ok)ok=ReadProcessMemory(p,(LPCVOID)0x00A35DB4,&player,4,&n)&&n==4&&player;
    BYTE stub[16]; DWORD k=0; stub[k++]=0xB9;put32(stub+k,player);k+=4;stub[k++]=0xE8;
    LPVOID mem=VirtualAllocEx(p,NULL,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    if(!mem){CloseHandle(p);return 4;} put32(stub+k,0x004F2E50-((DWORD)(uintptr_t)mem+k+4));k+=4;stub[k++]=0xC3;
    if(ok)ok=WriteProcessMemory(p,mem,stub,k,&n)&&n==k; HANDLE t=ok?CreateRemoteThread(p,NULL,0,(LPTHREAD_START_ROUTINE)mem,NULL,0,NULL):NULL;
    if(t){ok=WaitForSingleObject(t,1000)==WAIT_OBJECT_0;CloseHandle(t);} VirtualFreeEx(p,mem,0,MEM_RELEASE);CloseHandle(p);
    wprintf(L"RADAR_RECOMPUTE_ON %ls\n",ok?L"completed":L"failed");return ok?0:5;
}

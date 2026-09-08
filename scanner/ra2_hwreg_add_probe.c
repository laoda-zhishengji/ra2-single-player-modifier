#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

int wmain(void) {
    DWORD pid=0; HANDLE ps=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); PROCESSENTRY32W pe={sizeof(pe)};
    if(ps!=INVALID_HANDLE_VALUE && Process32FirstW(ps,&pe)) do { if(!_wcsicmp(pe.szExeFile,L"game.exe")){pid=pe.th32ProcessID;break;} } while(Process32NextW(ps,&pe));
    if(ps!=INVALID_HANDLE_VALUE) CloseHandle(ps); if(!pid){wprintf(L"GAME_NOT_FOUND\n");return 2;}
    HANDLE ts=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0); THREADENTRY32 te={sizeof(te)};
    if(ts==INVALID_HANDLE_VALUE)return 3; int n=0;
    if(Thread32First(ts,&te)) do { if(te.th32OwnerProcessID!=pid)continue;
        HANDLE t=OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|THREAD_SUSPEND_RESUME,FALSE,te.th32ThreadID); if(!t)continue;
        if(SuspendThread(t)!=(DWORD)-1){CONTEXT c;ZeroMemory(&c,sizeof(c));c.ContextFlags=CONTEXT_DEBUG_REGISTERS;
            if(GetThreadContext(t,&c)){c.Dr0=0x00414D90;c.Dr7|=1u; c.Dr6=0; if(SetThreadContext(t,&c))n++;}
            ResumeThread(t);} CloseHandle(t);
    } while(Thread32Next(ts,&te)); CloseHandle(ts); wprintf(L"HWREG_ADDED pid=%lu target=004B9362 threads=%d\n",pid,n); return 0;
}

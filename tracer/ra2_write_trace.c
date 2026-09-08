#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>

static const wchar_t *kPath = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static const uintptr_t kExec[4] = {0x00482A45,0x004E5E04,0x005F2B0D,0x0079467E};
static DWORD g_pid;

static DWORD find_target(void){
    HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); if(s==INVALID_HANDLE_VALUE)return 0;
    PROCESSENTRY32W e={0}; e.dwSize=sizeof(e); DWORD r=0;
    if(Process32FirstW(s,&e)) do { if(!_wcsicmp(e.szExeFile,L"game.exe")){
        HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);
        if(p){wchar_t path[MAX_PATH];DWORD n=MAX_PATH;
            if(QueryFullProcessImageNameW(p,0,path,&n)&&!_wcsicmp(path,kPath))r=e.th32ProcessID; CloseHandle(p);}
    }} while(!r&&Process32NextW(s,&e)); CloseHandle(s); return r;
}
static void set_hw(HANDLE t){
    CONTEXT c={0}; c.ContextFlags=CONTEXT_DEBUG_REGISTERS;
    if(!GetThreadContext(t,&c))return;
    c.Dr0=kExec[0]; c.Dr1=kExec[1]; c.Dr2=kExec[2]; c.Dr3=kExec[3]; c.Dr6=0;
    c.Dr7=(1u<<0)|(1u<<2)|(1u<<4)|(1u<<6);
    SetThreadContext(t,&c);
}
static void clear_hw(HANDLE t){
    CONTEXT c={0}; c.ContextFlags=CONTEXT_DEBUG_REGISTERS;
    if(GetThreadContext(t,&c)){c.Dr7=0;c.Dr6=0;SetThreadContext(t,&c);}
}
static void log_hit(DEBUG_EVENT *ev, uintptr_t base){
    CONTEXT c={0}; c.ContextFlags=CONTEXT_FULL|CONTEXT_DEBUG_REGISTERS;
    HANDLE t=OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT,FALSE,ev->dwThreadId); if(!t)return;
    if(GetThreadContext(t,&c)){
        uintptr_t eip=(uintptr_t)c.Eip; uintptr_t rel=eip>=base?eip-base:0;
        printf("event tid=%lu eip=0x%08lX module_offset=0x%08lX edi=0x%08lX esi=0x%08lX eax=0x%08lX dr6=0x%08lX\n",ev->dwThreadId,(unsigned long)eip,(unsigned long)rel,(unsigned long)c.Edi,(unsigned long)c.Esi,(unsigned long)c.Eax,(unsigned long)c.Dr6);
        fflush(stdout);
    } CloseHandle(t);
}
int wmain(void){
    g_pid=find_target(); if(!g_pid){fwprintf(stderr,L"未找到目标 game.exe。\n");return 2;}
    if(!DebugActiveProcess(g_pid)){fwprintf(stderr,L"无法附加，错误码=%lu。\n",GetLastError());return 3;}
    DebugSetProcessKillOnExit(FALSE); printf("attached pid=%lu exec_points=0x00482A45,0x004E5E04,0x005F2B0D,0x0079467E\n",g_pid); fflush(stdout);
    DEBUG_EVENT ev; uintptr_t base=0; BOOL done=FALSE; DWORD started=GetTickCount();
    const DWORD timeout_ms=120000;
    while(!done && GetTickCount()-started < timeout_ms){
        if(!WaitForDebugEvent(&ev,1000)) continue;
        DWORD cont=DBG_CONTINUE;
        if(ev.dwDebugEventCode==CREATE_PROCESS_DEBUG_EVENT){base=(uintptr_t)ev.u.CreateProcessInfo.lpBaseOfImage;set_hw(ev.u.CreateProcessInfo.hThread);if(ev.u.CreateProcessInfo.hFile)CloseHandle(ev.u.CreateProcessInfo.hFile);}
        else if(ev.dwDebugEventCode==CREATE_THREAD_DEBUG_EVENT)set_hw(ev.u.CreateThread.hThread);
        else if(ev.dwDebugEventCode==EXCEPTION_DEBUG_EVENT){
            DWORD code=ev.u.Exception.ExceptionRecord.ExceptionCode;
            if(code==EXCEPTION_SINGLE_STEP){log_hit(&ev,base);} else cont=DBG_EXCEPTION_NOT_HANDLED;
        } else if(ev.dwDebugEventCode==EXIT_PROCESS_DEBUG_EVENT)done=TRUE;
        ContinueDebugEvent(ev.dwProcessId,ev.dwThreadId,cont);
    }
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
    if(snapshot!=INVALID_HANDLE_VALUE){
        THREADENTRY32 te={0}; te.dwSize=sizeof(te);
        if(Thread32First(snapshot,&te)) do { if(te.th32OwnerProcessID==g_pid){
            HANDLE t=OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT,FALSE,te.th32ThreadID);
            if(t){clear_hw(t);CloseHandle(t);}
        }} while(Thread32Next(snapshot,&te));
        CloseHandle(snapshot);
    }
    DebugActiveProcessStop(g_pid); return 0;
}

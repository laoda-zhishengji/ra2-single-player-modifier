#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>

static const wchar_t kPath[] = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static const uintptr_t kEntry = 0x004991D0;
static const uintptr_t kCaller = 0x0049BBC9;

static DWORD find_target(void) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W e = { sizeof(e) }; DWORD r = 0;
    if (s == INVALID_HANDLE_VALUE) return 0;
    if (Process32FirstW(s, &e)) do {
        if (!_wcsicmp(e.szExeFile, L"game.exe")) {
            HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID);
            wchar_t q[MAX_PATH]; DWORD n = MAX_PATH;
            if (p && QueryFullProcessImageNameW(p, 0, q, &n) && !_wcsicmp(q, kPath)) r = e.th32ProcessID;
            if (p) CloseHandle(p);
        }
    } while (!r && Process32NextW(s, &e));
    CloseHandle(s); return r;
}

static BOOL patch_byte(HANDLE p, uintptr_t a, BYTE b, BYTE *old) {
    SIZE_T n = 0; if (!ReadProcessMemory(p, (void *)a, old, 1, &n) || n != 1) return FALSE;
    if (!WriteProcessMemory(p, (void *)a, &b, 1, &n) || n != 1) return FALSE;
    return FlushInstructionCache(p, (void *)a, 1);
}

static BOOL read_u32(HANDLE p, uintptr_t a, DWORD *v) {
    SIZE_T n = 0; return ReadProcessMemory(p, (void *)a, v, 4, &n) && n == 4;
}

int wmain(void) {
    DWORD pid = find_target();
    if (!pid) { wprintf(L"未找到 Steam 红警2 game.exe。\n"); return 2; }
    if (!DebugActiveProcess(pid)) { wprintf(L"无法附加调试器，错误=%lu\n", GetLastError()); return 3; }
    HANDLE p = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, pid);
    if (!p) { DebugActiveProcessStop(pid); return 4; }
    BYTE original = 0; if (!patch_byte(p, kEntry, 0xCC, &original)) { CloseHandle(p); DebugActiveProcessStop(pid); return 5; }
    wprintf(L"TRACE_READY 请在游戏中分别尝试一次基地附近和远距离放置；按 Ctrl+C 结束。\n");
    BOOL running = TRUE; DWORD pending_return = 0, pending_ctx = 0; BYTE return_old = 0;
    while (running) {
        DEBUG_EVENT ev; if (!WaitForDebugEvent(&ev, 1000)) continue;
        DWORD status = DBG_CONTINUE;
        if (ev.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
            CONTEXT c = {0}; c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
            HANDLE th = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, ev.dwThreadId);
            if (th && GetThreadContext(th, &c)) {
                DWORD ex = ev.u.Exception.ExceptionRecord.ExceptionCode;
                if (ex == EXCEPTION_BREAKPOINT && c.Eip == kEntry + 1) {
                    DWORD ret=0,a=0,b=0,d=0; read_u32(p,c.Esp,&ret); read_u32(p,c.Esp+4,&a); read_u32(p,c.Esp+8,&b); read_u32(p,c.Esp+0xC,&d);
                    wprintf(L"CALL 4991D0 ecx=%08X ret=%08X args=%08X,%08X,%08X\n",c.Ecx,ret,a,b,d);
                    SIZE_T n=0; WriteProcessMemory(p,(void*)kEntry,&original,1,&n); FlushInstructionCache(p,(void*)kEntry,1);
                    if (ret && patch_byte(p,ret,0xCC,&return_old)) { pending_return=ret; pending_ctx=c.Ecx; }
                    c.Eip=(DWORD)kEntry; SetThreadContext(th,&c);
                } else if (ex == EXCEPTION_BREAKPOINT && pending_return && c.Eip == pending_return + 1) {
                    DWORD f1=0, f2=0; read_u32(p,pending_ctx+0x117C,&f1); read_u32(p,pending_ctx+0x117D,&f2);
                    wprintf(L"RETURN ctx=%08X flags117C=%u flags117D=%u\n",pending_ctx,f1&0xff,f2&0xff);
                    SIZE_T n=0; WriteProcessMemory(p,(void*)pending_return,&return_old,1,&n); FlushInstructionCache(p,(void*)pending_return,1);
                    pending_return=0; pending_ctx=0;
                    patch_byte(p,kEntry,0xCC,&original);
                    c.Eip=(DWORD)(pending_return ? pending_return : 0); /* overwritten below */
                    c.Eip=(DWORD)(ev.u.Exception.ExceptionRecord.ExceptionAddress);
                    c.Eip=(DWORD)((uintptr_t)ev.u.Exception.ExceptionRecord.ExceptionAddress);
                    /* ExceptionAddress is the INT3 byte; execute the restored return byte. */
                    c.Eip=(DWORD)((uintptr_t)ev.u.Exception.ExceptionRecord.ExceptionAddress);
                    SetThreadContext(th,&c);
                }
            }
            if (th) CloseHandle(th);
        } else if (ev.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) running=FALSE;
        ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, status);
    }
    WriteProcessMemory(p,(void*)kEntry,&original,1,&(SIZE_T){0}); FlushInstructionCache(p,(void*)kEntry,1);
    CloseHandle(p); DebugActiveProcessStop(pid); return 0;
}

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>

// Diagnostic only: uses per-thread x86 hardware breakpoints. It does not
// write game code or game data. Target is the current Steam game.exe build.
// The post-call addresses capture EAX immediately after the exit-cell search.
static const DWORD Targets[4] = { 0x00414F44, 0x00414F49, 0x00414F7B, 0x00414F80 };
static DWORD gPid = 0;
static volatile LONG gStop = 0;

static BOOL WINAPI OnControl(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
        InterlockedExchange(&gStop, 1);
        if (gPid) DebugActiveProcessStop(gPid);
        return TRUE;
    }
    return FALSE;
}

static void Arm(DWORD tid) {
    HANDLE t = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME,
                          FALSE, tid);
    if (!t) return;
    CONTEXT c;
    ZeroMemory(&c, sizeof(c));
    c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (GetThreadContext(t, &c)) {
        c.Dr0 = Targets[0]; c.Dr1 = Targets[1];
        c.Dr2 = Targets[2]; c.Dr3 = Targets[3];
        c.Dr7 |= 0x55u;     // local execute breakpoints in DR0..DR3
        c.Dr6 = 0;
        SetThreadContext(t, &c);
    }
    CloseHandle(t);
}

static void ArmAll(DWORD pid) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (s == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 e;
    e.dwSize = sizeof(e);
    if (Thread32First(s, &e)) do {
        if (e.th32OwnerProcessID == pid) Arm(e.th32ThreadID);
    } while (Thread32Next(s, &e));
    CloseHandle(s);
}

static DWORD FindGame(void) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W e;
    e.dwSize = sizeof(e);
    DWORD pid = 0;
    if (Process32FirstW(s, &e)) do {
        if (!_wcsicmp(e.szExeFile, L"game.exe")) { pid = e.th32ProcessID; break; }
    } while (Process32NextW(s, &e));
    CloseHandle(s);
    return pid;
}

int wmain(int argc, wchar_t** argv) {
    DWORD pid = FindGame();
    BOOL waitForGame = argc > 1 && !_wcsicmp(argv[1], L"--wait");
    while (!pid && waitForGame) {
        wprintf(L"WAITING_FOR_GAME\n");
        Sleep(500);
        pid = FindGame();
    }
    if (!pid) { wprintf(L"GAME_NOT_FOUND\n"); return 2; }
    gPid = pid;
    SetConsoleCtrlHandler(OnControl, TRUE);
    wprintf(L"HWTRACE_READ_ONLY pid=%lu targets=414F44,414F49,414F7B,414F80\n", pid);
    if (!DebugActiveProcess(pid)) { wprintf(L"DEBUG_ATTACH_FAILED error=%lu\n", GetLastError()); return 3; }
    ArmAll(pid);

    DEBUG_EVENT ev;
    for (;;) {
        if (gStop) break;
        if (!WaitForDebugEvent(&ev, 1000)) {
            if (GetLastError() == ERROR_SEM_TIMEOUT) continue;
            break;
        }
        DWORD cont = DBG_CONTINUE;
        if (ev.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT) Arm(ev.dwThreadId);
        else if (ev.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
            const EXCEPTION_RECORD* x = &ev.u.Exception.ExceptionRecord;
            if (x->ExceptionCode == EXCEPTION_SINGLE_STEP) {
                HANDLE t = OpenThread(THREAD_GET_CONTEXT, FALSE, ev.dwThreadId);
                CONTEXT c; ZeroMemory(&c, sizeof(c)); c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_DEBUG_REGISTERS;
                if (t && GetThreadContext(t, &c)) {
                    DWORD arg0 = 0, arg1 = 0;
                    HANDLE p = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, ev.dwProcessId);
                    if (p) {
                        SIZE_T got;
                        ReadProcessMemory(p, (void*)c.Esp, &arg0, 4, &got);
                        ReadProcessMemory(p, (void*)(c.Esp + 4), &arg1, 4, &got);
                        CloseHandle(p);
                    }
                    wprintf(L"HIT tid=%lu dr6=%08lX eip=%08lX eax=%08lX ecx=%08lX edx=%08lX esi=%08lX edi=%08lX esp=%08lX stack0=%08lX stack1=%08lX\n",
                        ev.dwThreadId, c.Dr6, c.Eip, c.Eax, c.Ecx, c.Edx, c.Esi, c.Edi, c.Esp, arg0, arg1);
                    c.Dr6 = 0; c.EFlags |= 0x10000u; SetThreadContext(t, &c);
                }
                if (t) CloseHandle(t);
            } else cont = DBG_EXCEPTION_NOT_HANDLED;
        } else if (ev.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) break;
        ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, cont);
    }
    DebugActiveProcessStop(pid);
    return 0;
}

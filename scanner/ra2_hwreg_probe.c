#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

int wmain(void) {
    DWORD pid = 0;
    HANDLE ps = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (ps != INVALID_HANDLE_VALUE && Process32FirstW(ps, &pe)) do {
        if (!_wcsicmp(pe.szExeFile, L"game.exe")) { pid = pe.th32ProcessID; break; }
    } while (Process32NextW(ps, &pe));
    if (ps != INVALID_HANDLE_VALUE) CloseHandle(ps);
    if (!pid) { wprintf(L"GAME_NOT_FOUND\n"); return 2; }

    HANDLE ts = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te = { sizeof(te) };
    if (ts == INVALID_HANDLE_VALUE) return 3;
    wprintf(L"HWREG_READ_ONLY pid=%lu\n", pid);
    if (Thread32First(ts, &te)) do {
        if (te.th32OwnerProcessID != pid) continue;
        HANDLE t = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
        if (!t) { wprintf(L"TID=%lu OPEN_FAIL=%lu\n", te.th32ThreadID, GetLastError()); continue; }
        if (SuspendThread(t) == (DWORD)-1) { CloseHandle(t); continue; }
        CONTEXT c; ZeroMemory(&c, sizeof(c)); c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        BOOL ok = GetThreadContext(t, &c);
        ResumeThread(t);
        if (ok) wprintf(L"TID=%lu DR0=%08lX DR1=%08lX DR2=%08lX DR3=%08lX DR6=%08lX DR7=%08lX\n", te.th32ThreadID, c.Dr0, c.Dr1, c.Dr2, c.Dr3, c.Dr6, c.Dr7);
        else wprintf(L"TID=%lu GETCTX_FAIL=%lu\n", te.th32ThreadID, GetLastError());
        CloseHandle(t);
    } while (Thread32Next(ts, &te));
    CloseHandle(ts);
    return 0;
}

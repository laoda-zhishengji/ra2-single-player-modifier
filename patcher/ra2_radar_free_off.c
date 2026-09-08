#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>

static DWORD find_pid(void) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W e = { sizeof(e) }; DWORD id = 0;
    if (s != INVALID_HANDLE_VALUE && Process32FirstW(s, &e)) do {
        if (!_wcsicmp(e.szExeFile, L"game.exe")) { id = e.th32ProcessID; break; }
    } while (Process32NextW(s, &e));
    if (s != INVALID_HANDLE_VALUE) CloseHandle(s); return id;
}
static void put32(BYTE *p, DWORD v) { memcpy(p, &v, 4); }

int wmain(void) {
    DWORD id = find_pid(); if (!id) return 2;
    HANDLE p = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_CREATE_THREAD, FALSE, id);
    if (!p) return 3;
    BYTE zero = 0; SIZE_T n = 0;
    BOOL ok = WriteProcessMemory(p, (LPVOID)0x00833988, &zero, 1, &n) && n == 1;
    BYTE stub[32]; DWORD k = 0;
    stub[k++] = 0xB9; put32(stub+k, 0x008324E0); k += 4;
    stub[k++] = 0x6A; stub[k++] = 0x00; stub[k++] = 0xE8;
    LPVOID mem = VirtualAllocEx(p, NULL, 0x1000, MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) { CloseHandle(p); return 4; }
    put32(stub+k, 0x00633140 - ((DWORD)(uintptr_t)mem + k + 4)); k += 4;
    stub[k++] = 0xC3;
    if (ok) ok = WriteProcessMemory(p, mem, stub, k, &n) && n == k;
    HANDLE t = ok ? CreateRemoteThread(p, NULL, 0, (LPTHREAD_START_ROUTINE)mem, NULL, 0, NULL) : NULL;
    if (t) { ok = WaitForSingleObject(t, 1000) == WAIT_OBJECT_0; CloseHandle(t); }
    VirtualFreeEx(p, mem, 0, MEM_RELEASE); CloseHandle(p);
    wprintf(L"RADAR_FREE_OFF %ls\n", ok ? L"completed" : L"failed"); return ok ? 0 : 5;
}

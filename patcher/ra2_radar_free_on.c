#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>

static const wchar_t path[] = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static DWORD pid(void) {
    DWORD pids[256], n = 0;
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W e = { sizeof(e) };
    if (Process32FirstW(s, &e)) do {
        if (!_wcsicmp(e.szExeFile, L"game.exe")) { pids[n++] = e.th32ProcessID; break; }
    } while (Process32NextW(s, &e) && n < 256);
    CloseHandle(s);
    return n ? pids[0] : 0;
}
static void put32(BYTE *p, DWORD v) { memcpy(p, &v, 4); }

int wmain(void) {
    DWORD id = pid();
    if (!id) return 2;
    HANDLE p = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE | PROCESS_CREATE_THREAD, FALSE, id);
    if (!p) return 3;

    /* game.exe+0x4324E0+0x14A8: map-provided/free-radar flag. */
    BYTE one = 0;
    SIZE_T wrote = 0;
    BOOL ok = WriteProcessMemory(p, (LPVOID)0x00833988, &one, 1, &wrote) && wrote == 1;

    /* Clear free radar; the normal radar display is refreshed afterward. */
    BYTE stub[32]; DWORD k = 0;
    stub[k++] = 0xB9; put32(stub + k, 0x008324E0); k += 4;
    stub[k++] = 0x6A; stub[k++] = 0x01;
    stub[k++] = 0xE8;
    LPVOID mem = VirtualAllocEx(p, NULL, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) { CloseHandle(p); return 4; }
    put32(stub + k, 0x00633140 - ((DWORD)(uintptr_t)mem + k + 4)); k += 4;
    stub[k++] = 0xC3;
    if (ok) ok = WriteProcessMemory(p, mem, stub, k, &wrote) && wrote == k;
    HANDLE t = ok ? CreateRemoteThread(p, NULL, 0, (LPTHREAD_START_ROUTINE)mem, NULL, 0, NULL) : NULL;
    if (t) { ok = WaitForSingleObject(t, 1000) == WAIT_OBJECT_0; CloseHandle(t); }
    VirtualFreeEx(p, mem, 0, MEM_RELEASE);
    CloseHandle(p);
    wprintf(L"RADAR_FREE_ON %ls\n", ok ? L"completed" : L"failed");
    return ok ? 0 : 5;
}

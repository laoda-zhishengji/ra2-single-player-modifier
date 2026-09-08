#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>

static const wchar_t path[] = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";

static DWORD find_pid(void) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W e = {0};
    DWORD result = 0;
    e.dwSize = sizeof(e);
    if (s == INVALID_HANDLE_VALUE) return 0;
    if (Process32FirstW(s, &e)) do {
        if (!_wcsicmp(e.szExeFile, L"game.exe")) {
            HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID);
            wchar_t q[MAX_PATH];
            DWORD n = MAX_PATH;
            if (h && QueryFullProcessImageNameW(h, 0, q, &n) && !_wcsicmp(q, path)) result = e.th32ProcessID;
            if (h) CloseHandle(h);
        }
    } while (!result && Process32NextW(s, &e));
    CloseHandle(s);
    return result;
}

static BOOL read_dword(HANDLE p, DWORD address, DWORD *value) {
    SIZE_T n = 0;
    return ReadProcessMemory(p, (void *)(uintptr_t)address, value, 4, &n) && n == 4;
}

static BOOL write_byte(HANDLE p, DWORD address, BYTE value) {
    SIZE_T n = 0;
    return WriteProcessMemory(p, (void *)(uintptr_t)address, &value, 1, &n) && n == 1;
}

static BOOL write_dword(HANDLE p, DWORD address, DWORD value) {
    SIZE_T n = 0;
    return WriteProcessMemory(p, (void *)(uintptr_t)address, &value, 4, &n) && n == 4;
}

int wmain(void) {
    DWORD last_object[7] = {0};
    BYTE seen[7] = {0};
    BYTE last_available[7] = {0};
    for (;;) {
        DWORD id = find_pid();
        if (!id) return 0;
        HANDLE p = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, id);
        if (!p) return 2;
        DWORD player = 0, array = 0;
        if (read_dword(p, 0x00A35DB4, &player) && player && read_dword(p, player + 0x1A0, &array) && array) {
            for (DWORD i = 0; i < 7; ++i) {
                DWORD object = 0;
                if (!read_dword(p, array + i * 4, &object) || object < 0x10000) continue;
                if (last_object[i] != object) {
                    last_object[i] = object;
                    seen[i] = 0;
                    last_available[i] = 0;
                }
                BYTE ready = 0, wait = 0, active = 0;
                SIZE_T n = 0;
                ReadProcessMemory(p, (void *)(uintptr_t)(object + 0x57), &ready, 1, &n);
                ReadProcessMemory(p, (void *)(uintptr_t)(object + 0x58), &wait, 1, &n);
                ReadProcessMemory(p, (void *)(uintptr_t)(object + 0x48), &active, 1, &n);
                BYTE available = (BYTE)(ready && !wait && !active);
                BYTE falling_edge = (BYTE)(seen[i] && last_available[i] && !available);
                BYTE repair = (BYTE)(!seen[i] || falling_edge);
                if (repair) {
                    DWORD now = 0;
                    read_dword(p, 0x00A40D2C, &now);
                    write_byte(p, object + 0x48, 0);
                    write_byte(p, object + 0x58, 0);
                    write_byte(p, object + 0x57, 1);
                    write_dword(p, object + 0x2C, now);
                    write_dword(p, object + 0x30, 0);
                    write_dword(p, object + 0x34, 0);
                }
                if (i == 0) write_byte(p, object + 0x56, 1);
                seen[i] = 1;
                last_available[i] = repair ? 0 : available;
            }
        }
        CloseHandle(p);
        Sleep(100);
    }
}

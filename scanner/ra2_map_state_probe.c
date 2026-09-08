#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static const wchar_t path[] = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static DWORD find_pid(void) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W e = { sizeof(e) }; DWORD id = 0;
    if (s != INVALID_HANDLE_VALUE && Process32FirstW(s, &e)) do {
        if (!_wcsicmp(e.szExeFile, L"game.exe")) {
            HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID);
            wchar_t q[MAX_PATH]; DWORD n = MAX_PATH;
            if (h && QueryFullProcessImageNameW(h, 0, q, &n) && !_wcsicmp(q, path)) id = e.th32ProcessID;
            if (h) CloseHandle(h); if (id) break;
        }
    } while (Process32NextW(s, &e));
    if (s != INVALID_HANDLE_VALUE) CloseHandle(s); return id;
}
static int rd(HANDLE p, uintptr_t a, void *v, SIZE_T n) {
    SIZE_T got = 0; return ReadProcessMemory(p, (LPCVOID)a, v, n, &got) && got == n;
}
int wmain(void) {
    DWORD id = find_pid(); if (!id) { wprintf(L"GAME_NOT_FOUND\n"); return 2; }
    HANDLE p = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, id);
    if (!p) { wprintf(L"OPEN_FAILED\n"); return 3; }
    BYTE freeRadar=0, radar=0, display=0, shroud=0, dynamicFree=0; DWORD player=0, radarObject=0;
    BOOL ok = rd(p, 0x00833988, &freeRadar, 1) &&
              rd(p, 0x00A339B4, &radar, 1) &&
              rd(p, 0x008324E0 + 0x14D4, &display, 1) &&
              rd(p, 0x00A3D290, &radarObject, 4) &&
              rd(p, 0x00A35DB4, &player, 4);
    if (ok && radarObject) ok = rd(p, (uintptr_t)radarObject + 0x2228, &dynamicFree, 1);
    if (ok && player) ok = rd(p, (uintptr_t)player + 0x188, &shroud, 1);
    wprintf(L"MAP_STATE fixedFree=%u radarFlag=%u display=%u radarObject=%08X dynamicFree=%u player=%08X shroud=%u\n",
        freeRadar, radar, display, radarObject, dynamicFree, player, shroud);
    CloseHandle(p); return ok ? 0 : 4;
}

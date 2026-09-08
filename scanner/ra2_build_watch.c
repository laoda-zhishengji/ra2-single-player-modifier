#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>

static const wchar_t kPath[] = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";

static DWORD find_target(void) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); PROCESSENTRY32W e = {0}; DWORD r = 0;
    if (s == INVALID_HANDLE_VALUE) return 0; e.dwSize = sizeof(e);
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

typedef struct { uintptr_t address; int32_t before; } Candidate;

int wmain(void) {
    DWORD pid = find_target(); if (!pid) { wprintf(L"未找到目标 game.exe。\n"); return 2; }
    HANDLE p = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!p) return 3;
    Candidate *c = NULL; size_t count = 0, cap = 0;
    MEMORY_BASIC_INFORMATION mbi; uintptr_t cur = 0;
    while (VirtualQueryEx(p, (void *)cur, &mbi, sizeof(mbi)) == sizeof(mbi)) {
        DWORD prot = mbi.Protect & 0xff;
        BOOL readable = prot == PAGE_READONLY || prot == PAGE_READWRITE || prot == PAGE_WRITECOPY || prot == PAGE_EXECUTE_READ || prot == PAGE_EXECUTE_READWRITE || prot == PAGE_EXECUTE_WRITECOPY;
        if (readable && (mbi.State == MEM_COMMIT) && !(mbi.Protect & PAGE_GUARD)) {
            BYTE buf[4096];
            for (SIZE_T page = 0; page < mbi.RegionSize; page += sizeof(buf)) {
                SIZE_T chunk = mbi.RegionSize - page; if (chunk > sizeof(buf)) chunk = sizeof(buf);
                SIZE_T got = 0;
                if (!ReadProcessMemory(p, (BYTE *)mbi.BaseAddress + page, buf, chunk, &got)) continue;
                for (SIZE_T off = 0; off + 4 <= got; off += 4) {
                    int32_t v; memcpy(&v, buf + off, 4);
                    if (v >= 1 && v <= 100000) {
                        if (count == cap) { size_t nc = cap ? cap * 2 : 4096; Candidate *n = c ? (Candidate *)HeapReAlloc(GetProcessHeap(), 0, c, nc * sizeof(*c)) : (Candidate *)HeapAlloc(GetProcessHeap(), 0, nc * sizeof(*c)); if (!n) break; c = n; cap = nc; }
                        if (count < cap) { c[count].address = (uintptr_t)mbi.BaseAddress + page + off; c[count].before = v; ++count; }
                    }
                }
            }
        }
        cur = (uintptr_t)mbi.BaseAddress + mbi.RegionSize; if (!cur) break;
    }
    if (count == 0) {
        uintptr_t root = 0; DWORD root32 = 0; SIZE_T got = 0;
        if (ReadProcessMemory(p, (void *)0x00A35DB4, &root32, sizeof(root32), &got) && got == sizeof(root32) && root32) {
            root = root32;
            uintptr_t start = root > 0x10000 ? root - 0x10000 : root;
            BYTE buf[4096];
            for (uintptr_t addr = start; addr < root + 0x10000; addr += sizeof(buf)) {
                if (!ReadProcessMemory(p, (void *)addr, buf, sizeof(buf), &got)) continue;
                for (SIZE_T off = 0; off + 4 <= got; off += 4) {
                    int32_t v; memcpy(&v, buf + off, 4);
                    if (v >= 1 && v <= 100000) {
                        if (count == cap) { size_t nc = cap ? cap * 2 : 4096; Candidate *n = c ? (Candidate *)HeapReAlloc(GetProcessHeap(), 0, c, nc * sizeof(*c)) : (Candidate *)HeapAlloc(GetProcessHeap(), 0, nc * sizeof(*c)); if (!n) break; c = n; cap = nc; }
                        if (count < cap) { c[count].address = addr + off; c[count].before = v; ++count; }
                    }
                }
            }
        }
    }
    wprintf(L"pid=%lu candidates=%zu; waiting 3 seconds...\n", pid, count); Sleep(3000);
    size_t printed = 0;
    for (size_t i = 0; i < count && printed < 200; ++i) {
        int32_t after = 0; SIZE_T got = 0;
        int64_t delta;
        if (ReadProcessMemory(p, (void *)c[i].address, &after, 4, &got) && got == 4 && after != c[i].before && after >= 0 && after <= 100000) {
            delta = (int64_t)after - c[i].before;
            if (c[i].address < 0x10000000 || delta > 1000 || delta < -1000) continue;
            wprintf(L"%p: %ld -> %ld (%ls)\n", (void *)c[i].address, (long)c[i].before, (long)after, after < c[i].before ? L"decrease" : L"increase");
            ++printed;
        }
    }
    wprintf(L"READ_ONLY=TRUE results=%zu\n", printed);
    if (c) HeapFree(GetProcessHeap(), 0, c); CloseHandle(p); return 0;
}

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdint.h>
#include <stdio.h>

static const wchar_t kPath[] = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";

static DWORD find_pid(void) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W e = { sizeof(e) }; DWORD r = 0;
    if (s == INVALID_HANDLE_VALUE) return 0;
    if (Process32FirstW(s, &e)) do {
        if (!_wcsicmp(e.szExeFile, L"game.exe")) {
            HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID);
            wchar_t q[MAX_PATH]; DWORD n = ARRAYSIZE(q);
            if (p && QueryFullProcessImageNameW(p, 0, q, &n) && !_wcsicmp(q, kPath)) r = e.th32ProcessID;
            if (p) CloseHandle(p);
        }
    } while (!r && Process32NextW(s, &e));
    CloseHandle(s); return r;
}

static int readable(DWORD protect) {
    DWORD p = protect & 0xff;
    return p == PAGE_READONLY || p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
           p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
}

int wmain(void) {
    DWORD pid = find_pid();
    if (!pid) { wprintf(L"未找到匹配路径的 Steam game.exe。\n"); return 2; }
    HANDLE p = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!p) return 3;
    MEMORY_BASIC_INFORMATION mbi; uintptr_t cur = 0; BYTE buf[65536]; int hits = 0;
    while (VirtualQueryEx(p, (LPCVOID)cur, &mbi, sizeof(mbi)) == sizeof(mbi)) {
        if (mbi.State == MEM_COMMIT && readable(mbi.Protect) && !(mbi.Protect & PAGE_GUARD)) {
            SIZE_T off = 0;
            while (off < mbi.RegionSize) {
                SIZE_T want = mbi.RegionSize - off; if (want > sizeof(buf)) want = sizeof(buf);
                SIZE_T got = 0;
                if (ReadProcessMemory(p, (BYTE*)mbi.BaseAddress + off, buf, want, &got) && got >= 44) {
                    for (SIZE_T i = 0; i + 44 <= got; i += 4) {
                        int v[11]; memcpy(v, buf + i, sizeof(v));
                        if (v[0] != 5 || v[1] != 4 || v[2] != 5 || v[3] != 4 ||
                            v[4] < 0 || v[4] > 5 || v[5] != 2 || v[6] != 3 ||
                            v[7] != 4 || v[8] != 4 || v[9] != 3 || v[10] != 2) continue;
                        uintptr_t a = (uintptr_t)mbi.BaseAddress + off + i;
                        wprintf(L"IQ_CANDIDATE addr=0x%08lX MaxIQ=%d Super=%d Prod=%d Guard=%d RepairSell=%d Crush=%d Scatter=%d Content=%d Aircraft=%d Harvester=%d SellBack=%d\n",
                                (unsigned long)a, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10]);
                        if (++hits >= 32) { CloseHandle(p); return 0; }
                    }
                }
                off += want;
            }
        }
        uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (next <= cur) break; cur = next;
    }
    wprintf(L"IQ_CANDIDATES=%d\n", hits); CloseHandle(p); return hits ? 0 : 4;
}

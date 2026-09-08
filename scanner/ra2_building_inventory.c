#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static const wchar_t kPath[] =
    L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static const DWORD kTechnoVtable = 0x0079B12C;
static const DWORD kTypeVtable = 0x0079D1F8;

static DWORD find_pid(void) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W e = {sizeof(e)};
    DWORD result = 0;
    if (s == INVALID_HANDLE_VALUE) return 0;
    if (Process32FirstW(s, &e)) do {
        if (!_wcsicmp(e.szExeFile, L"game.exe")) {
            HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                   e.th32ProcessID);
            wchar_t q[MAX_PATH];
            DWORD n = ARRAYSIZE(q);
            if (p && QueryFullProcessImageNameW(p, 0, q, &n) &&
                !_wcsicmp(q, kPath)) result = e.th32ProcessID;
            if (p) CloseHandle(p);
        }
    } while (!result && Process32NextW(s, &e));
    CloseHandle(s);
    return result;
}

static BOOL readable(DWORD protect) {
    DWORD p = protect & 0xff;
    return p == PAGE_READONLY || p == PAGE_READWRITE ||
           p == PAGE_WRITECOPY || p == PAGE_EXECUTE_READ ||
           p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
}

static BOOL valid_id(const char *s) {
    int n = 0;
    while (n < 23 && s[n]) {
        unsigned char c = (unsigned char)s[n];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'))
            return FALSE;
        ++n;
    }
    return n > 2 && n < 23 && s[n] == 0;
}

int wmain(void) {
    DWORD pid = find_pid();
    if (!pid) { wprintf(L"未找到匹配路径的 Steam game.exe。\n"); return 2; }
    HANDLE p = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                           FALSE, pid);
    if (!p) return 3;

    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t cur = 0;
    BYTE buf[65536];
    int hits = 0;
    while (VirtualQueryEx(p, (LPCVOID)cur, &mbi, sizeof(mbi)) == sizeof(mbi)) {
        if (mbi.State == MEM_COMMIT && readable(mbi.Protect) &&
            !(mbi.Protect & PAGE_GUARD)) {
            SIZE_T off = 0;
            while (off < mbi.RegionSize) {
                SIZE_T want = mbi.RegionSize - off;
                if (want > sizeof(buf)) want = sizeof(buf);
                SIZE_T got = 0;
                if (ReadProcessMemory(p, (BYTE *)mbi.BaseAddress + off,
                                       buf, want, &got) && got >= 4) {
                    for (SIZE_T i = 0; i + 4 <= got; i += 4) {
                        DWORD vt = 0;
                        memcpy(&vt, buf + i, sizeof(vt));
                        /* Derived BuildingClass/TechnoClass instances may
                         * replace the base vtable, so use the common type
                         * pointer at +0x418 as the primary discriminator. */
                        if (vt < 0x00400000 || vt >= 0x00800000) continue;
                        uintptr_t obj = (uintptr_t)mbi.BaseAddress + off + i;
                        DWORD type = 0, type_vt = 0, owner = 0;
                        BYTE repair = 0;
                        LONG health = 0, max_health = 0;
                        char id[24] = {0};
                        SIZE_T n = 0;
                        if (!ReadProcessMemory(p, (LPCVOID)(obj + 0x418),
                                                &type, 4, &n) || n != 4 ||
                            !type ||
                            !ReadProcessMemory(p, (LPCVOID)type, &type_vt,
                                                4, &n) || n != 4 ||
                            type_vt != kTypeVtable ||
                            !ReadProcessMemory(p, (LPCVOID)(type + 0x24),
                                                id, 23, &n) || n != 23 ||
                            !valid_id(id)) continue;
                        ReadProcessMemory(p, (LPCVOID)(obj + 0x1B4), &owner, 4, &n);
                        ReadProcessMemory(p, (LPCVOID)(obj + 0x5B8), &repair, 1, &n);
                        ReadProcessMemory(p, (LPCVOID)(obj + 0x6C), &health, 4, &n);
                        ReadProcessMemory(p, (LPCVOID)(type + 0xA0), &max_health, 4, &n);
                        wprintf(L"OBJ %-10S obj=0x%08lX type=0x%08lX owner=0x%08lX repair=%u health=%ld/%ld\n",
                                id, (unsigned long)obj, (unsigned long)type,
                                (unsigned long)owner, (unsigned)repair,
                                (long)health, (long)max_health);
                        if (++hits >= 512) goto done;
                    }
                }
                off += want;
            }
        }
        cur = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (!cur) break;
    }
done:
    wprintf(L"READ_ONLY objects=%d type_offset=0x418 repair_offset=0x5B8\n",
            hits);
    CloseHandle(p);
    return 0;
}

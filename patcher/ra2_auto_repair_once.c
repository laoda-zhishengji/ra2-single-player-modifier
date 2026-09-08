#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#pragma comment(lib, "bcrypt.lib")

static const wchar_t kPath[] =
    L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static const wchar_t kSha256[] =
    L"73288C03B58D370BE268CA6D156B4E33BFDB2066DC980359467D8852FF3B00DF";
static const DWORD kTypeVtable = 0x0079D1F8;

static DWORD find_pid(void) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W e = {sizeof(e)}; DWORD r = 0;
    if (s == INVALID_HANDLE_VALUE) return 0;
    if (Process32FirstW(s, &e)) do {
        if (!_wcsicmp(e.szExeFile, L"game.exe")) {
            HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                   e.th32ProcessID);
            wchar_t q[MAX_PATH]; DWORD n = ARRAYSIZE(q);
            if (p && QueryFullProcessImageNameW(p, 0, q, &n) &&
                !_wcsicmp(q, kPath)) r = e.th32ProcessID;
            if (p) CloseHandle(p);
        }
    } while (!r && Process32NextW(s, &e));
    CloseHandle(s); return r;
}

static BOOL hash_ok(void) {
    HANDLE f = CreateFileW(kPath, GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, OPEN_EXISTING, 0, NULL);
    BCRYPT_ALG_HANDLE a = NULL; BCRYPT_HASH_HANDLE h = NULL;
    PUCHAR obj = NULL, dig = NULL; DWORD ol = 0, dl = 0, cb = 0;
    BYTE b[8192]; ULONG got = 0; wchar_t out[65]; BOOL ok = FALSE;
    if (f == INVALID_HANDLE_VALUE || BCryptOpenAlgorithmProvider(
        &a, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0) goto done;
    if (BCryptGetProperty(a, BCRYPT_OBJECT_LENGTH, (PUCHAR)&ol, 4, &cb, 0) < 0 ||
        BCryptGetProperty(a, BCRYPT_HASH_LENGTH, (PUCHAR)&dl, 4, &cb, 0) < 0) goto done;
    obj = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, ol);
    dig = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, dl);
    if (!obj || !dig || BCryptCreateHash(a, &h, obj, ol, NULL, 0, 0) < 0) goto done;
    for (;;) {
        if (!ReadFile(f, b, sizeof(b), &got, NULL)) goto done;
        if (!got) break;
        if (BCryptHashData(h, b, got, 0) < 0) goto done;
    }
    if (BCryptFinishHash(h, dig, dl, 0) < 0) goto done;
    for (DWORD i = 0; i < dl; ++i)
        swprintf_s(out + i * 2, ARRAYSIZE(out) - i * 2, L"%02X", dig[i]);
    out[64] = 0; ok = _wcsicmp(out, kSha256) == 0;
done:
    if (h) BCryptDestroyHash(h); if (a) BCryptCloseAlgorithmProvider(a, 0);
    if (obj) HeapFree(GetProcessHeap(), 0, obj);
    if (dig) HeapFree(GetProcessHeap(), 0, dig);
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    return ok;
}

static BOOL readable(DWORD protect) {
    DWORD p = protect & 0xff;
    return p == PAGE_READONLY || p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
           p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE ||
           p == PAGE_EXECUTE_WRITECOPY;
}

static BOOL valid_id(const char *s) {
    int n = 0;
    while (n < 23 && s[n]) {
        unsigned char c = (unsigned char)s[n++];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return FALSE;
    }
    return n > 2 && n < 23 && s[n] == 0;
}

int wmain(int argc, wchar_t **argv) {
    BOOL disable = argc == 2 && _wcsicmp(argv[1], L"--off") == 0;
    if (argc > 1 && !disable) {
        wprintf(L"Usage: ra2_auto_repair_once.exe [--off]\n");
        return 2;
    }
    DWORD pid = find_pid();
    if (!pid) { wprintf(L"未找到匹配路径的 Steam game.exe。\n"); return 2; }
    if (!hash_ok()) { wprintf(L"VERSION_MISMATCH: refusing process writes.\n"); return 3; }
    HANDLE p = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ |
                           PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, pid);
    if (!p) return 4;
    DWORD player = 0; SIZE_T n = 0;
    if (!ReadProcessMemory(p, (LPCVOID)0x00A35DB4, &player, 4, &n) ||
        n != 4 || !player) { CloseHandle(p); return 5; }
    MEMORY_BASIC_INFORMATION mbi; uintptr_t cur = 0; BYTE buf[65536]; int changed = 0;
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
                        DWORD vt = 0; memcpy(&vt, buf + i, 4);
                        if (vt < 0x00400000 || vt >= 0x00800000) continue;
                        uintptr_t obj = (uintptr_t)mbi.BaseAddress + off + i;
                        DWORD type = 0, type_vt = 0, owner = 0;
                        LONG health = 0, max_health = 0; BYTE repair = 0;
                        char id[24] = {0};
                        if (!ReadProcessMemory(p, (LPCVOID)(obj + 0x418), &type, 4, &n) ||
                            n != 4 || !type ||
                            !ReadProcessMemory(p, (LPCVOID)type, &type_vt, 4, &n) ||
                            n != 4 || type_vt != kTypeVtable ||
                            !ReadProcessMemory(p, (LPCVOID)(type + 0x24), id, 23, &n) ||
                            n != 23 || !valid_id(id)) continue;
                        if (!ReadProcessMemory(p, (LPCVOID)(obj + 0x1B4), &owner, 4, &n) ||
                            owner != player ||
                            !ReadProcessMemory(p, (LPCVOID)(obj + 0x6C), &health, 4, &n) ||
                            !ReadProcessMemory(p, (LPCVOID)(type + 0xA0), &max_health, 4, &n) ||
                            health <= 0 || max_health <= 0 || health >= max_health) continue;
                        ReadProcessMemory(p, (LPCVOID)(obj + 0x5B8), &repair, 1, &n);
                        if ((!disable && repair) || (disable && !repair)) continue;
                        BYTE one = disable ? 0 : 1; SIZE_T w = 0;
                        if (WriteProcessMemory(p, (LPVOID)(obj + 0x5B8), &one, 1, &w) && w == 1) {
                            wprintf(L"%ls obj=0x%08lX type=%S health=%ld/%ld flag=%d\n",
                                    disable ? L"REPAIR_OFF" : L"REPAIR_ON",
                                    (unsigned long)obj, id, (long)health,
                                    (long)max_health, disable ? 0 : 1);
                            ++changed;
                        }
                    }
                }
                off += want;
            }
        }
        cur = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        if (!cur) break;
    }
    wprintf(L"AUTO_REPAIR_%ls changed=%d player=0x%08lX\n",
            disable ? L"OFF" : L"ON", changed,
            (unsigned long)player);
    CloseHandle(p); return changed ? 0 : 6;
}

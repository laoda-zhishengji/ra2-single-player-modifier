#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "../product/ra2_product_manifest.h"

#pragma comment(lib, "bcrypt.lib")

/* Steam Red Alert II original, SHA-256 pinned to the tested 1.006 build. */
static const wchar_t *kPath = RA2_PRODUCT_GAME_PATH;
static const wchar_t *kSha256 = RA2_PRODUCT_SHA256;
static const DWORD kTypeVtable = 0x0079D1F8;
static volatile BOOL g_stop = FALSE;
static HANDLE g_stop_event = NULL;

typedef struct { uintptr_t obj; } Candidate;

static BOOL WINAPI handler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
        g_stop = TRUE; return TRUE;
    }
    return FALSE;
}

static DWORD find_pid(void) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W e = { sizeof(e) }; DWORD result = 0;
    if (s == INVALID_HANDLE_VALUE) return 0;
    if (Process32FirstW(s, &e)) do {
        if (!_wcsicmp(e.szExeFile, L"game.exe")) {
            HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID);
            wchar_t q[MAX_PATH]; DWORD n = ARRAYSIZE(q);
            if (p && QueryFullProcessImageNameW(p, 0, q, &n) && !_wcsicmp(q, kPath))
                result = e.th32ProcessID;
            if (p) CloseHandle(p);
        }
    } while (!result && Process32NextW(s, &e));
    CloseHandle(s); return result;
}

static BOOL version_ok(void) {
    HANDLE f = CreateFileW(kPath, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    BCRYPT_ALG_HANDLE a = NULL; BCRYPT_HASH_HANDLE h = NULL;
    PUCHAR obj = NULL, dig = NULL; DWORD ol = 0, dl = 0, cb = 0;
    BYTE b[8192]; ULONG got = 0; wchar_t out[65]; BOOL ok = FALSE;
    if (f == INVALID_HANDLE_VALUE || BCryptOpenAlgorithmProvider(&a, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0) goto done;
    if (BCryptGetProperty(a, BCRYPT_OBJECT_LENGTH, (PUCHAR)&ol, 4, &cb, 0) < 0 ||
        BCryptGetProperty(a, BCRYPT_HASH_LENGTH, (PUCHAR)&dl, 4, &cb, 0) < 0) goto done;
    obj = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, ol); dig = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, dl);
    if (!obj || !dig || BCryptCreateHash(a, &h, obj, ol, NULL, 0, 0) < 0) goto done;
    for (;;) { if (!ReadFile(f, b, sizeof(b), &got, NULL)) goto done; if (!got) break; if (BCryptHashData(h, b, got, 0) < 0) goto done; }
    if (BCryptFinishHash(h, dig, dl, 0) < 0) goto done;
    for (DWORD i = 0; i < dl; ++i) swprintf_s(out + i * 2, ARRAYSIZE(out) - i * 2, L"%02X", dig[i]);
    out[64] = 0; ok = _wcsicmp(out, kSha256) == 0;
done:
    if (h) BCryptDestroyHash(h); if (a) BCryptCloseAlgorithmProvider(a, 0);
    if (obj) HeapFree(GetProcessHeap(), 0, obj); if (dig) HeapFree(GetProcessHeap(), 0, dig);
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f); return ok;
}

static BOOL readable(DWORD protect) {
    DWORD p = protect & 0xff;
    return p == PAGE_READONLY || p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
           p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY;
}

static BOOL valid_id(const char *s) {
    int n = 0; while (n < 23 && s[n]) {
        unsigned char c = (unsigned char)s[n++];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return FALSE;
    }
    return n > 2 && n < 23 && s[n] == 0;
}

/* A player-owned CA* building is the game's observable result of garrisoning
 * a civilian building. Unoccupied civilian buildings remain neutral and are
 * therefore excluded by the owner test below. */
static BOOL is_civilian_id(const char *id) {
    if (id[0] != 'C' || id[1] != 'A') return FALSE;
    /* scenery/props with CA prefixes are not valid repair targets. */
    if (!_stricmp(id, "CAUSFGL") || !_strnicmp(id, "CAPARK", 6) ||
        !_strnicmp(id, "CAMISC", 6) || !_strnicmp(id, "CATEXS", 6)) return FALSE;
    return TRUE;
}

static int refresh_candidates(HANDLE p, Candidate *list, int capacity) {
    MEMORY_BASIC_INFORMATION mbi; uintptr_t cur = 0; BYTE buf[65536]; int count = 0;
    while (!g_stop && VirtualQueryEx(p, (LPCVOID)cur, &mbi, sizeof(mbi)) == sizeof(mbi)) {
        if (mbi.State == MEM_COMMIT && readable(mbi.Protect) && !(mbi.Protect & PAGE_GUARD)) {
            SIZE_T off = 0;
            while (!g_stop && off < mbi.RegionSize) {
                SIZE_T want = mbi.RegionSize - off; if (want > sizeof(buf)) want = sizeof(buf);
                SIZE_T got = 0;
                if (ReadProcessMemory(p, (BYTE *)mbi.BaseAddress + off, buf, want, &got) && got >= 4) {
                    for (SIZE_T i = 0; !g_stop && i + 4 <= got; i += 4) {
                        DWORD vt = 0, type = 0, type_vt = 0; char id[24] = {0}; SIZE_T n = 0;
                        memcpy(&vt, buf + i, 4); if (vt < 0x00400000 || vt >= 0x00800000) continue;
                        uintptr_t obj = (uintptr_t)mbi.BaseAddress + off + i;
                        if (!ReadProcessMemory(p, (LPCVOID)(obj + 0x418), &type, 4, &n) || n != 4 || !type ||
                            !ReadProcessMemory(p, (LPCVOID)type, &type_vt, 4, &n) || n != 4 || type_vt != kTypeVtable ||
                            !ReadProcessMemory(p, (LPCVOID)(type + 0x24), id, 23, &n) || n != 23 || !valid_id(id) ||
                            !is_civilian_id(id)) continue;
                        BOOL dup = FALSE; for (int j = 0; j < count; ++j) if (list[j].obj == obj) { dup = TRUE; break; }
                        if (!dup && count < capacity) list[count++].obj = obj;
                    }
                }
                off += want;
            }
        }
        cur = (uintptr_t)mbi.BaseAddress + mbi.RegionSize; if (!cur) break;
    }
    return count;
}

static int repair_pass(HANDLE p, DWORD player, Candidate *list, int count) {
    int changed = 0;
    for (int i = 0; !g_stop && i < count; ++i) {
        uintptr_t obj = list[i].obj; DWORD type = 0, type_vt = 0, owner = 0; LONG hp = 0, maxhp = 0;
        char id[24] = {0}; SIZE_T n = 0;
        if (!ReadProcessMemory(p, (LPCVOID)(obj + 0x418), &type, 4, &n) || n != 4 || !type ||
            !ReadProcessMemory(p, (LPCVOID)type, &type_vt, 4, &n) || n != 4 || type_vt != kTypeVtable ||
            !ReadProcessMemory(p, (LPCVOID)(type + 0x24), id, 23, &n) || n != 23 || !valid_id(id) || !is_civilian_id(id) ||
            !ReadProcessMemory(p, (LPCVOID)(obj + 0x1B4), &owner, 4, &n) || owner != player ||
            !ReadProcessMemory(p, (LPCVOID)(obj + 0x6C), &hp, 4, &n) ||
            !ReadProcessMemory(p, (LPCVOID)(type + 0xA0), &maxhp, 4, &n) || hp <= 0 || maxhp <= 0 || hp >= maxhp) continue;
        BYTE repair = 0;
        if (!ReadProcessMemory(p, (LPCVOID)(obj + 0x5B8), &repair, 1, &n) || repair) continue;
        BYTE one = 1; SIZE_T w = 0;
        if (WriteProcessMemory(p, (LPVOID)(obj + 0x5B8), &one, 1, &w) && w == 1) {
            if ((changed++ % 8) == 0) wprintf(L"GARRISON_REPAIR_FLAG %-10S %ld/%ld obj=0x%08lX\n", id, (long)hp, (long)maxhp, (unsigned long)obj);
        }
    }
    return changed;
}

int wmain(int argc, wchar_t **argv) {
    if (argc != 1) {
        wprintf(L"Usage: ra2_product_garrison_repair.exe\n");
        return 2;
    }
    if (!version_ok()) { wprintf(L"VERSION_MISMATCH: service stopped.\n"); return 2; }
    SetConsoleCtrlHandler(handler, TRUE);
    wprintf(L"GARRISON_CIVILIAN_REPAIR_SERVICE running; Ctrl+C to stop.\n");
    Candidate list[1024]; int count = 0; DWORD last_pid = 0, last_refresh = 0; int missing_ticks = 0;
    g_stop_event = CreateEventW(NULL, TRUE, FALSE, L"Local\\RA2ProductGarrisonRepairStop");
    while (!g_stop && (!g_stop_event || WaitForSingleObject(g_stop_event, 0) != WAIT_OBJECT_0)) {
        DWORD pid = find_pid();
        if (!pid) {
            if (last_pid && ++missing_ticks >= 10) break;
            Sleep(100);
            continue;
        }
        missing_ticks = 0;
        if (pid) {
            HANDLE p = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, pid);
            if (p) {
                DWORD player = 0, now = GetTickCount(); SIZE_T n = 0;
                if (pid != last_pid) { count = 0; last_refresh = 0; last_pid = pid; }
                if (ReadProcessMemory(p, (LPCVOID)0x00A35DB4, &player, 4, &n) && n == 4 && player) {
                    if (!last_refresh || now - last_refresh >= 1000) { count = refresh_candidates(p, list, ARRAYSIZE(list)); last_refresh = GetTickCount(); }
                    repair_pass(p, player, list, count);
                }
                CloseHandle(p);
            }
        }
        Sleep(100);
    }
    if (g_stop_event) CloseHandle(g_stop_event);
    wprintf(L"GARRISON_CIVILIAN_REPAIR_SERVICE stopped.\n"); return 0;
}

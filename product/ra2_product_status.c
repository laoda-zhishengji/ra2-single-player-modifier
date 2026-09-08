#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include "ra2_product_module.h"
#include "ra2_product_manifest.h"

#pragma comment(lib, "bcrypt.lib")


static DWORD find_pid(void) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W e = { sizeof(e) };
    DWORD result = 0;
    if (s == INVALID_HANDLE_VALUE) return 0;
    if (Process32FirstW(s, &e)) do {
        if (!_wcsicmp(e.szExeFile, L"game.exe")) {
            HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID);
            wchar_t path[MAX_PATH]; DWORD n = ARRAYSIZE(path);
            if (p && QueryFullProcessImageNameW(p, 0, path, &n) && !_wcsicmp(path, RA2_PRODUCT_GAME_PATH)) result = e.th32ProcessID;
            if (p) CloseHandle(p);
        }
    } while (!result && Process32NextW(s, &e));
    CloseHandle(s);
    return result;
}

static BOOL hash_file(wchar_t *out, DWORD cap) {
    HANDLE f = INVALID_HANDLE_VALUE; BCRYPT_ALG_HANDLE a = NULL; BCRYPT_HASH_HANDLE h = NULL;
    PUCHAR obj = NULL, dig = NULL; DWORD olen = 0, dlen = 0, cb = 0; BYTE buf[8192]; ULONG got;
    BOOL ok = FALSE;
    if (cap < 65) return FALSE;
    f = CreateFileW(RA2_PRODUCT_GAME_PATH, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE || BCryptOpenAlgorithmProvider(&a, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0) goto done;
    if (BCryptGetProperty(a, BCRYPT_OBJECT_LENGTH, (PUCHAR)&olen, sizeof(olen), &cb, 0) < 0 ||
        BCryptGetProperty(a, BCRYPT_HASH_LENGTH, (PUCHAR)&dlen, sizeof(dlen), &cb, 0) < 0) goto done;
    obj = HeapAlloc(GetProcessHeap(), 0, olen); dig = HeapAlloc(GetProcessHeap(), 0, dlen);
    if (!obj || !dig || BCryptCreateHash(a, &h, obj, olen, NULL, 0, 0) < 0) goto done;
    for (;;) { if (!ReadFile(f, buf, sizeof(buf), &got, NULL)) goto done; if (!got) break; if (BCryptHashData(h, buf, got, 0) < 0) goto done; }
    if (BCryptFinishHash(h, dig, dlen, 0) < 0) goto done;
    for (DWORD i = 0; i < dlen; ++i) swprintf_s(out + i * 2, cap - i * 2, L"%02X", dig[i]);
    out[dlen * 2] = 0; ok = TRUE;
done:
    if (h) BCryptDestroyHash(h); if (a) BCryptCloseAlgorithmProvider(a, 0);
    if (obj) HeapFree(GetProcessHeap(), 0, obj); if (dig) HeapFree(GetProcessHeap(), 0, dig);
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    return ok;
}

static BOOL read_mem(HANDLE p, uintptr_t address, void *out, SIZE_T size) {
    SIZE_T got = 0;
    return ReadProcessMemory(p, (const void *)address, out, size, &got) && got == size;
}

static const wchar_t *patch_state(BYTE a, BYTE b, BYTE oa, BYTE ob, BYTE pa, BYTE pb) {
    if (a == oa && b == ob) return L"UNAPPLIED";
    if (a == pa && b == pb) return L"APPLIED";
    return L"UNKNOWN";
}

static BOOL process_present(const wchar_t *name, DWORD *pid_out) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W e = { sizeof(e) };
    BOOL found = FALSE;
    if (s == INVALID_HANDLE_VALUE) return FALSE;
    if (Process32FirstW(s, &e)) do {
        if (!_wcsicmp(e.szExeFile, name)) {
            if (pid_out) *pid_out = e.th32ProcessID;
            found = TRUE;
            break;
        }
    } while (Process32NextW(s, &e));
    CloseHandle(s);
    return found;
}

static void print_service_status(void) {
    DWORD pid = 0;
    wprintf(L"auto_repair=%ls", process_present(L"ra2_product_auto_repair.exe", &pid) ? L"ON" : L"OFF");
    if (pid) wprintf(L" pid=%lu", pid);
    wprintf(L"\n");
    pid = 0;
    wprintf(L"garrison_repair=%ls", process_present(L"ra2_product_garrison_repair.exe", &pid) ? L"ON" : L"OFF");
    if (pid) wprintf(L" pid=%lu", pid);
    wprintf(L"\n");
}

int wmain(void) {
    wchar_t sha[65]; DWORD pid = find_pid();
    Ra2ModuleRegistry registry;
    ra2_registry_init(&registry);
    wprintf(L"PRODUCT_STATUS\n");
    wprintf(L"module_registry=READY count=%d\n", RA2_MODULE_MAX);
    print_service_status();
    if (!pid) { wprintf(L"game=NOT_FOUND\n"); return 2; }
    wprintf(L"game=FOUND pid=%lu\n", pid);
    if (!hash_file(sha, ARRAYSIZE(sha))) { wprintf(L"version=HASH_FAILED\n"); return 3; }
    wprintf(L"sha256=%ls\n", sha);
    if (_wcsicmp(sha, RA2_PRODUCT_SHA256)) { wprintf(L"version=MISMATCH\nall_features=BLOCKED\n"); return 4; }
    wprintf(L"version=MATCHED\n");
    HANDLE p = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!p) { wprintf(L"memory=READ_HANDLE_FAILED\n"); return 5; }

    BYTE b[2];
    if (read_mem(p, RA2_PRODUCT_MONEY_PATCH, b, sizeof(b))) wprintf(L"money=%ls bytes=%02X%02X\n", patch_state(b[0],b[1],0x2B,0xC7,0x90,0x90), b[0], b[1]);
    else wprintf(L"money=READ_FAILED\n");
    if (read_mem(p, RA2_PRODUCT_POWER_PATCH, b, sizeof(b))) wprintf(L"power=%ls bytes=%02X%02X\n", patch_state(b[0],b[1],0x03,0xC8,0x90,0x90), b[0], b[1]);
    else wprintf(L"power=READ_FAILED\n");

    DWORD root = 0, value = 0; const DWORD offs[] = {0x52B8,0x52BC,0x52C0,0x52C4,0x52C8};
    if (read_mem(p, RA2_PRODUCT_PLAYER_ROOT, &root, sizeof(root)) && root) {
        wprintf(L"production=RUNTIME_FIELDS root=0x%08lX values=", root);
        for (int i = 0; i < 5; ++i) { value = 0; if (read_mem(p, root + offs[i], &value, sizeof(value))) wprintf(L"%lu%s", value, i == 4 ? L"" : L","); else wprintf(L"?%s", i == 4 ? L"" : L","); }
        wprintf(L"\n");
    } else wprintf(L"production=NOT_IN_TASK\n");
    DWORD super_array = 0;
    if (read_mem(p, root + RA2_PRODUCT_SUPER_ARRAY_OFFSET, &super_array, sizeof(super_array)) && super_array) {
        wprintf(L"superweapons=VALID_OBJECTS");
        for (DWORD i = 0; i < 8; ++i) {
            DWORD obj = 0; BYTE ready = 0, wait = 0;
            if (!read_mem(p, super_array + i * 4, &obj, sizeof(obj)) || (obj & 3u) || obj < 0x10000u || obj > 0x7F000000u) continue;
            if (!read_mem(p, obj + 0x57, &ready, sizeof(ready)) || !read_mem(p, obj + 0x58, &wait, sizeof(wait))) continue;
            wprintf(L" id%lu=0x%08lX ready=%u wait=%u", i, obj, ready, wait);
        }
        wprintf(L"\n");
    } else wprintf(L"superweapons=NOT_IN_TASK\n");
    wprintf(L"writes=NONE\n");
    CloseHandle(p);
    return 0;
}

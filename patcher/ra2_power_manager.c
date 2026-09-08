#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>

#pragma comment(lib, "bcrypt.lib")

static const wchar_t kPath[] = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static const wchar_t kSha[] = L"73288C03B58D370BE268CA6D156B4E33BFDB2066DC980359467D8852FF3B00DF";
static const uintptr_t kAddress = 0x004F2D99;
static const BYTE kOriginal[] = {0x03, 0xC8};
static const BYTE kPatched[] = {0x90, 0x90};

static DWORD find_pid(void) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W e = {0}; DWORD result = 0;
    if (s == INVALID_HANDLE_VALUE) return 0;
    e.dwSize = sizeof(e);
    if (Process32FirstW(s, &e)) do {
        if (_wcsicmp(e.szExeFile, L"game.exe") == 0) {
            HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID);
            wchar_t path[MAX_PATH]; DWORD n = ARRAYSIZE(path);
            if (p && QueryFullProcessImageNameW(p, 0, path, &n) && _wcsicmp(path, kPath) == 0) result = e.th32ProcessID;
            if (p) CloseHandle(p);
        }
    } while (!result && Process32NextW(s, &e));
    CloseHandle(s); return result;
}

static BOOL hash_file(wchar_t *out, DWORD count) {
    HANDLE f = INVALID_HANDLE_VALUE; BCRYPT_ALG_HANDLE a = NULL; BCRYPT_HASH_HANDLE h = NULL;
    PUCHAR obj = NULL, digest = NULL; DWORD olen = 0, dlen = 0, cb = 0; BYTE buf[8192]; ULONG got;
    BOOL ok = FALSE; if (count < 65) return FALSE;
    f = CreateFileW(kPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE || BCryptOpenAlgorithmProvider(&a, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0) goto done;
    if (BCryptGetProperty(a, BCRYPT_OBJECT_LENGTH, (PUCHAR)&olen, sizeof(olen), &cb, 0) < 0 ||
        BCryptGetProperty(a, BCRYPT_HASH_LENGTH, (PUCHAR)&dlen, sizeof(dlen), &cb, 0) < 0) goto done;
    obj = HeapAlloc(GetProcessHeap(), 0, olen); digest = HeapAlloc(GetProcessHeap(), 0, dlen);
    if (!obj || !digest || BCryptCreateHash(a, &h, obj, olen, NULL, 0, 0) < 0) goto done;
    for (;;) { if (!ReadFile(f, buf, sizeof(buf), &got, NULL)) goto done; if (!got) break; if (BCryptHashData(h, buf, got, 0) < 0) goto done; }
    if (BCryptFinishHash(h, digest, dlen, 0) < 0) goto done;
    for (DWORD i = 0; i < dlen; ++i) swprintf_s(out + i * 2, count - i * 2, L"%02X", digest[i]);
    out[dlen * 2] = 0; ok = TRUE;
done:
    if (h) BCryptDestroyHash(h); if (a) BCryptCloseAlgorithmProvider(a, 0);
    if (obj) HeapFree(GetProcessHeap(), 0, obj); if (digest) HeapFree(GetProcessHeap(), 0, digest);
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f); return ok;
}

int wmain(int argc, wchar_t **argv) {
    BOOL apply = argc == 2 && !_wcsicmp(argv[1], L"--apply");
    BOOL restore = argc == 2 && !_wcsicmp(argv[1], L"--restore");
    if (argc > 1 && !apply && !restore) { wprintf(L"Usage: ra2_power_manager.exe [--apply|--restore]\n"); return 2; }
    DWORD pid = find_pid(); wchar_t sha[65];
    if (!pid || !hash_file(sha, ARRAYSIZE(sha))) { wprintf(L"Target or executable hash unavailable.\n"); return 3; }
    wprintf(L"pid=%lu sha256=%ls\n", pid, sha);
    if (_wcsicmp(sha, kSha)) { wprintf(L"VERSION_MISMATCH: refusing all memory writes.\n"); return 4; }
    DWORD access = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | ((apply || restore) ? (PROCESS_VM_OPERATION | PROCESS_VM_WRITE) : 0);
    HANDLE p = OpenProcess(access, FALSE, pid); if (!p) return 5;
    BYTE current[2]; SIZE_T n = 0;
    if (!ReadProcessMemory(p, (void *)kAddress, current, sizeof(current), &n) || n != sizeof(current)) { CloseHandle(p); return 6; }
    wprintf(L"module+0xF2D99: %02X %02X\n", current[0], current[1]);
    if (apply || restore) {
        const BYTE *expected = apply ? kOriginal : kPatched; const BYTE *replacement = apply ? kPatched : kOriginal;
        if (memcmp(current, expected, 2) != 0) { wprintf(L"PATCH_REFUSED: unexpected bytes.\n"); CloseHandle(p); return 7; }
        SIZE_T written = 0;
        if (!WriteProcessMemory(p, (void *)kAddress, replacement, 2, &written) || written != 2 || !FlushInstructionCache(p, (void *)kAddress, 2)) { CloseHandle(p); return 8; }
        wprintf(L"PATCH_STATE=%ls\n", apply ? L"APPLIED" : L"UNAPPLIED");
    } else {
        wprintf(L"PATCH_STATE=%ls\nNo memory was written.\n", memcmp(current, kPatched, 2) == 0 ? L"APPLIED" : L"UNAPPLIED");
    }
    CloseHandle(p); return 0;
}

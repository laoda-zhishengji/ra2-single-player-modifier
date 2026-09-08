#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>

#pragma comment(lib, "bcrypt.lib")

static const wchar_t kPath[] = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static const wchar_t kSha[] = L"73288C03B58D370BE268CA6D156B4E33BFDB2066DC980359467D8852FF3B00DF";

typedef struct { uintptr_t address; const wchar_t *name; } Probe;

static const Probe kProbes[] = {
    {0x004F9961, L"infantry multiplier read"},
    {0x004F9980, L"unit multiplier read"},
    {0x004F999F, L"aircraft multiplier read"},
    {0x004F99DD, L"defense multiplier read"},
    {0x0064A6C2, L"buildup time read"},
    {0x0064AF97, L"build speed read"}
};

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

int wmain(void) {
    DWORD pid = find_pid(); wchar_t sha[65];
    if (!pid || !hash_file(sha, ARRAYSIZE(sha))) { wprintf(L"Target or executable hash unavailable.\n"); return 3; }
    wprintf(L"pid=%lu sha256=%ls\n", pid, sha);
    if (_wcsicmp(sha, kSha)) { wprintf(L"VERSION_MISMATCH: read-only probe stopped.\n"); return 4; }
    HANDLE p = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!p) return 5;
    for (size_t i = 0; i < ARRAYSIZE(kProbes); ++i) {
        BYTE b[8] = {0}; SIZE_T n = 0;
        if (!ReadProcessMemory(p, (void *)kProbes[i].address, b, sizeof(b), &n) || n != sizeof(b)) {
            wprintf(L"%ls @ module+0x%lX: READ_FAILED\n", kProbes[i].name, (unsigned long)(kProbes[i].address - 0x00400000));
            continue;
        }
        wprintf(L"%ls @ module+0x%lX: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                kProbes[i].name, (unsigned long)(kProbes[i].address - 0x00400000),
                b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
    }
    wprintf(L"READ_ONLY=TRUE\n");
    CloseHandle(p); return 0;
}

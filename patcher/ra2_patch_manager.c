#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>

#pragma comment(lib, "bcrypt.lib")

static const wchar_t kGamePath[] = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";
static const wchar_t kExpectedSha256[] = L"73288C03B58D370BE268CA6D156B4E33BFDB2066DC980359467D8852FF3B00DF";
static const uintptr_t kPatchAddress = 0x004E53A9;
static const BYTE kOriginalBytes[] = {0x2B, 0xC7};
static const BYTE kPatchedBytes[] = {0x90, 0x90};

static void print_error(const wchar_t *what) {
    wprintf(L"%ls failed: %lu\n", what, GetLastError());
}

static DWORD find_game_pid(void) {
    DWORD result = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W entry;
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    ZeroMemory(&entry, sizeof(entry));
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"game.exe") == 0) {
                HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
                wchar_t path[MAX_PATH];
                DWORD length = ARRAYSIZE(path);
                if (process && QueryFullProcessImageNameW(process, 0, path, &length) &&
                    _wcsicmp(path, kGamePath) == 0) {
                    result = entry.th32ProcessID;
                    CloseHandle(process);
                    break;
                }
                if (process) CloseHandle(process);
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}

static BOOL sha256_file(const wchar_t *path, wchar_t *hex, DWORD hex_count) {
    BOOL ok = FALSE;
    HANDLE file = INVALID_HANDLE_VALUE;
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    PUCHAR object = NULL;
    PUCHAR digest = NULL;
    DWORD object_size = 0, digest_size = 0, cb = 0;
    BYTE buffer[8192];
    ULONG read_count;

    if (hex_count < 65) return FALSE;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) goto cleanup;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0) goto cleanup;
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, (PUCHAR)&object_size, sizeof(object_size), &cb, 0) < 0) goto cleanup;
    if (BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, (PUCHAR)&digest_size, sizeof(digest_size), &cb, 0) < 0) goto cleanup;
    object = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, object_size);
    digest = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, digest_size);
    if (!object || !digest) goto cleanup;
    if (BCryptCreateHash(algorithm, &hash, object, object_size, NULL, 0, 0) < 0) goto cleanup;
    for (;;) {
        if (!ReadFile(file, buffer, sizeof(buffer), &read_count, NULL)) goto cleanup;
        if (read_count == 0) break;
        if (BCryptHashData(hash, buffer, read_count, 0) < 0) goto cleanup;
    }
    if (BCryptFinishHash(hash, digest, digest_size, 0) < 0) goto cleanup;
    for (DWORD i = 0; i < digest_size; ++i) swprintf_s(hex + i * 2, hex_count - i * 2, L"%02X", digest[i]);
    hex[digest_size * 2] = L'\0';
    ok = TRUE;

cleanup:
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    if (object) HeapFree(GetProcessHeap(), 0, object);
    if (digest) HeapFree(GetProcessHeap(), 0, digest);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    return ok;
}

static BOOL read_bytes(HANDLE process, uintptr_t address, BYTE *bytes, DWORD count) {
    SIZE_T got = 0;
    return ReadProcessMemory(process, (LPCVOID)address, bytes, count, &got) && got == count;
}

static void print_bytes(HANDLE process, uintptr_t address, DWORD count) {
    BYTE bytes[32];
    if (count > sizeof(bytes) || !read_bytes(process, address, bytes, count)) {
        print_error(L"ReadProcessMemory");
        return;
    }
    wprintf(L"module+0x%X:", (unsigned)(address - 0x00400000));
    for (DWORD i = 0; i < count; ++i) wprintf(L" %02X", bytes[i]);
    wprintf(L"\n");
}

int wmain(int argc, wchar_t **argv) {
    BOOL apply = argc == 2 && _wcsicmp(argv[1], L"--apply") == 0;
    BOOL restore = argc == 2 && _wcsicmp(argv[1], L"--restore") == 0;
    if (argc > 1 && !apply && !restore) {
        wprintf(L"Usage: ra2_patch_manager.exe [--apply|--restore]\n");
        return 2;
    }
    wchar_t actual_sha[65];
    DWORD pid = find_game_pid();
    if (!pid) {
        wprintf(L"No matching Steam Red Alert II game.exe found.\n");
        return 2;
    }
    if (!sha256_file(kGamePath, actual_sha, ARRAYSIZE(actual_sha))) {
        print_error(L"SHA-256");
        return 3;
    }
    wprintf(L"pid=%lu\nsha256=%ls\n", pid, actual_sha);
    if (_wcsicmp(actual_sha, kExpectedSha256) != 0) {
        wprintf(L"VERSION_MISMATCH: refusing all memory writes.\n");
        return 4;
    }

    DWORD access = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;
    if (apply || restore) access |= PROCESS_VM_OPERATION | PROCESS_VM_WRITE;
    HANDLE process = OpenProcess(access, FALSE, pid);
    if (!process) {
        print_error(L"OpenProcess");
        return 5;
    }
    BYTE current[sizeof(kOriginalBytes)];
    if (!read_bytes(process, kPatchAddress, current, sizeof(current))) {
        print_error(L"ReadProcessMemory patch byte");
        CloseHandle(process);
        return 6;
    }
    wprintf(L"VERSION_OK: patch bytes at module+0x%X are %02X %02X.\n",
            (unsigned)(kPatchAddress - 0x00400000), current[0], current[1]);
    if (apply) {
        if (memcmp(current, kPatchedBytes, sizeof(current)) == 0) {
            wprintf(L"PATCH_STATE=APPLIED\n");
        } else if (memcmp(current, kOriginalBytes, sizeof(current)) != 0) {
            wprintf(L"PATCH_REFUSED: unexpected original bytes %02X %02X.\n", current[0], current[1]);
            CloseHandle(process);
            return 7;
        } else {
            BYTE value[sizeof(kPatchedBytes)];
            memcpy(value, kPatchedBytes, sizeof(value));
            SIZE_T written = 0;
            if (!WriteProcessMemory(process, (LPVOID)kPatchAddress, value, sizeof(value), &written) || written != sizeof(value) ||
                !FlushInstructionCache(process, (LPCVOID)kPatchAddress, sizeof(value))) {
                print_error(L"WriteProcessMemory");
                CloseHandle(process);
                return 8;
            }
            wprintf(L"PATCH_STATE=APPLIED\n");
        }
    } else if (restore) {
        if (memcmp(current, kOriginalBytes, sizeof(current)) == 0) {
            wprintf(L"PATCH_STATE=UNAPPLIED\n");
        } else if (memcmp(current, kPatchedBytes, sizeof(current)) != 0) {
            wprintf(L"RESTORE_REFUSED: unexpected current bytes %02X %02X.\n", current[0], current[1]);
            CloseHandle(process);
            return 9;
        } else {
            BYTE value[sizeof(kOriginalBytes)];
            memcpy(value, kOriginalBytes, sizeof(value));
            SIZE_T written = 0;
            if (!WriteProcessMemory(process, (LPVOID)kPatchAddress, value, sizeof(value), &written) || written != sizeof(value) ||
                !FlushInstructionCache(process, (LPCVOID)kPatchAddress, sizeof(value))) {
                print_error(L"WriteProcessMemory");
                CloseHandle(process);
                return 10;
            }
            wprintf(L"PATCH_STATE=UNAPPLIED\n");
        }
    } else {
        wprintf(L"PATCH_STATE=%ls\n", memcmp(current, kPatchedBytes, sizeof(current)) == 0 ? L"APPLIED" : L"UNAPPLIED");
        wprintf(L"No memory was written.\n");
    }
    print_bytes(process, kPatchAddress, 14);
    CloseHandle(process);
    return 0;
}

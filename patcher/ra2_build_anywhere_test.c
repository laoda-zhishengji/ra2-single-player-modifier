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

/*
 * Steam RA2 1.006 baseline only.
 * This test changes one player placement call site, not the shared
 * can-deploy function and not any BuildingTypeClass data.
 */
static const wchar_t *kPath = RA2_PRODUCT_GAME_PATH;
static const wchar_t *kSha256 = RA2_PRODUCT_SHA256;

static const uintptr_t kCall = 0x0049BBC9;
static const BYTE kOriginal[5] = {0xE8, 0x02, 0xD6, 0xFF, 0xFF};
static const uintptr_t kCall2 = 0x004997A0;
static const BYTE kOriginal2[5] = {0xE8, 0x2B, 0xFA, 0xFF, 0xFF};
static const BYTE kStub[8] = {0xB8, 0x01, 0x00, 0x00, 0x00, 0xC2, 0x10, 0x00};
static const uintptr_t kSharedCheck = 0x004991D0;
static const BYTE kSharedOriginal[8] =
    {0xA1, 0xB4, 0x5D, 0xA3, 0x00, 0x8B, 0x4C, 0x24};

static DWORD find_pid(void) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W entry = {0};
    DWORD result = 0;
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"game.exe") == 0) {
                HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                              FALSE, entry.th32ProcessID);
                wchar_t image_path[MAX_PATH];
                DWORD length = ARRAYSIZE(image_path);
                if (process && QueryFullProcessImageNameW(process, 0,
                                                           image_path, &length) &&
                    _wcsicmp(image_path, kPath) == 0) {
                    result = entry.th32ProcessID;
                }
                if (process) CloseHandle(process);
            }
        } while (!result && Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}

static BOOL hash_file(wchar_t *out, DWORD capacity) {
    HANDLE file = INVALID_HANDLE_VALUE;
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    PUCHAR object = NULL;
    PUCHAR digest = NULL;
    DWORD object_length = 0, digest_length = 0, returned = 0;
    BYTE buffer[8192];
    ULONG read_count = 0;
    BOOL ok = FALSE;

    if (capacity < 65) return FALSE;
    file = CreateFileW(kPath, GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) goto done;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                    NULL, 0) < 0) goto done;
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                          (PUCHAR)&object_length, sizeof(object_length),
                          &returned, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
                          (PUCHAR)&digest_length, sizeof(digest_length),
                          &returned, 0) < 0) goto done;
    object = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, object_length);
    digest = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, digest_length);
    if (!object || !digest || BCryptCreateHash(algorithm, &hash, object,
                                                 object_length, NULL, 0, 0) < 0) {
        goto done;
    }
    for (;;) {
        if (!ReadFile(file, buffer, sizeof(buffer), &read_count, NULL)) goto done;
        if (!read_count) break;
        if (BCryptHashData(hash, buffer, read_count, 0) < 0) goto done;
    }
    if (BCryptFinishHash(hash, digest, digest_length, 0) < 0) goto done;
    for (DWORD i = 0; i < digest_length; ++i) {
        swprintf_s(out + i * 2, capacity - i * 2, L"%02X", digest[i]);
    }
    out[digest_length * 2] = L'\0';
    ok = TRUE;

done:
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    if (object) HeapFree(GetProcessHeap(), 0, object);
    if (digest) HeapFree(GetProcessHeap(), 0, digest);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    return ok;
}

static BOOL read_bytes(HANDLE process, uintptr_t address, void *buffer,
                       SIZE_T size) {
    SIZE_T got = 0;
    return ReadProcessMemory(process, (LPCVOID)address, buffer, size, &got) &&
           got == size;
}

static BOOL write_bytes(HANDLE process, uintptr_t address, const void *buffer,
                        SIZE_T size) {
    SIZE_T written = 0;
    return WriteProcessMemory(process, (LPVOID)address, buffer, size,
                              &written) && written == size &&
           FlushInstructionCache(process, (LPCVOID)address, size);
}

static BOOL version_ok(void) {
    wchar_t actual[65];
    return hash_file(actual, ARRAYSIZE(actual)) &&
           _wcsicmp(actual, kSha256) == 0;
}

static int probe(HANDLE process) {
    BYTE current[5];
    if (!read_bytes(process, kCall, current, sizeof(current))) return 5;
    wprintf(L"placement_call=0x%p bytes=%02X %02X %02X %02X %02X\n",
            (void *)kCall, current[0], current[1], current[2], current[3],
            current[4]);
    if (memcmp(current, kOriginal, sizeof(kOriginal)) == 0) {
        wprintf(L"PATCH_STATE=UNAPPLIED\nSCOPE=PLAYER_PLACEMENT_CALL_ONLY\n");
        return 0;
    }
    if (current[0] == 0xE8) {
        int32_t relative = 0;
        memcpy(&relative, current + 1, sizeof(relative));
        uintptr_t cave = kCall + 5 + (intptr_t)relative;
        BYTE body[sizeof(kStub)];
        if (read_bytes(process, cave, body, sizeof(body)) &&
            memcmp(body, kStub, sizeof(kStub)) == 0) {
            wprintf(L"PATCH_STATE=APPLIED cave=0x%p\n", (void *)cave);
            return 0;
        }
    }
    wprintf(L"PATCH_STATE=UNKNOWN\n");
    return 6;
}

static int apply_patch(HANDLE process) {
    BYTE current[5];
    if (!read_bytes(process, kCall, current, sizeof(current))) return 5;
    if (memcmp(current, kOriginal, sizeof(kOriginal)) != 0) {
        wprintf(L"PATCH_REFUSED: placement call bytes are not the baseline.\n");
        return 6;
    }

    LPVOID cave = VirtualAllocEx(process, NULL, 0x1000,
                                 MEM_COMMIT | MEM_RESERVE,
                                 PAGE_EXECUTE_READWRITE);
    if (!cave) return 7;

    BYTE call_patch[5] = {0xE8, 0, 0, 0, 0};
    intptr_t relative = (intptr_t)(uintptr_t)cave - (intptr_t)(kCall + 5);
    if (relative < INT32_MIN || relative > INT32_MAX) {
        VirtualFreeEx(process, cave, 0, MEM_RELEASE);
        return 8;
    }
    memcpy(call_patch + 1, &relative, sizeof(int32_t));
    if (!write_bytes(process, (uintptr_t)cave, kStub, sizeof(kStub)) ||
        !write_bytes(process, kCall, call_patch, sizeof(call_patch))) {
        VirtualFreeEx(process, cave, 0, MEM_RELEASE);
        return 9;
    }
    wprintf(L"PATCH_STATE=APPLIED cave=0x%p\n"
            L"SCOPE=PLAYER_PLACEMENT_CALL_ONLY\n"
            L"PRESERVED=terrain occupancy water-land resource-tech AI\n",
            cave);
    return 0;
}

static int restore_patch(HANDLE process) {
    BYTE current[5];
    if (!read_bytes(process, kCall, current, sizeof(current))) return 5;
    if (memcmp(current, kOriginal, sizeof(kOriginal)) == 0) {
        wprintf(L"PATCH_STATE=UNAPPLIED\n");
        return 0;
    }
    if (current[0] != 0xE8) {
        wprintf(L"PATCH_REFUSED: placement call is neither baseline nor our patch.\n");
        return 6;
    }
    int32_t relative = 0;
    memcpy(&relative, current + 1, sizeof(relative));
    uintptr_t cave = kCall + 5 + (intptr_t)relative;
    BYTE body[sizeof(kStub)];
    if (!read_bytes(process, cave, body, sizeof(body)) ||
        memcmp(body, kStub, sizeof(kStub)) != 0) {
        wprintf(L"PATCH_REFUSED: call target is not our verified code cave.\n");
        return 7;
    }
    if (!write_bytes(process, kCall, kOriginal, sizeof(kOriginal))) return 8;
    VirtualFreeEx(process, (LPVOID)cave, 0, MEM_RELEASE);
    wprintf(L"PATCH_STATE=UNAPPLIED\n");
    return 0;
}

static int apply_call2(HANDLE process) {
    BYTE current[5];
    if (!read_bytes(process, kCall2, current, sizeof(current))) return 5;
    if (memcmp(current, kOriginal2, sizeof(kOriginal2)) != 0) {
        wprintf(L"PATCH_REFUSED: second placement call bytes are not the baseline.\n");
        return 6;
    }
    LPVOID cave = VirtualAllocEx(process, NULL, 0x1000,
                                 MEM_COMMIT | MEM_RESERVE,
                                 PAGE_EXECUTE_READWRITE);
    if (!cave) return 7;
    BYTE call_patch[5] = {0xE8, 0, 0, 0, 0};
    intptr_t relative = (intptr_t)(uintptr_t)cave - (intptr_t)(kCall2 + 5);
    if (relative < INT32_MIN || relative > INT32_MAX) {
        VirtualFreeEx(process, cave, 0, MEM_RELEASE);
        return 8;
    }
    memcpy(call_patch + 1, &relative, sizeof(int32_t));
    if (!write_bytes(process, (uintptr_t)cave, kStub, sizeof(kStub)) ||
        !write_bytes(process, kCall2, call_patch, sizeof(call_patch))) {
        VirtualFreeEx(process, cave, 0, MEM_RELEASE);
        return 9;
    }
    wprintf(L"DIAGNOSTIC_CALL2=APPLIED call=0x%p cave=0x%p\n"
            L"SCOPE=SECOND_PLACEMENT_CALL_ONLY\n",
            (void *)kCall2, cave);
    return 0;
}

static int restore_call2(HANDLE process) {
    BYTE current[5];
    if (!read_bytes(process, kCall2, current, sizeof(current))) return 5;
    if (memcmp(current, kOriginal2, sizeof(kOriginal2)) == 0) {
        wprintf(L"DIAGNOSTIC_CALL2=UNAPPLIED\n");
        return 0;
    }
    if (current[0] != 0xE8) return 6;
    int32_t relative = 0;
    memcpy(&relative, current + 1, sizeof(relative));
    uintptr_t cave = kCall2 + 5 + (intptr_t)relative;
    BYTE body[sizeof(kStub)];
    if (!read_bytes(process, cave, body, sizeof(body)) ||
        memcmp(body, kStub, sizeof(kStub)) != 0) return 7;
    if (!write_bytes(process, kCall2, kOriginal2, sizeof(kOriginal2))) return 8;
    VirtualFreeEx(process, (LPVOID)cave, 0, MEM_RELEASE);
    wprintf(L"DIAGNOSTIC_CALL2=UNAPPLIED\n");
    return 0;
}

static int apply_all(HANDLE process) {
    int first = apply_patch(process);
    if (first != 0) return first;
    int second = apply_call2(process);
    if (second != 0) {
        restore_patch(process);
        wprintf(L"TRANSACTION=ROLLED_BACK\n");
        return second;
    }
    wprintf(L"BUILD_ANYWHERE=APPLIED\n");
    return 0;
}

static int restore_all(HANDLE process) {
    int second = restore_call2(process);
    if (second != 0) return second;
    int first = restore_patch(process);
    if (first != 0) return first;
    wprintf(L"BUILD_ANYWHERE=UNAPPLIED\n");
    return 0;
}

/* Diagnostic only: this is the exact whole-function experiment described in
 * the reference article. It is deliberately separate from the player-only
 * call-site patch because it can affect every caller of 004991D0. */
static int apply_global_diagnostic(HANDLE process) {
    BYTE current[sizeof(kSharedOriginal)];
    if (!read_bytes(process, kSharedCheck, current, sizeof(current))) return 5;
    if (memcmp(current, kSharedOriginal, sizeof(current)) != 0) {
        wprintf(L"PATCH_REFUSED: shared check bytes are not the baseline.\n");
        return 6;
    }
    if (!write_bytes(process, kSharedCheck, kStub, sizeof(kStub))) return 7;
    wprintf(L"DIAGNOSTIC_GLOBAL=APPLIED address=0x%p\n"
            L"WARNING=affects all callers; single-player short test only\n",
            (void *)kSharedCheck);
    return 0;
}

static int restore_global_diagnostic(HANDLE process) {
    BYTE current[sizeof(kStub)];
    if (!read_bytes(process, kSharedCheck, current, sizeof(current))) return 5;
    if (memcmp(current, kSharedOriginal, sizeof(kSharedOriginal)) == 0) {
        wprintf(L"DIAGNOSTIC_GLOBAL=UNAPPLIED\n");
        return 0;
    }
    if (memcmp(current, kStub, sizeof(kStub)) != 0) {
        wprintf(L"PATCH_REFUSED: shared check is neither baseline nor our diagnostic patch.\n");
        return 6;
    }
    if (!write_bytes(process, kSharedCheck, kSharedOriginal,
                     sizeof(kSharedOriginal))) return 7;
    wprintf(L"DIAGNOSTIC_GLOBAL=UNAPPLIED\n");
    return 0;
}

int wmain(int argc, wchar_t **argv) {
    BOOL apply = argc == 2 && _wcsicmp(argv[1], L"--apply") == 0;
    BOOL restore = argc == 2 && _wcsicmp(argv[1], L"--restore") == 0;
    BOOL apply_global = argc == 2 && _wcsicmp(argv[1], L"--apply-global") == 0;
    BOOL restore_global = argc == 2 && _wcsicmp(argv[1], L"--restore-global") == 0;
    BOOL apply_call_site2 = argc == 2 && _wcsicmp(argv[1], L"--apply-call2") == 0;
    BOOL restore_call_site2 = argc == 2 && _wcsicmp(argv[1], L"--restore-call2") == 0;
    BOOL read_only = argc == 1 || (argc == 2 && _wcsicmp(argv[1], L"--probe") == 0);
    if (!apply && !restore && !apply_global && !restore_global &&
        !apply_call_site2 && !restore_call_site2 && !read_only) {
        wprintf(L"Usage: ra2_build_anywhere_test.exe [--probe|--apply|--restore|--apply-global|--restore-global|--apply-call2|--restore-call2]\n");
        return 2;
    }

    DWORD pid = find_pid();
    if (!pid) {
        wprintf(L"未找到匹配路径的 Steam game.exe。\n");
        return 3;
    }
    if (!version_ok()) {
        wprintf(L"VERSION_MISMATCH: refusing all process writes.\n");
        return 4;
    }
    DWORD access = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;
    if (apply || restore || apply_global || restore_global ||
        apply_call_site2 || restore_call_site2) {
        access |= PROCESS_VM_OPERATION | PROCESS_VM_WRITE;
    }
    HANDLE process = OpenProcess(access, FALSE, pid);
    if (!process) return 5;

    int result;
    if (read_only) result = probe(process);
    else if (apply) result = apply_all(process);
    else if (restore) result = restore_all(process);
    else if (apply_global) result = apply_global_diagnostic(process);
    else if (restore_global) result = restore_global_diagnostic(process);
    else if (apply_call_site2) result = apply_call2(process);
    else result = restore_call2(process);
    CloseHandle(process);
    return result;
}

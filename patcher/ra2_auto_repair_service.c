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

static const wchar_t kStopEvent[] = L"Local\\RA2ProductAutoRepairStop";
static const uintptr_t kKnownRepairSell = (uintptr_t)0x1DCC2104;
static volatile BOOL g_stop = FALSE;

static BOOL WINAPI handler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
        g_stop = TRUE;
        return TRUE;
    }
    return FALSE;
}

static DWORD find_pid(void) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W entry = { sizeof(entry) };
    DWORD result = 0;
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    if (Process32FirstW(snapshot, &entry)) do {
        if (!_wcsicmp(entry.szExeFile, L"game.exe")) {
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            wchar_t path[MAX_PATH];
            DWORD length = ARRAYSIZE(path);
            if (process && QueryFullProcessImageNameW(process, 0, path, &length) &&
                !_wcsicmp(path, RA2_PRODUCT_GAME_PATH)) result = entry.th32ProcessID;
            if (process) CloseHandle(process);
        }
    } while (!result && Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    return result;
}

static BOOL hash_ok(void) {
    HANDLE file = INVALID_HANDLE_VALUE;
    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    PUCHAR object = NULL, digest = NULL;
    DWORD object_length = 0, digest_length = 0, returned = 0;
    BYTE buffer[8192];
    ULONG read = 0;
    wchar_t output[65];
    BOOL ok = FALSE;

    file = CreateFileW(RA2_PRODUCT_GAME_PATH, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE ||
        BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0) < 0) goto done;
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, (PUCHAR)&object_length,
                          sizeof(object_length), &returned, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, (PUCHAR)&digest_length,
                          sizeof(digest_length), &returned, 0) < 0) goto done;
    object = HeapAlloc(GetProcessHeap(), 0, object_length);
    digest = HeapAlloc(GetProcessHeap(), 0, digest_length);
    if (!object || !digest || BCryptCreateHash(algorithm, &hash, object, object_length, NULL, 0, 0) < 0) goto done;
    for (;;) {
        if (!ReadFile(file, buffer, sizeof(buffer), &read, NULL)) goto done;
        if (!read) break;
        if (BCryptHashData(hash, buffer, read, 0) < 0) goto done;
    }
    if (BCryptFinishHash(hash, digest, digest_length, 0) < 0) goto done;
    for (DWORD i = 0; i < digest_length; ++i)
        swprintf_s(output + i * 2, ARRAYSIZE(output) - i * 2, L"%02X", digest[i]);
    output[64] = 0;
    ok = !_wcsicmp(output, RA2_PRODUCT_SHA256);

done:
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    if (object) HeapFree(GetProcessHeap(), 0, object);
    if (digest) HeapFree(GetProcessHeap(), 0, digest);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    return ok;
}

static BOOL writable(DWORD protect) {
    DWORD page = protect & 0xff;
    return page == PAGE_READWRITE || page == PAGE_WRITECOPY ||
           page == PAGE_EXECUTE_READWRITE || page == PAGE_EXECUTE_WRITECOPY;
}

/* RepairSell is the fifth integer in the tested 11-field IQ rules record. */
static BOOL read_repair_sell(HANDLE process, uintptr_t address, int *value, BOOL require_writable) {
    MEMORY_BASIC_INFORMATION info;
    int fields[11];
    SIZE_T got = 0;
    uintptr_t region_base;
    if (address < 16 ||
        VirtualQueryEx(process, (LPCVOID)(address - 16), &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD)) return FALSE;
    region_base = (uintptr_t)info.BaseAddress;
    if (info.RegionSize < sizeof(fields) || address < region_base || address - region_base < 16 ||
        address - region_base > info.RegionSize - (sizeof(fields) - 16) ||
        (require_writable && !writable(info.Protect)) ||
        !ReadProcessMemory(process, (LPCVOID)(address - 16), fields, sizeof(fields), &got) ||
        got != sizeof(fields)) return FALSE;
    if (fields[0] != 5 || fields[1] != 4 || fields[2] != 5 || fields[3] != 2 ||
        fields[4] < 0 || fields[4] > 5 || fields[5] != 2 || fields[6] != 2 ||
        fields[7] != 3 || fields[8] != 3 || fields[9] != 2 || fields[10] != 2) return FALSE;
    if (value) *value = fields[4];
    return TRUE;
}

/* Return a fallback only when the live task has exactly one writable match. */
static uintptr_t find_unique_repair_sell(HANDLE process, int *value, int *matches) {
    MEMORY_BASIC_INFORMATION info;
    uintptr_t cursor = 0, candidate = 0;
    BYTE buffer[65536];
    int candidate_value = -1, count = 0;
    while (VirtualQueryEx(process, (LPCVOID)cursor, &info, sizeof(info)) == sizeof(info)) {
        if (info.State == MEM_COMMIT && info.Type == MEM_PRIVATE &&
            writable(info.Protect) && !(info.Protect & PAGE_GUARD)) {
            SIZE_T offset = 0;
            while (offset < info.RegionSize) {
                SIZE_T wanted = info.RegionSize - offset;
                SIZE_T got = 0;
                if (wanted > sizeof(buffer)) wanted = sizeof(buffer);
                if (ReadProcessMemory(process, (BYTE *)info.BaseAddress + offset,
                                      buffer, wanted, &got) && got >= 44) {
                    for (SIZE_T i = 0; i + 44 <= got; i += 4) {
                        int fields[11];
                        memcpy(fields, buffer + i, sizeof(fields));
                        if (fields[0] == 5 && fields[1] == 4 && fields[2] == 5 && fields[3] == 2 &&
                            fields[4] >= 0 && fields[4] <= 5 && fields[5] == 2 && fields[6] == 2 &&
                            fields[7] == 3 && fields[8] == 3 && fields[9] == 2 && fields[10] == 2) {
                            uintptr_t found = (uintptr_t)info.BaseAddress + offset + i + 16;
                            if (found != candidate) {
                                candidate = found;
                                candidate_value = fields[4];
                                if (++count > 1) goto done;
                            }
                        }
                    }
                }
                if (wanted < 44) break;
                offset += wanted - 43;
            }
        }
        uintptr_t next = (uintptr_t)info.BaseAddress + info.RegionSize;
        if (next <= cursor) break;
        cursor = next;
    }

done:
    if (matches) *matches = count;
    if (count != 1) return 0;
    if (value) *value = candidate_value;
    return candidate;
}

static void restore_if_unchanged(HANDLE process, uintptr_t address, int original, BOOL wrote) {
    int current = -1;
    SIZE_T written = 0;
    if (process && wrote && read_repair_sell(process, address, &current, TRUE) && current == 0)
        WriteProcessMemory(process, (LPVOID)address, &original, sizeof(original), &written);
}

static BOOL read_task_root(HANDLE process, DWORD *root) {
    SIZE_T got = 0;
    *root = 0;
    return ReadProcessMemory(process, (LPCVOID)RA2_PRODUCT_PLAYER_ROOT, root, sizeof(*root), &got) &&
           got == sizeof(*root);
}

int wmain(int argc, wchar_t **argv) {
    (void)argv;
    if (argc != 1) {
        wprintf(L"Usage: ra2_product_auto_repair.exe\n");
        return 2;
    }
    if (!hash_ok()) {
        wprintf(L"VERSION_MISMATCH: service stopped.\n");
        return 2;
    }
    SetConsoleCtrlHandler(handler, TRUE);
    HANDLE stop = CreateEventW(NULL, TRUE, FALSE, kStopEvent);
    if (!stop) return 3;

    DWORD pid = 0, task_root = 0, last_scan = 0;
    uintptr_t address = 0;
    int original = 1;
    BOOL wrote = FALSE;
    HANDLE process = NULL;
    wprintf(L"AUTO_REPAIR_IQ_SERVICE running\n");

    while (!g_stop && WaitForSingleObject(stop, 100) == WAIT_TIMEOUT) {
        DWORD current_pid = find_pid();
        if (!current_pid) {
            restore_if_unchanged(process, address, original, wrote);
            address = 0; task_root = 0; wrote = FALSE; last_scan = 0;
            if (process) { CloseHandle(process); process = NULL; }
            pid = 0;
            continue;
        }
        if (current_pid != pid) {
            restore_if_unchanged(process, address, original, wrote);
            if (process) CloseHandle(process);
            process = NULL;
            pid = current_pid; address = 0; task_root = 0; wrote = FALSE; last_scan = 0;
        }
        if (!process) process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ |
            PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, pid);
        if (!process) continue;

        DWORD current_root = 0;
        if (!read_task_root(process, &current_root) || !current_root) {
            restore_if_unchanged(process, address, original, wrote);
            address = 0; task_root = 0; wrote = FALSE; last_scan = 0;
            Sleep(100);
            continue;
        }
        if (current_root != task_root) {
            restore_if_unchanged(process, address, original, wrote);
            address = 0; wrote = FALSE; original = 1; task_root = current_root; last_scan = 0;
            wprintf(L"AUTO_REPAIR_TASK_CHANGED root=0x%08lX\n", current_root);
        }

        int current_value = -1;
        if (address && !read_repair_sell(process, address, &current_value, TRUE)) {
            restore_if_unchanged(process, address, original, wrote);
            address = 0; wrote = FALSE; last_scan = 0;
        }
        if (!address) {
            int known_value = -1;
            if (read_repair_sell(process, kKnownRepairSell, &known_value, TRUE)) {
                address = kKnownRepairSell;
                original = known_value;
                wprintf(L"AUTO_REPAIR_IQ_TARGET source=known addr=0x%08lX old=%d\n",
                        (unsigned long)address, original);
            } else if (!last_scan || GetTickCount() - last_scan >= 1000) {
                int matches = 0, found_value = -1;
                uintptr_t found = find_unique_repair_sell(process, &found_value, &matches);
                last_scan = GetTickCount();
                if (found) {
                    address = found;
                    original = found_value;
                    wprintf(L"AUTO_REPAIR_IQ_TARGET source=scan addr=0x%08lX old=%d\n",
                            (unsigned long)address, original);
                } else if (matches > 1) {
                    wprintf(L"AUTO_REPAIR_IQ_AMBIGUOUS matches>1; no write performed\n");
                } else {
                    wprintf(L"AUTO_REPAIR_IQ_NOT_FOUND; retrying\n");
                }
            }
        }

        if (address && read_repair_sell(process, address, &current_value, TRUE) && current_value != 0) {
            int zero = 0;
            SIZE_T written = 0;
            if (WriteProcessMemory(process, (LPVOID)address, &zero, sizeof(zero), &written) &&
                written == sizeof(zero)) {
                wrote = TRUE;
                wprintf(L"AUTO_REPAIR_IQ_APPLIED addr=0x%08lX old=%d new=0\n",
                        (unsigned long)address, original);
            }
        }
    }

    restore_if_unchanged(process, address, original, wrote);
    if (process) CloseHandle(process);
    CloseHandle(stop);
    wprintf(L"AUTO_REPAIR_IQ_SERVICE stopped\n");
    return 0;
}

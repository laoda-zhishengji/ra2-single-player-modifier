#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static const wchar_t *kTargetPath =
    L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Command & Conquer Red Alert II\\game.exe";

static DWORD find_target(void)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W entry = {0};
    entry.dwSize = sizeof(entry);
    DWORD result = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"game.exe") == 0) {
                HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                              entry.th32ProcessID);
                if (process) {
                    wchar_t path[MAX_PATH];
                    DWORD size = MAX_PATH;
                    if (QueryFullProcessImageNameW(process, 0, path, &size) &&
                        _wcsicmp(path, kTargetPath) == 0) result = entry.th32ProcessID;
                    CloseHandle(process);
                }
            }
        } while (!result && Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}

int wmain(int argc, wchar_t **argv)
{
    if (argc != 2) {
        fwprintf(stderr, L"用法: ra2_money_scan.exe <整数资金值>\n");
        return 2;
    }
    wchar_t *end = NULL;
    long parsed = wcstol(argv[1], &end, 10);
    if (!end || *end || parsed < 0 || parsed > INT32_MAX) return 2;
    int32_t wanted = (int32_t)parsed;
    DWORD pid = find_target();
    if (!pid) return 3;

    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!process) return 4;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    unsigned char *address = (unsigned char *)si.lpMinimumApplicationAddress;
    unsigned char *maximum = (unsigned char *)si.lpMaximumApplicationAddress;
    MEMORY_BASIC_INFORMATION mbi;
    unsigned matches = 0;
    while (address < maximum && VirtualQueryEx(process, address, &mbi, sizeof(mbi)) == sizeof(mbi)) {
        DWORD protect = mbi.Protect & 0xff;
        BOOL writable = protect == PAGE_READWRITE || protect == PAGE_WRITECOPY ||
                        protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
        if (mbi.State == MEM_COMMIT && writable && !(mbi.Protect & PAGE_GUARD)) {
            SIZE_T region_size = mbi.RegionSize;
            unsigned char *buffer = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, region_size);
            if (buffer) {
                SIZE_T bytes_read = 0;
                if (ReadProcessMemory(process, mbi.BaseAddress, buffer, region_size, &bytes_read)) {
                    for (SIZE_T offset = 0; offset + sizeof(wanted) <= bytes_read; offset += 4) {
                        int32_t value;
                        memcpy(&value, buffer + offset, sizeof(value));
                        if (value == wanted) {
                            printf("0x%p region=0x%zx protect=0x%lx\n",
                                   (void *)((uintptr_t)mbi.BaseAddress + offset),
                                   (size_t)region_size, (unsigned long)mbi.Protect);
                            if (++matches >= 200) break;
                        }
                    }
                }
                HeapFree(GetProcessHeap(), 0, buffer);
            }
        }
        if (matches >= 200) break;
        address = (unsigned char *)mbi.BaseAddress + mbi.RegionSize;
    }
    CloseHandle(process);
    printf("matches=%u pid=%lu value=%ld\n", matches, pid, (long)wanted);
    return 0;
}

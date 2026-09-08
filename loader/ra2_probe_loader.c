#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

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
                HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
                if (process) {
                    wchar_t path[MAX_PATH];
                    DWORD size = MAX_PATH;
                    if (QueryFullProcessImageNameW(process, 0, path, &size) &&
                        _wcsicmp(path, kTargetPath) == 0) {
                        result = entry.th32ProcessID;
                    }
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
        fwprintf(stderr, L"用法: ra2_probe_loader.exe <ra2_probe.dll的完整路径>\n");
        return 2;
    }

    DWORD pid = find_target();
    if (!pid) {
        fwprintf(stderr, L"未找到匹配的 Steam《红警2》原版 game.exe。\n");
        return 3;
    }

    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                 PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                                 FALSE, pid);
    if (!process) {
        fwprintf(stderr, L"无法打开目标进程，错误码=%lu。\n", GetLastError());
        return 4;
    }

    SIZE_T bytes = (wcslen(argv[1]) + 1) * sizeof(wchar_t);
    void *remote_path = VirtualAllocEx(process, NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_path) {
        fwprintf(stderr, L"无法准备目标路径，错误码=%lu。\n", GetLastError());
        CloseHandle(process);
        return 5;
    }

    if (!WriteProcessMemory(process, remote_path, argv[1], bytes, NULL)) {
        fwprintf(stderr, L"无法写入目标路径，错误码=%lu。\n", GetLastError());
        VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        CloseHandle(process);
        return 6;
    }

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    FARPROC load_library = kernel32 ? GetProcAddress(kernel32, "LoadLibraryW") : NULL;
    if (!load_library) {
        fwprintf(stderr, L"无法定位 LoadLibraryW。\n");
        VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        CloseHandle(process);
        return 7;
    }

    HANDLE thread = CreateRemoteThread(process, NULL, 0,
                                       (LPTHREAD_START_ROUTINE)load_library,
                                       remote_path, 0, NULL);
    if (!thread) {
        fwprintf(stderr, L"无法启动加载线程，错误码=%lu。\n", GetLastError());
        VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
        CloseHandle(process);
        return 8;
    }

    WaitForSingleObject(thread, 5000);
    DWORD exit_code = 0;
    GetExitCodeThread(thread, &exit_code);
    CloseHandle(thread);
    VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
    CloseHandle(process);

    if (!exit_code) {
        fwprintf(stderr, L"DLL 加载失败。\n");
        return 9;
    }
    wprintf(L"DLL 已加载到 game.exe，PID=%lu。\n", pid);
    wprintf(L"请检查 %%TEMP%%\\ra2_modifier_probe.log。\n");
    return 0;
}

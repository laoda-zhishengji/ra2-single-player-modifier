#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <strsafe.h>

static void write_probe_log(const wchar_t *event_name)
{
    wchar_t temp_path[MAX_PATH];
    wchar_t log_path[MAX_PATH];
    DWORD length = GetTempPathW(MAX_PATH, temp_path);
    if (length == 0 || length >= MAX_PATH) return;

    if (FAILED(StringCchPrintfW(log_path, MAX_PATH,
                                L"%sra2_modifier_probe.log", temp_path))) {
        return;
    }

    HANDLE file = CreateFileW(log_path, FILE_APPEND_DATA,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;

    SYSTEMTIME now;
    GetLocalTime(&now);
    wchar_t line[256];
    int count = _snwprintf_s(line, 256, _TRUNCATE,
                             L"%04u-%02u-%02u %02u:%02u:%02u event=%s pid=%lu\r\n",
                             now.wYear, now.wMonth, now.wDay,
                             now.wHour, now.wMinute, now.wSecond,
                             event_name, GetCurrentProcessId());
    if (count > 0) {
        DWORD written = 0;
        WriteFile(file, line, (DWORD)(count * sizeof(wchar_t)), &written, NULL);
    }
    CloseHandle(file);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        OutputDebugStringW(L"[RA2 probe] DLL_PROCESS_ATTACH\n");
        write_probe_log(L"attach");
    } else if (reason == DLL_PROCESS_DETACH) {
        OutputDebugStringW(L"[RA2 probe] DLL_PROCESS_DETACH\n");
        write_probe_log(L"detach");
    }
    return TRUE;
}

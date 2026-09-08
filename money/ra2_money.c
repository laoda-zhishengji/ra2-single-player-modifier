#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <strsafe.h>

static volatile LONG g_running = 1;
static HANDLE g_thread = NULL;
static const uintptr_t kMoneyOffsetA = 0x004373CC;
static const uintptr_t kMoneyOffsetB = 0x004373D0;
static const LONG kMoneyValue = 999999;

static void log_line(const wchar_t *event_name)
{
    wchar_t temp[MAX_PATH], path[MAX_PATH], line[256];
    DWORD n = GetTempPathW(MAX_PATH, temp);
    if (!n || n >= MAX_PATH) return;
    if (FAILED(StringCchPrintfW(path, MAX_PATH, L"%sra2_modifier_money.log", temp))) return;
    HANDLE file = CreateFileW(path, FILE_APPEND_DATA,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME now;
    GetLocalTime(&now);
    int count = _snwprintf_s(line, 256, _TRUNCATE,
        L"%04u-%02u-%02u %02u:%02u:%02u event=%s pid=%lu value=%ld\r\n",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
        event_name, GetCurrentProcessId(), (long)kMoneyValue);
    if (count > 0) {
        DWORD written;
        WriteFile(file, line, (DWORD)(count * sizeof(wchar_t)), &written, NULL);
    }
    CloseHandle(file);
}

static DWORD WINAPI money_thread(void *unused)
{
    (void)unused;
    log_line(L"money_feature_started");
    HMODULE game = GetModuleHandleW(L"game.exe");
    if (!game) {
        log_line(L"game_module_missing");
        InterlockedExchange(&g_running, 0);
        return 1;
    }

    LONG *money_a = (LONG *)((uintptr_t)game + kMoneyOffsetA);
    LONG *money_b = (LONG *)((uintptr_t)game + kMoneyOffsetB);
    while (InterlockedCompareExchange(&g_running, 1, 1)) {
        InterlockedExchange(money_a, kMoneyValue);
        InterlockedExchange(money_b, kMoneyValue);
        Sleep(100);
    }
    log_line(L"money_feature_stopped");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        g_thread = CreateThread(NULL, 0, money_thread, NULL, 0, NULL);
        if (!g_thread) return FALSE;
    } else if (reason == DLL_PROCESS_DETACH) {
        InterlockedExchange(&g_running, 0);
        if (g_thread) CloseHandle(g_thread);
    }
    return TRUE;
}
